#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerMetabolismComponent.generated.h"

class ACharacter;
class UCombatComponent;

USTRUCT(BlueprintType)
struct JACKNKILL_API FPlayerMetabolismSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float Fat = 0.30f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float Muscular = 0.20f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float TargetFat = 0.30f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float TargetMuscular = 0.20f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float DayProgress = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float CaloricIntake = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    float StaminaBurn = 0.0f;
};

UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class JACKNKILL_API UPlayerMetabolismComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPlayerMetabolismComponent();

    // Accumulated food kcal and activity kcal-equivalent for the current game day.
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Metabolism")
    float CaloricIntake = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Metabolism")
    float StaminaBurn = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Metabolism|Morphs")
    float Fat = 0.30f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Metabolism|Morphs")
    float Muscular = 0.20f;

    UPROPERTY(EditDefaultsOnly, Category = "Metabolism", meta = (ClampMin = "0"))
    float RestingCaloriesPerDay = 1800.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Metabolism", meta = (ClampMin = "1"))
    float CaloriesPerFullFatMorph = 30000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Metabolism", meta = (ClampMin = "1"))
    float TrainingCaloriesPerDay = 400.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Metabolism", meta = (ClampMin = "0"))
    float MuscleGainPerDay = 0.025f;

    UPROPERTY(EditDefaultsOnly, Category = "Metabolism", meta = (ClampMin = "0"))
    float MuscleLossPerDay = 0.010f;

    UPROPERTY(EditDefaultsOnly, Category = "Metabolism", meta = (ClampMin = "0.01"))
    float MorphHalfLifeDays = 2.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Metabolism|Movement", meta = (ClampMin = "0"))
    float BaseWalkSpeed = 450.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Metabolism|Movement", meta = (ClampMin = "0"))
    float BaseSprintSpeed = 650.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Metabolism|Movement", meta = (ClampMin = "0.1", ClampMax = "1"))
    float FatSprintMultiplier = 0.75f;

    UPROPERTY(EditDefaultsOnly, Category = "Metabolism|Combat", meta = (ClampMin = "1", ClampMax = "3"))
    float MuscularDamageMultiplier = 1.25f;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Metabolism")
    void AddCalories(float Calories);

    // The stamina system converts spent stamina into authored activity kcal.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Metabolism")
    void RecordStaminaBurn(float ActivityCalories);

    // Call from the world clock. 1.0 = one game day, not one real-time second.
    // Returns false without changes for invalid input or jumps above 30 days.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Metabolism")
    bool AdvanceGameDays(float ElapsedDays);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Metabolism|Movement")
    void SetSprintRequested(bool bRequested);

    UFUNCTION(BlueprintPure, Category = "Metabolism|Save")
    FPlayerMetabolismSnapshot GetSnapshot() const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Metabolism|Save")
    bool RestoreSnapshot(const FPlayerMetabolismSnapshot& Snapshot);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TWeakObjectPtr<ACharacter> Character;
    TWeakObjectPtr<UCombatComponent> Combat;
    float TargetFat = 0.30f;
    float TargetMuscular = 0.20f;
    double DayProgress = 0.0;
    bool bSprintRequested = false;
    float LastAppliedFat = -1.0f;
    float LastAppliedMuscular = -1.0f;

    void CompleteDay();
    void ApplyBodyState();
    bool CanChangeState() const;
};
