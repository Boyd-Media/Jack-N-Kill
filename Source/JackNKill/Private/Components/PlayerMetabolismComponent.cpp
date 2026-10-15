#include "Components/PlayerMetabolismComponent.h"

#include "Components/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Systems/GameplayRules.h"
#include "World/PersistenceAccess.h"

namespace
{
    float SafeValue(float Value, float Fallback, float Minimum, float Maximum)
    {
        return FMath::IsFinite(Value) ? FMath::Clamp(Value, Minimum, Maximum) : Fallback;
    }
}

UPlayerMetabolismComponent::UPlayerMetabolismComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UPlayerMetabolismComponent::CanChangeState(bool bRestore) const
{
    return IsValid(GetOwner()) && GetOwner()->HasAuthority()
        && JNK::CanMutateDuringPersistence(GetWorld(), bRestore);
}

void UPlayerMetabolismComponent::BeginPlay()
{
    Super::BeginPlay();
    Character = Cast<ACharacter>(GetOwner());
    Combat = GetOwner()->FindComponentByClass<UCombatComponent>();
    if (!Character.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerMetabolismComponent requires an ACharacter owner."));
        return;
    }
    USkeletalMeshComponent* Mesh = Character->GetMesh();
    USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    if (!Asset || !Asset->FindMorphTarget(TEXT("Fat")) || !Asset->FindMorphTarget(TEXT("Muscular")))
    {
        UE_LOG(LogTemp, Warning, TEXT("Author/import Fat and Muscular morph targets on the player mesh."));
    }
    if (CanChangeState())
    {
        ApplyBodyState();
    }
}

void UPlayerMetabolismComponent::AddCalories(float Calories)
{
    if (CanChangeState() && FMath::IsFinite(Calories) && Calories > 0.0f)
    {
        CaloricIntake = FMath::Min(CaloricIntake + FMath::Min(Calories, 100000.0f), 100000.0f);
    }
}

void UPlayerMetabolismComponent::RecordStaminaBurn(float ActivityCalories)
{
    if (CanChangeState() && FMath::IsFinite(ActivityCalories) && ActivityCalories > 0.0f)
    {
        StaminaBurn = FMath::Min(StaminaBurn + FMath::Min(ActivityCalories, 100000.0f), 100000.0f);
    }
}

void UPlayerMetabolismComponent::CompleteDay()
{
    const float Resting = SafeValue(RestingCaloriesPerDay, 1800.0f, 0.0f, 100000.0f);
    const float CaloriesScale = SafeValue(CaloriesPerFullFatMorph, 30000.0f, 1.0f, 1000000.0f);
    const float Training = SafeValue(TrainingCaloriesPerDay, 400.0f, 1.0f, 100000.0f);
    const float Gain = SafeValue(MuscleGainPerDay, 0.025f, 0.0f, 1.0f);
    const float Loss = SafeValue(MuscleLossPerDay, 0.010f, 0.0f, 1.0f);
    const float Expenditure = Resting + StaminaBurn;
    TargetFat = FMath::Clamp(TargetFat + (CaloricIntake - Expenditure) / CaloriesScale, 0.0f, 1.0f);
    const bool bFed = CaloricIntake >= Expenditure * 0.9f;
    const float TrainingFactor = FMath::Clamp(StaminaBurn / Training, 0.0f, 1.0f);
    TargetMuscular = FMath::Clamp(TargetMuscular
        + (bFed && TrainingFactor > 0.0f ? Gain * TrainingFactor : -Loss), 0.0f, 1.0f);
    CaloricIntake = 0.0f;
    StaminaBurn = 0.0f;
}

bool UPlayerMetabolismComponent::AdvanceGameDays(float ElapsedDays)
{
    if (!CanChangeState() || !FMath::IsFinite(ElapsedDays) || ElapsedDays < 0.0f || ElapsedDays > 30.0f)
    {
        return false;
    }
    const double HalfLife = SafeValue(MorphHalfLifeDays, 2.0f, 0.01f, 365.0f);
    double Remaining = ElapsedDays;
    while (Remaining > 0.0)
    {
        const double Step = FMath::Min(Remaining, 1.0 - DayProgress);
        // Exponential smoothing has the same result independent of clock cadence
        // between daily target changes, including accelerated game time.
        const float Alpha = static_cast<float>(JNK::Rules::SmoothingAlpha(Step, HalfLife));
        Fat = FMath::Clamp(FMath::Lerp(Fat, TargetFat, Alpha), 0.0f, 1.0f);
        Muscular = FMath::Clamp(FMath::Lerp(Muscular, TargetMuscular, Alpha), 0.0f, 1.0f);
        DayProgress += Step;
        Remaining -= Step;
        if (DayProgress >= 1.0 - 1e-9)
        {
            DayProgress = 0.0;
            CompleteDay();
        }
    }
    ApplyBodyState();
    return true;
}

