#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "PublicOrderAIController.generated.h"

class APublicOrderPlayerController;

UENUM(BlueprintType)
enum class EPublicOrderSuspectKind : uint8
{
    Player, GangMember
};

UENUM(BlueprintType)
enum class EPublicOrderState : uint8
{
    Patrol, Pursuing, AwaitingBribe, Arresting, Hostile
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPublicOrderAction, APawn*, Suspect);

// Gatorwake Public Order Bureau behavior. Crime reports come from trusted
// server-side witness/gameplay systems, never unchecked client submissions.
UCLASS()
class JACKNKILL_API APublicOrderAIController : public AAIController
{
    GENERATED_BODY()
public:
    APublicOrderAIController();

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Public Order")
    float CorruptionLevel = 0.0f;

    // INDEX_NONE randomizes per instance. Supply unique saved seeds for replay.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Public Order")
    int32 CorruptionSeed = INDEX_NONE;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Public Order")
    int32 BribeCredits = 500;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Public Order")
    float BribeTimeoutSeconds = 15.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Public Order")
    float ApproachDistanceCm = 200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Public Order")
    float ForceRangeCm = 1200.0f;

    UPROPERTY(BlueprintAssignable, Category = "Public Order")
    FPublicOrderAction OnArrestRequested;

    // The combat layer owns aiming, LOS checks, fire cadence and damage.
    UPROPERTY(BlueprintAssignable, Category = "Public Order")
    FPublicOrderAction OnUseForceRequested;

    UPROPERTY(BlueprintAssignable, Category = "Public Order")
    FPublicOrderAction OnEnforcementEnded;

    UFUNCTION(BlueprintPure, Category = "Public Order")
    EPublicOrderState GetEnforcementState() const { return State; }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Public Order")
    void ReportCrime(FGuid CrimeId, APawn* Suspect, EPublicOrderSuspectKind Kind,
        bool bUseForceAllowed);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Public Order")
    void FinishArrest(bool bSucceeded);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Public Order")
    void ClearEncounter();

    void ResolveBribe(const FGuid& RequestId, APublicOrderPlayerController* Player,
        bool bAccept);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    EPublicOrderState State = EPublicOrderState::Patrol;
    TWeakObjectPtr<APawn> CurrentSuspect;
    TWeakObjectPtr<APublicOrderPlayerController> BribePlayer;
    TArray<FGuid> RecentCrimeIds;
    FGuid ActiveBribeId;
    FTimerHandle BribeTimer;
    double BribeDeadline = 0.0;
    double NextMoveRetryAt = 0.0;
    bool bForceAllowed = false;

    void StartBribePrompt();
    void BecomeHostile();
    void OnBribeExpired();
    void ClosePrompt(bool bPaid);
};
