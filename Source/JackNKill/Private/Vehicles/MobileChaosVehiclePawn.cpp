#include "Vehicles/MobileChaosVehiclePawn.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Vehicles/MobileVehicleWheels.h"
#include "Audio/VehicleRadioSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Sound/SoundBase.h"

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
    SetLocalRadioDriver(nullptr);
    Super::UnPossessed();
}

void AMobileChaosVehiclePawn::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() != NM_DedicatedServer && !RadioPlaylist.IsEmpty())
    {
        TArray<USoundBase*> Tracks;
        for (const TObjectPtr<USoundBase>& Track : RadioPlaylist)
        {
            Tracks.Add(Track.Get());
        }
        if (!GetWorld()->GetSubsystem<UVehicleRadioSubsystem>()->RegisterVehicle(this, Tracks))
        {
            UE_LOG(LogTemp, Warning, TEXT("Vehicle radio registration failed: empty playlist or local radio budget reached."));
        }
    }
    RefreshLocalRadioDriver();
}

void AMobileChaosVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (NextTrackAction)
        {
            Input->BindAction(NextTrackAction, ETriggerEvent::Started, this, &AMobileChaosVehiclePawn::NextRadioTrack);
        }
        if (PreviousTrackAction)
        {
            Input->BindAction(PreviousTrackAction, ETriggerEvent::Started, this, &AMobileChaosVehiclePawn::PreviousRadioTrack);
        }
        if (ToggleRadioAction)
        {
            Input->BindAction(ToggleRadioAction, ETriggerEvent::Started, this, &AMobileChaosVehiclePawn::ToggleVehicleRadio);
        }
    }
}

void AMobileChaosVehiclePawn::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    RefreshLocalRadioDriver();
}

void AMobileChaosVehiclePawn::PawnClientRestart()
{
    Super::PawnClientRestart();
    RefreshLocalRadioDriver();
}

void AMobileChaosVehiclePawn::OnRep_Controller()
{
    Super::OnRep_Controller();
    RefreshLocalRadioDriver();
}

void AMobileChaosVehiclePawn::RefreshLocalRadioDriver()
{
    APlayerController* Player = Cast<APlayerController>(GetController());
    SetLocalRadioDriver(Player && Player->IsLocalController() ? Player : nullptr);
}

void AMobileChaosVehiclePawn::SetLocalRadioDriver(APlayerController* Player)
{
    if (LocalRadioDriver.Get() != Player)
    {
        if (LocalRadioDriver.IsValid() && VehicleInputMapping)
        {
            if (ULocalPlayer* Local = LocalRadioDriver->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Input = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                {
                    Input->RemoveMappingContext(VehicleInputMapping);
                }
            }
        }
        LocalRadioDriver = Player;
        if (Player && VehicleInputMapping)
        {
            if (ULocalPlayer* Local = Player->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Input = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                {
                    Input->AddMappingContext(VehicleInputMapping, 1);
                }
            }
        }
    }
    if (GetWorld())
    {
        GetWorld()->GetSubsystem<UVehicleRadioSubsystem>()->SetListenerInside(this, Player != nullptr);
    }
}

void AMobileChaosVehiclePawn::NextRadioTrack()
{
    if (GetWorld())
    {
        GetWorld()->GetSubsystem<UVehicleRadioSubsystem>()->NextTrack(this);
    }
}

void AMobileChaosVehiclePawn::PreviousRadioTrack()
{
    if (GetWorld())
    {
        GetWorld()->GetSubsystem<UVehicleRadioSubsystem>()->PreviousTrack(this);
    }
}

void AMobileChaosVehiclePawn::ToggleVehicleRadio()
{
    if (GetWorld())
    {
        GetWorld()->GetSubsystem<UVehicleRadioSubsystem>()->ToggleRadio(this);
    }
}

void AMobileChaosVehiclePawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    SetLocalRadioDriver(nullptr);
    Super::EndPlay(EndPlayReason);
}
