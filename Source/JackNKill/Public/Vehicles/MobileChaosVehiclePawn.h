#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "MobileChaosVehiclePawn.generated.h"

UCLASS()
class JACKNKILL_API AMobileChaosVehiclePawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()
public:
    AMobileChaosVehiclePawn();

    // Call from the possessing controller's Enhanced Input/touch bindings.
    UFUNCTION(BlueprintCallable, Category = "Vehicle|Input")
    void SetDriveInputs(float Throttle, float Steering, float Brake, bool bHandbrake);

    UFUNCTION(BlueprintCallable, Category = "Vehicle|Input")
    void ClearDriveInputs();

protected:
    virtual void UnPossessed() override;
};
