#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficManager.generated.h"

class ATrafficRoadSpline;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class APawn;

UCLASS()
class JACKNKILL_API ATrafficManager : public AActor
{
    GENERATED_BODY()
public:
    ATrafficManager();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traffic")
    TObjectPtr<UInstancedStaticMeshComponent> TrafficInstances;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
    TObjectPtr<UStaticMesh> VehicleMesh;

    // Register local/loaded lane actors; do not scan the world every frame.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
    TArray<TSoftObjectPtr<ATrafficRoadSpline>> Roads;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic", meta = (ClampMin = "1", ClampMax = "256"))
    int32 MaxVehicles = 96;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic", meta = (ClampMin = "1"))
    float SpawnRadiusCm = 15000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic", meta = (ClampMin = "1"))
    float DespawnRadiusCm = 25000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic", meta = (ClampMin = "1"))
    float MinimumSpacingCm = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
    FRotator MeshForwardCorrection = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
    float MeshHeightOffsetCm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
    int32 RandomSeed = 12345;

    UFUNCTION(BlueprintCallable, Category = "Traffic")
    void SetTrackedPlayer(APawn* Player);

    UFUNCTION(BlueprintCallable, Category = "Traffic")
    void RegisterRoad(ATrafficRoadSpline* Road);

    UFUNCTION(BlueprintCallable, Category = "Traffic")
    void UnregisterRoad(ATrafficRoadSpline* Road);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    struct FAmbientCar
    {
        TWeakObjectPtr<ATrafficRoadSpline> Road;
        float DistanceCm = 0.0f;
        float SpeedCmPerSecond = 0.0f;
        FTransform WorldTransform = FTransform::Identity;
    };

    TWeakObjectPtr<APawn> TrackedPlayer;
    TArray<FAmbientCar> Cars;
    TArray<FTransform> InstanceTransforms;
    FRandomStream Random;
    float SpawnElapsed = 0.0f;
    bool bInstancesNeedRebuild = false;

    bool TrySpawn(const FVector& PlayerLocation);
    FTransform CarTransform(const FAmbientCar& Car) const;
    void ClearTraffic();
};
