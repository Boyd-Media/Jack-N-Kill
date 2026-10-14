#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "StalkerAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UBehaviorTree;

UCLASS()
class JACKNKILL_API AStalkerAIController : public AAIController
{
    GENERATED_BODY()
public:
    AStalkerAIController();

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

    // Single-player default is player zero; assign explicitly for other modes.
    UFUNCTION(BlueprintCallable, Category = "AI")
    void SetTrackedPlayer(APawn* Player);

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Weather")
    float StormAggression = 1.0f;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AI|Weather")
    void SetStormAggression(float Multiplier);

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "AI")
    TObjectPtr<UAIPerceptionComponent> StalkerPerception;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

    TWeakObjectPtr<APawn> TrackedPlayer;
    double InvestigationExpiresAt = 0.0;
    float BaseMovementSpeed = 600.0f;

    UFUNCTION()
    void OnTargetUpdated(AActor* Actor, FAIStimulus Stimulus);

    void UpdateDistanceBudget();
    void ResetKnowledge();
};
