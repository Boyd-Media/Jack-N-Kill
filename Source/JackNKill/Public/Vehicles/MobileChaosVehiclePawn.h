#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "MobileChaosVehiclePawn.generated.h"

class UInputAction;
class UInputMappingContext;
class USoundBase;
class APlayerController;

UCLASS()
class JACKNKILL_API AMobileChaosVehiclePawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()
public:
    AMobileChaosVehiclePawn();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Radio")
    TArray<TObjectPtr<USoundBase>> RadioPlaylist;

    UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputMappingContext> VehicleInputMapping;

    UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> NextTrackAction;

    UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> PreviousTrackAction;

    UPROPERTY(EditDefaultsOnly, Category = "Vehicle|Input")
    TObjectPtr<UInputAction> ToggleRadioAction;

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Radio")
    void NextRadioTrack();

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Radio")
    void PreviousRadioTrack();

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Radio")
    void ToggleVehicleRadio();

    // Call from the possessing controller's Enhanced Input/touch bindings.
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Input")
    void SetDriveInputs(float Throttle, float Steering, float Brake, bool bHandbrake);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Input")
    void ClearDriveInputs();

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void PawnClientRestart() override;
    virtual void OnRep_Controller() override;
    virtual void UnPossessed() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TWeakObjectPtr<APlayerController> LocalRadioDriver;
    void SetLocalRadioDriver(APlayerController* Player);
    void RefreshLocalRadioDriver();
};
