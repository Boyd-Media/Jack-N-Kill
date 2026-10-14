#include "Vehicles/MobileVehicleWheels.h"

UMobileFrontWheel::UMobileFrontWheel()
{
    AxleType = EAxleType::Front;
    SweepShape = ESweepShape::Raycast;
    SweepType = ESweepType::SimpleSweep;
    WheelRadius = 32.0f;
    WheelWidth = 20.0f;
    bAffectedBySteering = true;
    bAffectedByEngine = false;
    bAffectedByBrake = true;
    bAffectedByHandbrake = false;
    MaxSteerAngle = 35.0f;
    SuspensionMaxRaise = 8.0f;
    SuspensionMaxDrop = 12.0f;
    SpringRate = 250.0f;
    SuspensionDampingRatio = 0.5f;
}

UMobileRearWheel::UMobileRearWheel()
{
    AxleType = EAxleType::Rear;
    SweepShape = ESweepShape::Raycast;
    SweepType = ESweepType::SimpleSweep;
    WheelRadius = 32.0f;
    WheelWidth = 20.0f;
    bAffectedBySteering = false;
    bAffectedByEngine = true;
    bAffectedByBrake = true;
    bAffectedByHandbrake = true;
    MaxSteerAngle = 0.0f;
    SuspensionMaxRaise = 8.0f;
    SuspensionMaxDrop = 12.0f;
    SpringRate = 250.0f;
    SuspensionDampingRatio = 0.5f;
}
