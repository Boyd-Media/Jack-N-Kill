#include "AI/PublicOrderAIController.h"

#include "Engine/World.h"
#include "Framework/PublicOrderPlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

APublicOrderAIController::APublicOrderAIController()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickInterval = 0.20f;
}

void APublicOrderAIController::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        CorruptionLevel = CorruptionSeed == INDEX_NONE
            ? FMath::FRand() : FRandomStream(CorruptionSeed).FRand();
        BribeCredits = FMath::Clamp(BribeCredits, 1, 100000);
        BribeTimeoutSeconds = FMath::IsFinite(BribeTimeoutSeconds)
            ? FMath::Clamp(BribeTimeoutSeconds, 1.0f, 120.0f) : 15.0f;
        ApproachDistanceCm = FMath::IsFinite(ApproachDistanceCm)
            ? FMath::Clamp(ApproachDistanceCm, 50.0f, 500.0f) : 200.0f;
        ForceRangeCm = FMath::IsFinite(ForceRangeCm)
            ? FMath::Max(ApproachDistanceCm, ForceRangeCm) : 1200.0f;
    }
}

void APublicOrderAIController::ReportCrime(FGuid CrimeId, APawn* Suspect,
    EPublicOrderSuspectKind Kind, bool bUseForceAllowed)
{
    if (!HasAuthority() || !GetPawn() || !IsValid(Suspect) || Suspect == GetPawn()
        || !CrimeId.IsValid() || RecentCrimeIds.Contains(CrimeId))
    {
        return;
    }
    const bool bCorrupt = CorruptionLevel >= 0.5f;
    if (bCorrupt && Kind == EPublicOrderSuspectKind::GangMember)
    {
        return;
    }
    // One encounter at a time. Other crime reports belong in a Director queue.
    if (State != EPublicOrderState::Patrol)
    {
        return;
    }
    if (RecentCrimeIds.Num() >= 32)
    {
        RecentCrimeIds.RemoveAt(0);
    }
    RecentCrimeIds.Add(CrimeId);
    CurrentSuspect = Suspect;
    bForceAllowed = !bCorrupt && bUseForceAllowed;
    State = EPublicOrderState::Pursuing;
    SetActorTickEnabled(true);
    NextMoveRetryAt = 0.0;
    SetFocus(Suspect);
}

void APublicOrderAIController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || State == EPublicOrderState::Patrol)
    {
        return;
    }
    APawn* Suspect = CurrentSuspect.Get();
    APawn* OfficerPawn = GetPawn();
    if (!IsValid(Suspect) || !IsValid(OfficerPawn))
    {
        ClearEncounter();
        return;
    }
    if (State == EPublicOrderState::AwaitingBribe)
    {
        // Leaving the encounter or changing possession is not payment.
        if (Suspect->GetController() != BribePlayer.Get()
            || FVector::DistSquared(Suspect->GetActorLocation(), OfficerPawn->GetActorLocation())
                > FMath::Square(ApproachDistanceCm * 3.0f))
        {
            BecomeHostile();
        }
        return;
    }
    if (State != EPublicOrderState::Pursuing)
    {
        return;
    }
    const float ActionRange = bForceAllowed ? ForceRangeCm : ApproachDistanceCm;
    if (FVector::DistSquared(Suspect->GetActorLocation(), OfficerPawn->GetActorLocation())
        <= FMath::Square(ActionRange))
    {
        StopMovement();
        if (CorruptionLevel >= 0.5f)
        {
            StartBribePrompt();
        }
        else if (bForceAllowed)
        {
            BecomeHostile();
        }
        else
        {
            State = EPublicOrderState::Arresting;
            OnArrestRequested.Broadcast(Suspect);
        }
    }
    else if (GetWorld()->GetTimeSeconds() >= NextMoveRetryAt)
    {
        // Moving targets are tracked by path following. Retry at a bounded rate.
        MoveToActor(Suspect, ActionRange * 0.75f, true);
        NextMoveRetryAt = GetWorld()->GetTimeSeconds() + 2.0;
    }
}

