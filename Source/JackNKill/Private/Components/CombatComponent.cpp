#include "Components/CombatComponent.h"

#include "Camera/CameraShakeBase.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
    bool IsPositiveFinite(float Value)
    {
        return FMath::IsFinite(Value) && Value > 0.0f;
    }

    bool IsAttackValid(const FMeleeAttackDefinition& Attack)
    {
        return IsPositiveFinite(Attack.StartupSeconds)
            && IsPositiveFinite(Attack.ActiveSeconds)
            && IsPositiveFinite(Attack.RecoverySeconds)
            && IsPositiveFinite(Attack.Radius)
            && FMath::IsFinite(Attack.Damage) && Attack.Damage >= 0.0f
            && FMath::IsFinite(Attack.HitstopSeconds) && Attack.HitstopSeconds >= 0.0f
            && (Attack.Shape == ECombatTraceShape::Sphere
                || (Attack.Shape == ECombatTraceShape::Capsule
                    && FMath::IsFinite(Attack.CapsuleHalfHeight)
                    && Attack.CapsuleHalfHeight >= Attack.Radius));
    }
}

UCombatComponent::UCombatComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    InputQueue.Reserve(3);
}

void UCombatComponent::SetTraceSource(USceneComponent* Source, FName SocketName)
{
    if (!HasCombatAuthority())
    {
        return;
    }
    if (TraceSource.IsValid())
    {
        RemoveTickPrerequisiteComponent(TraceSource.Get());
    }
    TraceSource.Reset();
    AActor* Owner = GetOwner();
    if (IsValid(Source) && IsValid(Owner)
        && (Source->GetOwner() == Owner
            || (IsValid(Source->GetOwner()) && Source->GetOwner()->IsOwnedBy(Owner))))
    {
        TraceSource = Source;
        TraceSocket = SocketName;
        AddTickPrerequisiteComponent(Source);
    }
    // Never sweep from a previous weapon's sample into a replacement weapon.
    if (State != ECombatState::Idle)
    {
        CancelCombat();
    }
}

bool UCombatComponent::ReadTraceTransform(FTransform& OutTransform) const
{
    const USceneComponent* Source = TraceSource.Get();
    if (!Source || (!TraceSocket.IsNone() && !Source->DoesSocketExist(TraceSocket)))
    {
        return false;
    }
    OutTransform = TraceSocket.IsNone() ? Source->GetComponentTransform()
        : Source->GetSocketTransform(TraceSocket, RTS_World);
    return !OutTransform.ContainsNaN();
}

bool UCombatComponent::RequestAttack(FName AttackId)
{
    if (!GetWorld() || !IsValid(GetOwner()) || AttackId.IsNone())
    {
        return false;
    }
    if (HasCombatAuthority())
    {
        return RequestAttackAuthoritative(AttackId);
    }
    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (GetWorld()->GetNetMode() != NM_Client || !Pawn || !Pawn->IsLocallyControlled()
        || !Pawn->IsPlayerControlled() || !Pawn->GetIsReplicated() || !GetIsReplicated()
        || !Attacks.Contains(AttackId) || GetWorld()->GetTimeSeconds() < NextLocalRequestTime)
    {
        return false;
    }
    NextLocalRequestTime = GetWorld()->GetTimeSeconds() + 0.05;
    ServerRequestAttack(AttackId);
    return true;
}

bool UCombatComponent::HasCombatAuthority() const
{
    return GetWorld() && IsValid(GetOwner())
        && GetWorld()->GetNetMode() != NM_Client && GetOwner()->HasAuthority();
}

void UCombatComponent::ServerRequestAttack_Implementation(FName AttackId)
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (!HasCombatAuthority() || GetWorld()->GetNetMode() == NM_Standalone
        || !Pawn || !Pawn->IsPlayerControlled() || AttackId.IsNone()
        || GetWorld()->GetTimeSeconds() < NextRemoteRequestTime)
    {
        return;
    }
    NextRemoteRequestTime = GetWorld()->GetTimeSeconds() + 0.05;
    // UE enforces ownership. Client submits no target, damage or hit vectors.
    RequestAttackAuthoritative(AttackId);
}

bool UCombatComponent::RequestAttackAuthoritative(FName AttackId)
{
    const FMeleeAttackDefinition* Definition = Attacks.Find(AttackId);
    FTransform Transform;
    if (!HasCombatAuthority() || !Definition || !IsAttackValid(*Definition)
        || !ReadTraceTransform(Transform)
        || State == ECombatState::HitStun || State == ECombatState::BlockStun)
    {
        return false;
    }
    if (State == ECombatState::Idle)
    {
        return StartAttack(AttackId);
    }
    PruneInputQueue();
    if (InputQueue.Num() >= FMath::Clamp(MaxBufferedInputs, 1, 8)
        || !IsPositiveFinite(BufferLifetimeSeconds))
    {
        return false;
    }
    InputQueue.Add({AttackId, GetWorld()->GetTimeSeconds() + BufferLifetimeSeconds});
    return true;
}

