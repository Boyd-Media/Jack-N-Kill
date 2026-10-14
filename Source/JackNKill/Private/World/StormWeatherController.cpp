#include "World/StormWeatherController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Hearing.h"

AStormWeatherController::AStormWeatherController()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    NetUpdateFrequency = 2.0f;
}

void AStormWeatherController::BeginPlay()
{
    Super::BeginPlay();
    OnRep_StormIntensity();
}

float AStormWeatherController::GetFootstepRadiusScale() const
{
    const float Severe = FMath::Clamp((StormIntensity - 0.7f) / 0.3f, 0.0f, 1.0f);
    return FMath::Lerp(1.0f, 0.5f, Severe);
}

float AStormWeatherController::GetStalkerAggression() const
{
    const float Severe = FMath::Clamp((StormIntensity - 0.7f) / 0.3f, 0.0f, 1.0f);
    return FMath::Lerp(1.0f, 1.5f, Severe);
}

void AStormWeatherController::SetStormIntensity(float Intensity)
{
    if (GetNetMode() == NM_Client || !HasAuthority() || !FMath::IsFinite(Intensity))
    {
        return;
    }
    const float NewIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    if (NewIntensity != StormIntensity)
    {
        StormIntensity = NewIntensity;
        OnRep_StormIntensity();
        ForceNetUpdate();
    }
}

void AStormWeatherController::OnRep_StormIntensity()
{
    OnStormChanged.Broadcast(StormIntensity, GetFootstepRadiusScale(), GetStalkerAggression());
}

void AStormWeatherController::ReportPlayerFootstep(APawn* Player, FVector Location,
    float BaseRadiusCm, float Loudness)
{
    if (GetNetMode() == NM_Client || !HasAuthority() || !IsValid(Player)
        || Player->GetWorld() != GetWorld() || Location.ContainsNaN()
        || !FMath::IsFinite(BaseRadiusCm) || BaseRadiusCm <= 0.0f
        || !FMath::IsFinite(Loudness) || Loudness <= 0.0f)
    {
        return;
    }
    UAISense_Hearing::ReportNoiseEvent(GetWorld(), Location, FMath::Clamp(Loudness, 0.01f, 1.0f),
        Player, BaseRadiusCm * GetFootstepRadiusScale(), TEXT("Footstep"));
}

void AStormWeatherController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AStormWeatherController, StormIntensity);
}
