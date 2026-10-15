#include "Components/MeleeTraceComponent.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UMeleeTraceComponent::UMeleeTraceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SweepResults.Reserve(8);
}

void UMeleeTraceComponent::BeginAttackWindow()
{
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !IsValid(GetOwner()) || !GetOwner()->HasAuthority())
    {
        return;
    }
    ++WindowRevision;
    HitActors.Reset();
    bAttackWindowOpen = true;
}

void UMeleeTraceComponent::EndAttackWindow()
{
    ++WindowRevision;
    bAttackWindowOpen = false;
    HitActors.Reset();
}

void UMeleeTraceComponent::SweepAttack(const FVector& StartWS,
    const FVector& EndWS, float Radius, const FBaseDamageHit& HitTemplate,
    AActor* WeaponActor)
{
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (bSweepInProgress || !bAttackWindowOpen || !World || !IsValid(Owner)
        || World->GetNetMode() == NM_Client || !Owner->HasAuthority()
        || !FMath::IsFinite(Radius) || Radius <= 0.0f
        || StartWS.ContainsNaN() || EndWS.ContainsNaN())
    {
        return;
    }

    TGuardValue<bool> SweepGuard(bSweepInProgress, true);
    const uint32 Revision = WindowRevision;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(JNKMeleeSweep), false);
    Params.AddIgnoredActor(Owner);
    if (IsValid(HitTemplate.Attacker.Get()))
    {
        Params.AddIgnoredActor(HitTemplate.Attacker.Get());
    }
    if (IsValid(WeaponActor))
    {
        Params.AddIgnoredActor(WeaponActor);
    }

    SweepResults.Reset();
    World->SweepMultiByChannel(SweepResults, StartWS, EndWS,
        FQuat::Identity, ECC_GameTraceChannel1,
        FCollisionShape::MakeSphere(Radius), Params);

    for (const FHitResult& Contact : SweepResults)
    {
        if (!bAttackWindowOpen || Revision != WindowRevision)
        {
            break;
        }
        AActor* Target = Contact.GetActor();
        if (!IsValid(Target) || !Target->GetClass()->ImplementsInterface(
            UBaseDamageInterface::StaticClass()))
        {
            continue;
        }
        const TWeakObjectPtr<AActor> TargetKey(Target);
        if (HitActors.Contains(TargetKey))
        {
            continue;
        }

        FBaseDamageHit Hit = HitTemplate;
        Hit.Attacker = IsValid(HitTemplate.Attacker.Get())
            ? HitTemplate.Attacker.Get() : Owner;
        Hit.Damage = FMath::IsFinite(Hit.Damage)
            ? FMath::Max(0.0f, Hit.Damage) : 0.0f;
        Hit.ImpactPointWS = Contact.ImpactPoint;
        Hit.ImpactNormalWS = Contact.ImpactNormal.GetSafeNormal();
        Hit.HitDirectionWS = (EndWS - StartWS).GetSafeNormal();
        if (Hit.HitDirectionWS.IsNearlyZero())
        {
            Hit.HitDirectionWS = (Target->GetActorLocation()
                - Hit.Attacker->GetActorLocation()).GetSafeNormal();
        }
        if (IBaseDamageInterface::Execute_CanReceiveDamageHit(Target, Hit)
            && IsValid(Target) && bAttackWindowOpen && Revision == WindowRevision)
        {
            HitActors.Add(TargetKey);
            IBaseDamageInterface::Execute_ReceiveDamageHit(Target, Hit);
        }
    }
}
