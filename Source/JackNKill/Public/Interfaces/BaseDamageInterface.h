#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BaseDamageInterface.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EBaseHitReaction : uint8
{
    None, Stagger, Knockdown, Launch, Execution
};

USTRUCT(BlueprintType)
struct JACKNKILL_API FBaseDamageHit
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    TObjectPtr<AActor> Attacker = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    float Damage = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    EBaseHitReaction Reaction = EBaseHitReaction::Stagger;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    FVector ImpactPointWS = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    FVector ImpactNormalWS = FVector::ZeroVector;

    // Normalized incoming direction; zero if no direction is available.
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    FVector HitDirectionWS = FVector::ZeroVector;
};

UINTERFACE(BlueprintType, Blueprintable)
class JACKNKILL_API UBaseDamageInterface : public UInterface
{
    GENERATED_BODY()
};

class JACKNKILL_API IBaseDamageInterface
{
    GENERATED_BODY()

public:
    // Query only; do not mutate gameplay state here.
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat")
    bool CanReceiveDamageHit(const FBaseDamageHit& Hit) const;

    // Revalidate eligibility before applying damage and requested reactions.
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat")
    void ReceiveDamageHit(const FBaseDamageHit& Hit);
};
