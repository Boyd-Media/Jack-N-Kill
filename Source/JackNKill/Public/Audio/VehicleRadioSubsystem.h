#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "VehicleRadioSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class AActor;

USTRUCT()
struct FVehicleRadioEntry
{
    GENERATED_BODY()

    UPROPERTY()
    TWeakObjectPtr<AActor> Vehicle;

    UPROPERTY()
    TObjectPtr<UAudioComponent> Audio;

    UPROPERTY()
    TArray<TObjectPtr<USoundBase>> Tracks;

    int32 TrackIndex = 0;
    bool bEnabled = false;
    bool bInside = false;
    float CutoffHz = 1800.0f;
    float Volume = 0.30f;
};

// Local cosmetic playback. Sound Waves/Cues use UAudioComponent; MediaSound
// instead requires a MediaPlayer and is not a playlist player for USoundBase.
UCLASS()
class JACKNKILL_API UVehicleRadioSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Radio")
    bool RegisterVehicle(AActor* Vehicle, const TArray<USoundBase*>& Playlist);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Radio")
    void UnregisterVehicle(AActor* Vehicle);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Radio")
    void NextTrack(AActor* Vehicle);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Radio")
    void PreviousTrack(AActor* Vehicle);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Radio")
    void ToggleRadio(AActor* Vehicle);

    // Call for passenger entry/exit too; this is listener state, not driver state.
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Radio")
    void SetListenerInside(AActor* Vehicle, bool bInside);

    virtual void Deinitialize() override;

private:
    UPROPERTY(Transient)
    TArray<FVehicleRadioEntry> Radios;

    FTimerHandle ToneTimer;
    double LastToneTime = 0.0;

    int32 FindRadio(AActor* Vehicle) const;
    void SelectTrack(AActor* Vehicle, int32 Offset);
    void PlaySelected(FVehicleRadioEntry& Entry);
    void UpdateTone();

    UFUNCTION()
    void OnVehicleEndPlay(AActor* Vehicle, EEndPlayReason::Type Reason);
};
