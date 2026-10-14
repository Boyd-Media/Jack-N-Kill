#include "AI/StalkerAIController.h"
#include "AITypes.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"

namespace StalkerKeys
{
    const FName TargetActor(TEXT("TargetActor"));
    const FName HasSight(TEXT("HasSight"));
    const FName InvestigationLocation(TEXT("InvestigationLocation"));
    const FName HasInvestigation(TEXT("HasInvestigation"));
}

AStalkerAIController::AStalkerAIController()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.35f;
    StalkerPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("StalkerPerception"));
    SetPerceptionComponent(*StalkerPerception);
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    SightConfig->SightRadius = 2500.0f;
    SightConfig->LoseSightRadius = 3000.0f;
    SightConfig->PeripheralVisionAngleDegrees = 65.0f;
    SightConfig->SetMaxAge(2.0f);
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
    HearingConfig->HearingRange = 4500.0f;
    HearingConfig->SetMaxAge(4.0f);
    HearingConfig->DetectionByAffiliation = SightConfig->DetectionByAffiliation;
    StalkerPerception->ConfigureSense(*SightConfig);
    StalkerPerception->ConfigureSense(*HearingConfig);
    StalkerPerception->SetDominantSense(UAISense_Sight::StaticClass());
    StalkerPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AStalkerAIController::OnTargetUpdated);
}

void AStalkerAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (!HasAuthority())
    {
        return;
    }
    if (const ACharacter* Character = Cast<ACharacter>(InPawn))
    {
        BaseMovementSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
    }
    if (BehaviorTreeAsset)
    {
        RunBehaviorTree(BehaviorTreeAsset);
    }
    SetTrackedPlayer(UGameplayStatics::GetPlayerPawn(this, 0));
}

void AStalkerAIController::ResetKnowledge()
{
    if (ACharacter* Character = Cast<ACharacter>(GetPawn()))
    {
        Character->GetCharacterMovement()->MaxWalkSpeed = BaseMovementSpeed;
    }
    ClearFocus(EAIFocusPriority::Gameplay);
    InvestigationExpiresAt = 0.0;
    if (UBlackboardComponent* BB = GetBlackboardComponent())
    {
        BB->ClearValue(StalkerKeys::TargetActor);
        BB->SetValueAsBool(StalkerKeys::HasSight, false);
        BB->SetValueAsBool(StalkerKeys::HasInvestigation, false);
        BB->ClearValue(StalkerKeys::InvestigationLocation);
    }
}

void AStalkerAIController::SetTrackedPlayer(APawn* Player)
{
    TrackedPlayer = Player;
    ResetKnowledge();
    StalkerPerception->ForgetAll();
    StalkerPerception->RequestStimuliListenerUpdate();
    UpdateDistanceBudget();
}

void AStalkerAIController::OnUnPossess()
{
    if (ACharacter* Character = Cast<ACharacter>(GetPawn()))
    {
        Character->GetCharacterMovement()->MaxWalkSpeed = BaseMovementSpeed;
    }
    ResetKnowledge();
    TrackedPlayer.Reset();
    Super::OnUnPossess();
}

void AStalkerAIController::OnTargetUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    UBlackboardComponent* BB = GetBlackboardComponent();
    if (!HasAuthority() || !BB || Actor != TrackedPlayer.Get() || !IsValid(Actor))
    {
        return;
    }
    const bool bSight = Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>();
    const bool bHearing = Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>();
    if (bSight)
    {
        BB->SetValueAsBool(StalkerKeys::HasSight, Stimulus.WasSuccessfullySensed());
        if (Stimulus.WasSuccessfullySensed())
        {
            BB->SetValueAsObject(StalkerKeys::TargetActor, Actor);
            BB->SetValueAsVector(StalkerKeys::InvestigationLocation, Stimulus.StimulusLocation);
            BB->SetValueAsBool(StalkerKeys::HasInvestigation, false);
            SetFocus(Actor);
        }
        else
        {
            ClearFocus(EAIFocusPriority::Gameplay);
            BB->ClearValue(StalkerKeys::TargetActor);
            // Last perceived position, not the player's current hidden position.
            if (!Stimulus.StimulusLocation.ContainsNaN()
                && Stimulus.StimulusLocation != FAISystem::InvalidLocation)
            {
                BB->SetValueAsVector(StalkerKeys::InvestigationLocation, Stimulus.StimulusLocation);
            }
            BB->SetValueAsBool(StalkerKeys::HasInvestigation, true);
            InvestigationExpiresAt = GetWorld()->GetTimeSeconds() + 8.0 * StormAggression;
        }
    }
    else if (bHearing && Stimulus.WasSuccessfullySensed()
        && !BB->GetValueAsBool(StalkerKeys::HasSight))
    {
        BB->SetValueAsVector(StalkerKeys::InvestigationLocation, Stimulus.StimulusLocation);
        BB->SetValueAsBool(StalkerKeys::HasInvestigation, true);
        InvestigationExpiresAt = GetWorld()->GetTimeSeconds() + 8.0 * StormAggression;
    }
    UpdateDistanceBudget();
}

void AStalkerAIController::UpdateDistanceBudget()
{
    const APawn* Player = TrackedPlayer.Get();
    const APawn* Stalker = GetPawn();
    float Interval = 1.0f;
    if (Player && Stalker)
    {
        const double DistanceSquared = FVector::DistSquared(Player->GetActorLocation(), Stalker->GetActorLocation());
        Interval = DistanceSquared < FMath::Square(1500.0)
            ? 0.10f : (DistanceSquared < FMath::Square(4000.0) ? 0.35f : 1.0f);
    }
    SetActorTickInterval(Interval);
}

void AStalkerAIController::SetStormAggression(float Multiplier)
{
    if (HasAuthority() && GetNetMode() != NM_Client && FMath::IsFinite(Multiplier))
    {
        StormAggression = FMath::Clamp(Multiplier, 1.0f, 1.5f);
    }
}

void AStalkerAIController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority())
    {
        return;
    }
    UpdateDistanceBudget();
    if (!TrackedPlayer.IsValid())
    {
        ResetKnowledge();
        return;
    }
    UBlackboardComponent* BB = GetBlackboardComponent();
    if (ACharacter* Character = Cast<ACharacter>(GetPawn()))
    {
        const bool bPursuing = BB && BB->GetValueAsBool(StalkerKeys::HasSight);
        const float SpeedScale = bPursuing ? FMath::Lerp(1.0f, 1.2f, (StormAggression - 1.0f) / 0.5f) : 1.0f;
        Character->GetCharacterMovement()->MaxWalkSpeed = BaseMovementSpeed * SpeedScale;
    }
    if (BB && BB->GetValueAsBool(StalkerKeys::HasInvestigation)
        && GetWorld()->GetTimeSeconds() >= InvestigationExpiresAt)
    {
        BB->SetValueAsBool(StalkerKeys::HasInvestigation, false);
    }
}
