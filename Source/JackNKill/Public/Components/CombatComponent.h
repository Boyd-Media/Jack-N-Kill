#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/BaseDamageInterface.h"
#include "CombatComponent.generated.h"

class UCameraShakeBase;
class USceneComponent;

UENUM(BlueprintType)
enum class ECombatState : uint8
{
    Idle, Startup, Active, Recovery, HitStun, BlockStun
};

UENUM(BlueprintType)
enum class ECombatTraceShape : uint8
{
    Sphere, Capsule
};

USTRUCT(BlueprintType)
struct JACKNKILL_API FMeleeAttackDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.001"))
    float StartupSeconds = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.001"))
    float ActiveSeconds = 0.10f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.001"))
    float RecoverySeconds = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0"))
    float Damage = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    EBaseHitReaction Reaction = EBaseHitReaction::Stagger;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
    ECombatTraceShape Shape = ECombatTraceShape::Sphere;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (ClampMin = "0.1"))
    float Radius = 12.0f;

    // Capsule half-height includes its hemispheres; must be >= Radius.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (ClampMin = "0.1"))
    float CapsuleHalfHeight = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback", meta = (ClampMin = "0"))
    float HitstopSeconds = 0.035f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
    TSubclassOf<UCameraShakeBase> CameraShake;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCombatStateChanged,
    ECombatState, PreviousState, ECombatState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FCombatImpactFeedback,
    AActor*, Target, const FBaseDamageHit&, Hit,
    float, HitstopSeconds, TSubclassOf<UCameraShakeBase>, CameraShake);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class JACKNKILL_API UCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    TMap<FName, FMeleeAttackDefinition> Attacks;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input", meta = (ClampMin = "1", ClampMax = "8"))
    int32 MaxBufferedInputs = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input", meta = (ClampMin = "0.01"))
    float BufferLifetimeSeconds = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Trace")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel1;

    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FCombatStateChanged OnStateChanged;

    // Dispatch only. The local presentation layer owns freeze/shake application.
    UPROPERTY(BlueprintAssignable, Category = "Combat|Feedback")
    FCombatImpactFeedback OnImpactFeedback;

    UFUNCTION(BlueprintPure, Category = "Combat")
    ECombatState GetCombatState() const { return State; }

    // Returns true if started or queued. Full queues reject the newest input.
    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool RequestAttack(FName AttackId);

    // Source must belong to this actor or its owned weapon actor.
    UFUNCTION(BlueprintCallable, Category = "Combat|Trace")
    void SetTraceSource(USceneComponent* Source, FName SocketName);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ApplyStun(ECombatState StunState, float DurationSeconds);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void CancelCombat();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat|Modifiers")
    void SetMetabolismDamageMultiplier(float Multiplier);

    UFUNCTION(BlueprintPure, Category = "Combat|Modifiers")
    float GetMetabolismDamageMultiplier() const { return MetabolismDamageMultiplier; }

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    struct FBufferedAttack
    {
        FName Id;
        double ExpiresAt = 0.0;
    };

    ECombatState State = ECombatState::Idle;
    uint32 StateRevision = 0;
    FTimerHandle StateTimer;
    FMeleeAttackDefinition CurrentAttack;
    float MetabolismDamageMultiplier = 1.0f;
    TArray<FBufferedAttack> InputQueue;
    TSet<TWeakObjectPtr<AActor>> HitActors;
    TWeakObjectPtr<USceneComponent> TraceSource;
    FName TraceSocket;
    FVector PreviousSampleWS = FVector::ZeroVector;

    bool StartAttack(FName AttackId);
    void EnterState(ECombatState NewState, float DurationSeconds = 0.0f);
    void AdvanceState();
    void PruneInputQueue();
    void ConsumeBufferedAttack();
    void TraceActiveHit();
    bool ReadTraceTransform(FTransform& OutTransform) const;
};