bool UCombatComponent::StartAttack(FName AttackId)
{
    const FMeleeAttackDefinition* Definition = Attacks.Find(AttackId);
    FTransform Transform;
    if (!HasCombatAuthority() || !Definition || !IsAttackValid(*Definition) || !ReadTraceTransform(Transform))
    {
        return false;
    }
    CurrentAttack = *Definition;
    // Snapshot this modifier once per attack; never mutate the authored definition.
    CurrentAttack.Damage *= MetabolismDamageMultiplier;
    HitActors.Reset();
    NetworkState.AttackId = AttackId;
    NetworkState.AttackStartServerTime = GetWorld()->GetTimeSeconds();
    ++NetworkState.AttackSequence;
    const uint32 ExpectedRevision = StateRevision + 1;
    EnterState(ECombatState::Startup, CurrentAttack.StartupSeconds);
    if (StateRevision == ExpectedRevision)
    {
        OnAttackStarted.Broadcast(AttackId, 0.0f);
    }
    return true;
}

void UCombatComponent::EnterState(ECombatState NewState, float DurationSeconds)
{
    UWorld* World = GetWorld();
    if (!World || !HasCombatAuthority())
    {
        return;
    }
    World->GetTimerManager().ClearTimer(StateTimer);
    const ECombatState PreviousState = State;
    State = NewState;
    ++StateRevision;
    NetworkState.State = State;
    NetworkState.PhaseStartServerTime = World->GetTimeSeconds();
    NetworkState.PhaseDuration = DurationSeconds;
    NetworkState.StateSequence = StateRevision;
    if (World->GetNetMode() != NM_Standalone && GetOwner()->GetIsReplicated())
    {
        GetOwner()->ForceNetUpdate();
    }
    SetComponentTickEnabled(State == ECombatState::Active);
    if (DurationSeconds > 0.0f)
    {
        World->GetTimerManager().SetTimer(StateTimer, this,
            &UCombatComponent::AdvanceState, DurationSeconds, false);
    }
    // Observers may cancel or stun; callers must check StateRevision afterward.
    OnStateChanged.Broadcast(PreviousState, State);
}

void UCombatComponent::AdvanceState()
{
    if (!HasCombatAuthority())
    {
        return;
    }
    if (State == ECombatState::Startup)
    {
        FTransform Transform;
        if (!ReadTraceTransform(Transform))
        {
            CancelCombat();
            return;
        }
        PreviousSampleWS = Transform.GetLocation();
        EnterState(ECombatState::Active, CurrentAttack.ActiveSeconds);
    }
    else if (State == ECombatState::Active)
    {
        // Include the last sample, even for windows shorter than one frame.
        const uint32 Revision = StateRevision;
        TraceActiveHit();
        if (Revision == StateRevision)
        {
            EnterState(ECombatState::Recovery, CurrentAttack.RecoverySeconds);
        }
    }
    else if (State == ECombatState::Recovery
        || State == ECombatState::HitStun || State == ECombatState::BlockStun)
    {
        const uint32 ExpectedRevision = StateRevision + 1;
        EnterState(ECombatState::Idle);
        if (StateRevision == ExpectedRevision && State == ECombatState::Idle)
        {
            ConsumeBufferedAttack();
        }
    }
}

void UCombatComponent::PruneInputQueue()
{
    const double Now = GetWorld()->GetTimeSeconds();
    InputQueue.RemoveAll([Now](const FBufferedAttack& Input)
    {
        return Input.ExpiresAt <= Now;
    });
}

void UCombatComponent::ConsumeBufferedAttack()
{
    PruneInputQueue();
    while (State == ECombatState::Idle && !InputQueue.IsEmpty())
    {
        const FName Id = InputQueue[0].Id;
        InputQueue.RemoveAt(0);
        if (StartAttack(Id))
        {
            return;
        }
    }
}

void UCombatComponent::ApplyStun(ECombatState StunState, float DurationSeconds)
{
    if (!HasCombatAuthority() || (StunState != ECombatState::HitStun && StunState != ECombatState::BlockStun)
        || !IsPositiveFinite(DurationSeconds))
    {
        return;
    }
    InputQueue.Reset();
    HitActors.Reset();
    EnterState(StunState, DurationSeconds);
}

void UCombatComponent::CancelCombat()
{
    if (!HasCombatAuthority())
    {
        return;
    }
    InputQueue.Reset();
    HitActors.Reset();
    EnterState(ECombatState::Idle);
}

