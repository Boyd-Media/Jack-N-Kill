#include "Audio/VehicleRadioSubsystem.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

int32 UVehicleRadioSubsystem::FindRadio(AActor* Vehicle) const
{
    const TWeakObjectPtr<AActor> Key(Vehicle);
    return Radios.IndexOfByPredicate([Key](const FVehicleRadioEntry& Entry)
    {
        return Entry.Vehicle == Key;
    });
}

bool UVehicleRadioSubsystem::RegisterVehicle(AActor* Vehicle, const TArray<USoundBase*>& Playlist)
{
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer
        || !IsValid(Vehicle) || Vehicle->GetWorld() != GetWorld()
        || !Vehicle->GetRootComponent())
    {
        return false;
    }
    if (FindRadio(Vehicle) != INDEX_NONE)
    {
        return true;
    }
    // A separate local audio budget; ambient ISM cars do not register radios.
    if (Radios.Num() >= 8)
    {
        return false;
    }
    FVehicleRadioEntry Entry;
    Entry.Vehicle = Vehicle;
    for (USoundBase* Track : Playlist)
    {
        if (IsValid(Track))
        {
            Entry.Tracks.Add(Track);
        }
    }
    if (Entry.Tracks.IsEmpty())
    {
        return false;
    }
    Entry.Audio = NewObject<UAudioComponent>(Vehicle);
    Vehicle->AddInstanceComponent(Entry.Audio);
    Entry.Audio->bAutoActivate = false;
    Entry.Audio->bAutoDestroy = false;
    Entry.Audio->bStopWhenOwnerDestroyed = true;
    Entry.Audio->bAllowSpatialization = true;
    Entry.Audio->bIsUISound = false;
    Entry.Audio->SetupAttachment(Vehicle->GetRootComponent());
    Entry.Audio->bOverrideAttenuation = true;
    FSoundAttenuationSettings& Settings = Entry.Audio->AttenuationOverrides;
    Settings.bAttenuate = true;
    Settings.bSpatialize = true;
    Settings.bEnableOcclusion = false;
    Settings.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
    Settings.AttenuationShape = EAttenuationShape::Sphere;
    Settings.AttenuationShapeExtents = FVector(300.0f, 0.0f, 0.0f);
    Settings.FalloffDistance = 1700.0f;
    Entry.Audio->RegisterComponent();
    Entry.Audio->SetLowPassFilterEnabled(true);
    Entry.Audio->SetLowPassFilterFrequency(Entry.CutoffHz);
    Entry.Audio->SetVolumeMultiplier(Entry.Volume);
    Radios.Add(MoveTemp(Entry));
    Vehicle->OnEndPlay.AddDynamic(this, &UVehicleRadioSubsystem::OnVehicleEndPlay);
    return true;
}

void UVehicleRadioSubsystem::UnregisterVehicle(AActor* Vehicle)
{
    const int32 Index = FindRadio(Vehicle);
    if (Index == INDEX_NONE)
    {
        return;
    }
    FVehicleRadioEntry& Entry = Radios[Index];
    if (IsValid(Entry.Audio))
    {
        Entry.Audio->Stop();
        Entry.Audio->DestroyComponent();
    }
    if (IsValid(Vehicle))
    {
        Vehicle->OnEndPlay.RemoveDynamic(this, &UVehicleRadioSubsystem::OnVehicleEndPlay);
    }
    Radios.RemoveAtSwap(Index);
    if (Radios.IsEmpty() && GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ToneTimer);
    }
}

void UVehicleRadioSubsystem::PlaySelected(FVehicleRadioEntry& Entry)
{
    if (!IsValid(Entry.Audio) || Entry.Tracks.IsEmpty())
    {
        return;
    }
    Entry.Audio->Stop();
    Entry.Audio->SetSound(Entry.Tracks[Entry.TrackIndex]);
    if (Entry.bEnabled)
    {
        Entry.Audio->Play();
    }
}

