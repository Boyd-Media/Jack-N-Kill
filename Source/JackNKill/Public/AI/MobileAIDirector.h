#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "MobileAIDirector.generated.h"

class AStormWeatherController;
class ATargetPoint;
class UCurveFloat;

UENUM(BlueprintType)
enum class EDirectorAgentKind : uint8
{
    AlienStalker, PublicOrderPatrol, OtherHuman
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDirectorEncounterSpawned,
    APawn*, Agent, EDirectorAgentKind, Kind);

UCLASS()
class JACKNKILL_API AMobileAIDirector : public AActor
{
    GENERATED_BODY()
public:
    AMobileAIDirector();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Director")
    TObjectPtr<AStormWeatherController> Weather;

    UPROPERTY(EditAnywhere, Category = "Director")
    TObjectPtr<UCurveFloat> TensionCurve;

    UPROPERTY(EditAnywhere, Category = "Director")
    TSubclassOf<APawn> AlienStalkerClass;

    UPROPERTY(EditAnywhere, Category = "Director")
    TSubclassOf<APawn> PublicOrderPatrolClass;

    UPROPERTY(EditInstanceOnly, Category = "Director")
    TArray<TSoftObjectPtr<ATargetPoint>> SpawnPoints;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Director")
    float PlayerStress = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Director")
    float DesiredTension = 0.0f;

    UPROPERTY(BlueprintAssignable, Category = "Director")
    FDirectorEncounterSpawned OnEncounterSpawned;

    // Health/ammo normalized by trusted server systems. Use ammo = 1 for a
    // loadout where ammunition scarcity is not relevant.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Director")
    void UpdatePlayerStatus(APawn* Player, float Health, float Ammo);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Director")
    void RemovePlayerStatus(APawn* Player);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Director")
    void NotifyEncounterStarted(APawn* Player);

    // Reserve BEFORE activating any external faction agent. Invalid GUID = full.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Director|Admission")
    FGuid RequestAgentSlot(EDirectorAgentKind Kind);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Director|Admission")
    bool ActivateReservedAgent(FGuid Handle, APawn* Agent);

    // Call only after behavior/movement/fire is stopped and the pawn is inactive.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Director|Admission")
    void ReleaseAgentSlot(FGuid Handle);

    UFUNCTION(BlueprintPure, Category = "Director|Admission")
    int32 GetOccupiedSlotCount() const { return Agents.Num(); }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    struct FPlayerStatus
    {
        TWeakObjectPtr<APawn> Player;
        float Health = 1.0f;
        float Ammo = 1.0f;
        double LastEncounter = 0.0;
        double LastTelemetry = 0.0;
    };
    struct FAgentSlot
    {
        FGuid Handle;
        EDirectorAgentKind Kind;
        TWeakObjectPtr<APawn> Agent;
        bool bReserved = true;
        double ReservationExpiresAt = 0.0;
    };
    TArray<FPlayerStatus> Players;
    TArray<FAgentSlot> Agents;
    FTimerHandle DecisionTimer;
    double CycleStartTime = 0.0;
    double NextSpawnTime = 0.0;
    double RecoveryUntil = 0.0;

    bool IsDirectorAuthority() const;
    void Evaluate();
    bool SpawnEncounter(EDirectorAgentKind Kind, APawn* FocusPlayer);
    void ApplyAgentWeather(APawn* Agent);

    UFUNCTION()
    void HandleStormChanged(float Intensity, float NoiseScale, float Aggression);

    UFUNCTION()
    void HandleAgentEndPlay(AActor* Agent, EEndPlayReason::Type Reason);
};
