#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StormWeatherController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FStormChanged,
    float, Intensity, float, FootstepRadiusScale, float, StalkerAggression);

UCLASS()
class JACKNKILL_API AStormWeatherController : public AActor
{
    GENERATED_BODY()
public:
    AStormWeatherController();

    UPROPERTY(BlueprintAssignable, Category = "Weather")
    FStormChanged OnStormChanged;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weather")
    void SetStormIntensity(float Intensity);

    UFUNCTION(BlueprintPure, Category = "Weather")
    float GetStormIntensity() const { return StormIntensity; }

    UFUNCTION(BlueprintPure, Category = "Weather")
    float GetFootstepRadiusScale() const;

    UFUNCTION(BlueprintPure, Category = "Weather")
    float GetStalkerAggression() const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weather")
    void ReportPlayerFootstep(APawn* Player, FVector Location,
        float BaseRadiusCm = 1200.0f, float Loudness = 1.0f);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(ReplicatedUsing = OnRep_StormIntensity)
    float StormIntensity = 0.0f;

    UFUNCTION()
    void OnRep_StormIntensity();
};