void UVehicleRadioSubsystem::SelectTrack(AActor* Vehicle, int32 Offset)
{
    const int32 Index = FindRadio(Vehicle);
    if (Index == INDEX_NONE)
    {
        return;
    }
    FVehicleRadioEntry& Entry = Radios[Index];
    const int32 Count = Entry.Tracks.Num();
    Entry.TrackIndex = (Entry.TrackIndex + Offset + Count) % Count;
    PlaySelected(Entry);
}

void UVehicleRadioSubsystem::NextTrack(AActor* Vehicle)
{
    SelectTrack(Vehicle, 1);
}

void UVehicleRadioSubsystem::PreviousTrack(AActor* Vehicle)
{
    SelectTrack(Vehicle, -1);
}

void UVehicleRadioSubsystem::ToggleRadio(AActor* Vehicle)
{
    const int32 Index = FindRadio(Vehicle);
    if (Index != INDEX_NONE)
    {
        FVehicleRadioEntry& Entry = Radios[Index];
        Entry.bEnabled = !Entry.bEnabled;
        PlaySelected(Entry);
    }
}

void UVehicleRadioSubsystem::SetListenerInside(AActor* Vehicle, bool bInside)
{
    const int32 Index = FindRadio(Vehicle);
    if (Index == INDEX_NONE || !GetWorld())
    {
        return;
    }
    Radios[Index].bInside = bInside;
    if (!GetWorld()->GetTimerManager().IsTimerActive(ToneTimer))
    {
        LastToneTime = GetWorld()->GetTimeSeconds();
        GetWorld()->GetTimerManager().SetTimer(ToneTimer, this,
            &UVehicleRadioSubsystem::UpdateTone, 0.05f, true);
    }
}

void UVehicleRadioSubsystem::UpdateTone()
{
    const double Now = GetWorld()->GetTimeSeconds();
    const float Delta = static_cast<float>(Now - LastToneTime);
    LastToneTime = Now;
    bool bStillBlending = false;
    for (FVehicleRadioEntry& Entry : Radios)
    {
        if (!IsValid(Entry.Audio))
        {
            continue;
        }
        const float TargetCutoff = Entry.bInside ? 20000.0f : 1800.0f;
        const float TargetVolume = Entry.bInside ? 0.55f : 0.30f;
        const float Alpha = 1.0f - FMath::Exp(-12.0f * Delta);
        Entry.CutoffHz = FMath::Lerp(Entry.CutoffHz, TargetCutoff, Alpha);
        Entry.Volume = FMath::Lerp(Entry.Volume, TargetVolume, Alpha);
        const bool bSettled = FMath::Abs(Entry.CutoffHz - TargetCutoff) < 5.0f
            && FMath::Abs(Entry.Volume - TargetVolume) < 0.002f;
        if (bSettled)
        {
            Entry.CutoffHz = TargetCutoff;
            Entry.Volume = TargetVolume;
        }
        else
        {
            bStillBlending = true;
        }
        Entry.Audio->SetLowPassFilterFrequency(Entry.CutoffHz);
        Entry.Audio->SetVolumeMultiplier(Entry.Volume);
    }
    if (!bStillBlending)
    {
        GetWorld()->GetTimerManager().ClearTimer(ToneTimer);
    }
}

void UVehicleRadioSubsystem::OnVehicleEndPlay(AActor* Vehicle, EEndPlayReason::Type Reason)
{
    UnregisterVehicle(Vehicle);
}

void UVehicleRadioSubsystem::Deinitialize()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ToneTimer);
    }
    for (FVehicleRadioEntry& Entry : Radios)
    {
        if (Entry.Vehicle.IsValid())
        {
            Entry.Vehicle->OnEndPlay.RemoveDynamic(this, &UVehicleRadioSubsystem::OnVehicleEndPlay);
        }
        if (IsValid(Entry.Audio))
        {
            Entry.Audio->Stop();
            Entry.Audio->DestroyComponent();
        }
    }
    Radios.Reset();
    Super::Deinitialize();
}
