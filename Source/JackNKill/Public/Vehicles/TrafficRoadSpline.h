#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficRoadSpline.generated.h"

class USplineComponent;
class ATrafficManager;

UCLASS()
class JACKNKILL_API ATrafficRoadSpline : public AActor
{
    GENERATED_BODY()
public:
    ATrafficRoadSpline();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traffic")
    TObjectPtr<USplineComponent> Spline;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic", meta = (ClampMin = "1"))
    float SpeedCmPerSecond = 1600.0f;

    // Optional streaming-safe registration with an already-loaded manager.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
    TSoftObjectPtr<ATrafficManager> Manager;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