void APublicOrderAIController::StartBribePrompt()
{
    APawn* Suspect = CurrentSuspect.Get();
    APublicOrderPlayerController* Player = Suspect
        ? Cast<APublicOrderPlayerController>(Suspect->GetController()) : nullptr;
    if (!Player)
    {
        BecomeHostile();
        return;
    }
    State = EPublicOrderState::AwaitingBribe;
    BribePlayer = Player;
    ActiveBribeId = FGuid::NewGuid();
    BribeDeadline = GetWorld()->GetTimeSeconds() + BribeTimeoutSeconds;
    GetWorld()->GetTimerManager().SetTimer(BribeTimer, this,
        &APublicOrderAIController::OnBribeExpired, BribeTimeoutSeconds, false);
    if (!Player->BeginBribePrompt(this, ActiveBribeId, BribeCredits, BribeTimeoutSeconds))
    {
        BecomeHostile();
    }
}

void APublicOrderAIController::ResolveBribe(const FGuid& RequestId,
    APublicOrderPlayerController* Player, bool bAccept)
{
    APawn* Suspect = CurrentSuspect.Get();
    if (!HasAuthority() || State != EPublicOrderState::AwaitingBribe
        || !RequestId.IsValid() || RequestId != ActiveBribeId || Player != BribePlayer.Get()
        || !IsValid(Suspect) || Suspect->GetController() != Player)
    {
        return;
    }
    if (GetWorld()->GetTimeSeconds() >= BribeDeadline
        || !GetPawn() || FVector::DistSquared(Suspect->GetActorLocation(), GetPawn()->GetActorLocation())
            > FMath::Square(ApproachDistanceCm * 3.0f))
    {
        BecomeHostile();
        return;
    }
    // Consume before calling an overridable economy function: replay/reentrant
    // responses cannot charge twice. Economy implementation must be nonlatent.
    const FGuid ConsumedId = ActiveBribeId;
    ActiveBribeId.Invalidate();
    GetWorld()->GetTimerManager().ClearTimer(BribeTimer);
    const bool bPaid = bAccept && Player->TrySpendBribeCredits(BribeCredits);
    Player->CloseBribePrompt(ConsumedId, bPaid);
    BribePlayer.Reset();
    if (bPaid)
    {
        ClearEncounter();
    }
    else
    {
        BecomeHostile();
    }
}

void APublicOrderAIController::ClosePrompt(bool bPaid)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(BribeTimer);
    }
    const FGuid Id = ActiveBribeId;
    ActiveBribeId.Invalidate();
    if (Id.IsValid() && BribePlayer.IsValid())
    {
        BribePlayer->CloseBribePrompt(Id, bPaid);
    }
    BribePlayer.Reset();
}

void APublicOrderAIController::OnBribeExpired()
{
    if (State == EPublicOrderState::AwaitingBribe)
    {
        BecomeHostile();
    }
}

void APublicOrderAIController::BecomeHostile()
{
    ClosePrompt(false);
    State = EPublicOrderState::Hostile;
    if (CurrentSuspect.IsValid())
    {
        SetFocus(CurrentSuspect.Get());
        OnUseForceRequested.Broadcast(CurrentSuspect.Get());
    }
    else
    {
        ClearEncounter();
    }
}

void APublicOrderAIController::FinishArrest(bool bSucceeded)
{
    if (!HasAuthority() || State != EPublicOrderState::Arresting)
    {
        return;
    }
    if (bSucceeded)
    {
        ClearEncounter();
    }
    else
    {
        BecomeHostile();
    }
}

void APublicOrderAIController::ClearEncounter()
{
    if (!HasAuthority())
    {
        return;
    }
    APawn* PreviousSuspect = CurrentSuspect.Get();
    ClosePrompt(false);
    StopMovement();
    ClearFocus(EAIFocusPriority::Gameplay);
    CurrentSuspect.Reset();
    bForceAllowed = false;
    State = EPublicOrderState::Patrol;
    SetActorTickEnabled(false);
    if (IsValid(PreviousSuspect))
    {
        OnEnforcementEnded.Broadcast(PreviousSuspect);
    }
}

void APublicOrderAIController::OnUnPossess()
{
    ClearEncounter();
    Super::OnUnPossess();
}

void APublicOrderAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearEncounter();
    Super::EndPlay(EndPlayReason);
}