void UCombatComponent::SetMetabolismDamageMultiplier(float Multiplier)
{
    if (HasCombatAuthority() && FMath::IsFinite(Multiplier))
    {
        MetabolismDamageMultiplier = FMath::Clamp(Multiplier, 0.0f, 3.0f);
    }
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (HasCombatAuthority() && State == ECombatState::Active)
    {
        TraceActiveHit();
    }
}

void UCombatComponent::TraceActiveHit()
{
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    FTransform Transform;
    if (!HasCombatAuthority() || State != ECombatState::Active || !World || !IsValid(Owner))
    {
        return;
    }
    if (!ReadTraceTransform(Transform))
    {
        CancelCombat();
        return;
    }
    const uint32 Revision = StateRevision;
    const FVector Start = PreviousSampleWS;
    const FVector End = Transform.GetLocation();
    PreviousSampleWS = End;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(JNKCombatSweep), false);
    Params.AddIgnoredActor(Owner);
    if (TraceSource->GetOwner() != Owner)
    {
        Params.AddIgnoredActor(TraceSource->GetOwner());
    }
    const FCollisionShape Shape = CurrentAttack.Shape == ECombatTraceShape::Sphere
        ? FCollisionShape::MakeSphere(CurrentAttack.Radius)
        : FCollisionShape::MakeCapsule(CurrentAttack.Radius, CurrentAttack.CapsuleHalfHeight);
    FHitResult Contact;
    if (!World->SweepSingleByChannel(Contact, Start, End, Transform.GetRotation(),
        TraceChannel.GetValue(), Shape, Params))
    {
        return;
    }
    AActor* Target = Contact.GetActor();
    const TWeakObjectPtr<AActor> TargetKey(Target);
    if (!IsValid(Target) || HitActors.Contains(TargetKey)
        || !Target->GetClass()->ImplementsInterface(UBaseDamageInterface::StaticClass()))
    {
        return;
    }
    FBaseDamageHit Hit;
    Hit.Attacker = Owner;
    Hit.Damage = CurrentAttack.Damage;
    Hit.Reaction = CurrentAttack.Reaction;
    Hit.ImpactPointWS = Contact.ImpactPoint;
    Hit.ImpactNormalWS = Contact.ImpactNormal.GetSafeNormal();
    Hit.HitDirectionWS = (End - Start).GetSafeNormal();
    if (Hit.HitDirectionWS.IsNearlyZero())
    {
        Hit.HitDirectionWS = (Target->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
    }
    if (!IBaseDamageInterface::Execute_CanReceiveDamageHit(Target, Hit)
        || !IsValid(Target) || Revision != StateRevision)
    {
        return;
    }
    // Snapshot feedback before receiver callbacks can replace CurrentAttack.
    const float Hitstop = CurrentAttack.HitstopSeconds;
    const TSubclassOf<UCameraShakeBase> Shake = CurrentAttack.CameraShake;
    HitActors.Add(TargetKey);
    IBaseDamageInterface::Execute_ReceiveDamageHit(Target, Hit);
    if (World->GetNetMode() == NM_Standalone)
    {
        OnImpactFeedback.Broadcast(IsValid(Target) ? Target : nullptr, Hit, Hitstop, Shake);
    }
    else if (Owner->GetIsReplicated() && GetIsReplicated())
    {
        MulticastImpactFeedback(IsValid(Target) ? Target : nullptr, Hit, Hitstop, Shake);
    }
}

void UCombatComponent::MulticastImpactFeedback_Implementation(AActor* Target,
    FBaseDamageHit Hit, float HitstopSeconds, TSubclassOf<UCameraShakeBase> CameraShake)
{
    if (GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
    {
        OnImpactFeedback.Broadcast(Target, Hit, HitstopSeconds, CameraShake);
    }
}

void UCombatComponent::OnRep_NetworkState()
{
    const ECombatState Previous = State;
    State = NetworkState.State;
    SetComponentTickEnabled(false);
    if (NetworkState.AttackSequence != LastPresentedAttackSequence)
    {
        LastPresentedAttackSequence = NetworkState.AttackSequence;
        if (State == ECombatState::Startup || State == ECombatState::Active || State == ECombatState::Recovery)
        {
            const AGameStateBase* GameState = GetWorld()->GetGameState();
            const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
            const float Elapsed = static_cast<float>(FMath::Max(0.0, Now - NetworkState.AttackStartServerTime));
            OnAttackStarted.Broadcast(NetworkState.AttackId, Elapsed);
        }
    }
    OnStateChanged.Broadcast(Previous, State);
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCombatComponent, NetworkState);
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(StateTimer);
    }
    SetComponentTickEnabled(false);
    InputQueue.Reset();
    HitActors.Reset();
    Super::EndPlay(EndPlayReason);
}
