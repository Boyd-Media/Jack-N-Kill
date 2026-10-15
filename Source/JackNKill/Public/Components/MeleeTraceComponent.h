#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/BaseDamageInterface.h"
#include "MeleeTraceComponent.generated.h"

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class JACKNKILL_API UMeleeTraceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMeleeTraceComponent();
    void BeginAttackWindow();
    void EndAttackWindow();
    void SweepAttack(const FVector& StartWS, const FVector& EndWS,
        float Radius, const FBaseDamageHit& HitTemplate,
        AActor* WeaponActor = nullptr);

private:
    bool bAttackWindowOpen = false;
    bool bSweepInProgress = false;
    uint32 WindowRevision = 0;
    TSet<TWeakObjectPtr<AActor>> HitActors;
    TArray<FHitResult> SweepResults;
};
