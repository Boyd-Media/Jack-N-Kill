#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "MobileVehicleWheels.generated.h"

UCLASS()
class JACKNKILL_API UMobileFrontWheel : public UChaosVehicleWheel
{
    GENERATED_BODY()
public:
    UMobileFrontWheel();
};

UCLASS()
class JACKNKILL_API UMobileRearWheel : public UChaosVehicleWheel
{
    GENERATED_BODY()
public:
    UMobileRearWheel();
};
