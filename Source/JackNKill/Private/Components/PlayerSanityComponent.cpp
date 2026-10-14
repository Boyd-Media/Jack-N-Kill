#include "Components/PlayerSanityComponent.h"

#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

UPlayerSanityComponent::UPlayerSanityComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.10f;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UPlayerSanityComponent::InitializePresentation(UPostProcessComponent* PostProcess,
    UMaterialInterface* Material, UAudioComponent* Ambience)
{
    ReleasePresentation();
    PresentationPostProcess = PostProcess;
    AmbienceAudio = Ambience;
    OriginalPitchMultiplier = IsValid(Ambience) ? Ambience->PitchMultiplier : 1.0f;
    SanityMaterial = IsValid(Material) ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
    if (IsValid(PostProcess) && SanityMaterial)
    {
        PostProcess->Settings.AddBlendable(SanityMaterial, 1.0f);
    }
    PublishPresentation();
}

bool UPlayerSanityComponent::IsLookingAtMonster() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!PC || !PC->IsLocalController() || !IsValid(Monster) || !GetWorld())
    {
        return false;
    }
    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    FVector Target;
    FRotator TargetRotation;
    Monster->GetActorEyesViewPoint(Target, TargetRotation);
    const FVector ToMonster = Target - ViewLocation;
    if (!FMath::IsFinite(MaxLookDistance) || MaxLookDistance <= 0.0f
        || !FMath::IsFinite(LookHalfAngleDegrees)
        || ToMonster.SizeSquared() > FMath::Square(MaxLookDistance)
        || FVector::DotProduct(ViewRotation.Vector(), ToMonster.GetSafeNormal())
            < FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(LookHalfAngleDegrees, 1.0f, 89.0f))))
    {
        return false;
    }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(JNKSanityVisibility), false, GetOwner());
    FHitResult Hit;
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit,
        ViewLocation, Target, ECC_Visibility, Params);
    return !bBlocked || Hit.GetActor() == Monster;
}

void UPlayerSanityComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (!Pawn || !Pawn->IsLocallyControlled())
    {
        return;
    }
    const float Drain = FMath::IsFinite(DrainPerSecond) ? FMath::Max(0.0f, DrainPerSecond) : 0.0f;
    const float Recovery = FMath::IsFinite(RecoveryPerSecond) ? FMath::Max(0.0f, RecoveryPerSecond) : 0.0f;
    const float Rate = IsLookingAtMonster() ? -Drain : Recovery;
    const float NewSanity = FMath::Clamp(NormalizedSanity + Rate * DeltaTime, 0.0f, 1.0f);
    if (NewSanity != NormalizedSanity)
    {
        NormalizedSanity = NewSanity;
        PublishPresentation();
    }
}

void UPlayerSanityComponent::PublishPresentation()
{
    if (SanityMaterial)
    {
        SanityMaterial->SetScalarParameterValue(TEXT("Sanity"), NormalizedSanity);
    }
    if (AmbienceAudio.IsValid())
    {
        AmbienceAudio->SetFloatParameter(AudioParameterName, NormalizedSanity);
        AmbienceAudio->SetPitchMultiplier(OriginalPitchMultiplier
            * FMath::Lerp(0.80f, 1.0f, NormalizedSanity));
    }
    OnSanityChanged.Broadcast(NormalizedSanity);
    OnAudioParameter.Broadcast(AudioParameterName, NormalizedSanity);
}

void UPlayerSanityComponent::ReleasePresentation()
{
    if (PresentationPostProcess.IsValid() && SanityMaterial)
    {
        PresentationPostProcess->Settings.RemoveBlendable(SanityMaterial);
    }
    if (AmbienceAudio.IsValid())
    {
        AmbienceAudio->SetPitchMultiplier(OriginalPitchMultiplier);
        AmbienceAudio->SetFloatParameter(AudioParameterName, 1.0f);
    }
    PresentationPostProcess.Reset();
    AmbienceAudio.Reset();
    SanityMaterial = nullptr;
}

void UPlayerSanityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ReleasePresentation();
    Super::EndPlay(EndPlayReason);
}