void UPlayerMetabolismComponent::SetSprintRequested(bool bRequested)
{
    if (CanChangeState())
    {
        bSprintRequested = bRequested;
        ApplyBodyState();
    }
}

void UPlayerMetabolismComponent::ApplyBodyState()
{
    if (Character.IsValid())
    {
        if (USkeletalMeshComponent* Mesh = Character->GetMesh())
        {
            // Avoid re-uploading effectively identical mobile morph weights.
            if (FMath::Abs(Fat - LastAppliedFat) >= 0.001f)
            {
                Mesh->SetMorphTarget(TEXT("Fat"), Fat);
                LastAppliedFat = Fat;
            }
            if (FMath::Abs(Muscular - LastAppliedMuscular) >= 0.001f)
            {
                Mesh->SetMorphTarget(TEXT("Muscular"), Muscular);
                LastAppliedMuscular = Muscular;
            }
        }
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            const float SprintScale = static_cast<float>(JNK::Rules::SprintMultiplier(Fat, FatSprintMultiplier));
            Movement->MaxWalkSpeed = bSprintRequested
                ? SafeValue(BaseSprintSpeed, 650.0f, 0.0f, 3000.0f) * SprintScale
                : SafeValue(BaseWalkSpeed, 450.0f, 0.0f, 3000.0f);
        }
    }
    if (Combat.IsValid())
    {
        Combat->SetMetabolismDamageMultiplier(static_cast<float>(JNK::Rules::MeleeMultiplier(Muscular, MuscularDamageMultiplier)));
    }
}

FPlayerMetabolismSnapshot UPlayerMetabolismComponent::GetSnapshot() const
{
    FPlayerMetabolismSnapshot Result;
    Result.Fat = Fat;
    Result.Muscular = Muscular;
    Result.TargetFat = TargetFat;
    Result.TargetMuscular = TargetMuscular;
    // Avoid rounding a just-below-midnight double to an invalid 1.0f snapshot.
    Result.DayProgress = FMath::Min(static_cast<float>(DayProgress), 0.99999994f);
    Result.CaloricIntake = CaloricIntake;
    Result.StaminaBurn = StaminaBurn;
    return Result;
}

bool UPlayerMetabolismComponent::RestoreSnapshot(const FPlayerMetabolismSnapshot& Snapshot)
{
    if (!CanChangeState(true) || !FMath::IsFinite(Snapshot.Fat) || !FMath::IsFinite(Snapshot.Muscular)
        || !FMath::IsFinite(Snapshot.TargetFat) || !FMath::IsFinite(Snapshot.TargetMuscular)
        || !FMath::IsFinite(Snapshot.DayProgress) || Snapshot.DayProgress < 0.0f || Snapshot.DayProgress >= 1.0f
        || !FMath::IsFinite(Snapshot.CaloricIntake) || !FMath::IsFinite(Snapshot.StaminaBurn))
    {
        return false;
    }
    Fat = FMath::Clamp(Snapshot.Fat, 0.0f, 1.0f);
    Muscular = FMath::Clamp(Snapshot.Muscular, 0.0f, 1.0f);
    TargetFat = FMath::Clamp(Snapshot.TargetFat, 0.0f, 1.0f);
    TargetMuscular = FMath::Clamp(Snapshot.TargetMuscular, 0.0f, 1.0f);
    DayProgress = Snapshot.DayProgress;
    bSprintRequested = false;
    CaloricIntake = FMath::Clamp(Snapshot.CaloricIntake, 0.0f, 100000.0f);
    StaminaBurn = FMath::Clamp(Snapshot.StaminaBurn, 0.0f, 100000.0f);
    LastAppliedFat = LastAppliedMuscular = -1.0f;
    ApplyBodyState();
    return true;
}

void UPlayerMetabolismComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (CanChangeState())
    {
        if (Combat.IsValid())
        {
            Combat->SetMetabolismDamageMultiplier(1.0f);
        }
        if (Character.IsValid() && Character->GetCharacterMovement())
        {
            Character->GetCharacterMovement()->MaxWalkSpeed = SafeValue(BaseWalkSpeed, 450.0f, 0.0f, 3000.0f);
        }
    }
    Super::EndPlay(EndPlayReason);
}
