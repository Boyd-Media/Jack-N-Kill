#include "Vehicles/MobileChaosVehiclePawn.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Vehicles/MobileVehicleWheels.h"

AMobileChaosVehiclePawn::AMobileChaosVehiclePawn()
{
    // Movement/physics have their own ticks; no extra pawn Tick is needed.
    PrimaryActorTick.bCanEverTick = false;
    GetMesh()->SetCollisionProfileName(TEXT("Vehicle"));
    GetMesh()->SetSimulatePhysics(true);

    UChaosWheeledVehicleMovementComponent* Movement =
        CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
    Movement->Mass = 1400.0f;
    Movement->DragCoefficient = 0.30f;
    Movement->bEnableCenterOfMassOverride = true;
    Movement->CenterOfMassOverride = FVector(0.0f, 0.0f, -15.0f);
    Movement->DifferentialSetup.DifferentialType = EVehicleDifferential::RearWheelDrive;
    Movement->EngineSetup.MaxTorque = 350.0f;
    Movement->EngineSetup.MaxRPM = 6000.0f;
    Movement->EngineSetup.EngineIdleRPM = 900.0f;
    FRichCurve* Torque = Movement->EngineSetup.TorqueCurve.GetRichCurve();
    Torque->Reset();
    Torque->AddKey(0.0f, 0.25f);
    Torque->AddKey(1500.0f, 0.75f);
    Torque->AddKey(3500.0f, 1.0f);
    Torque->AddKey(6000.0f, 0.65f);
    Movement->TransmissionSetup.bUseAutomaticGears = true;
    Movement->TransmissionSetup.ForwardGearRatios = {3.20f, 2.10f, 1.50f, 1.10f, 0.85f};
    Movement->TransmissionSetup.ReverseGearRatios = {3.00f};
    Movement->TransmissionSetup.FinalRatio = 3.42f;

    Movement->WheelSetups.SetNum(4);
    const FName Bones[] = {TEXT("Wheel_FL"), TEXT("Wheel_FR"), TEXT("Wheel_RL"), TEXT("Wheel_RR")};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        FChaosWheelSetup& Setup = Movement->WheelSetups[Index];
        Setup.BoneName = Bones[Index];
        Setup.WheelClass = Index < 2 ? UMobileFrontWheel::StaticClass() : UMobileRearWheel::StaticClass();
        Setup.AdditionalOffset = FVector::ZeroVector;
    }
}

void AMobileChaosVehiclePawn::SetDriveInputs(float Throttle, float Steering,
    float Brake, bool bHandbrake)
{
    UChaosVehicleMovementComponent* Movement = GetVehicleMovementComponent();
    Movement->SetThrottleInput(FMath::IsFinite(Throttle) ? FMath::Clamp(Throttle, 0.0f, 1.0f) : 0.0f);
    Movement->SetSteeringInput(FMath::IsFinite(Steering) ? FMath::Clamp(Steering, -1.0f, 1.0f) : 0.0f);
    Movement->SetBrakeInput(FMath::IsFinite(Brake) ? FMath::Clamp(Brake, 0.0f, 1.0f) : 0.0f);
    Movement->SetHandbrakeInput(bHandbrake);
}

void AMobileChaosVehiclePawn::ClearDriveInputs()
{
    SetDriveInputs(0.0f, 0.0f, 0.0f, false);
}

void AMobileChaosVehiclePawn::UnPossessed()
{
    ClearDriveInputs();
    Super::UnPossessed();
}
