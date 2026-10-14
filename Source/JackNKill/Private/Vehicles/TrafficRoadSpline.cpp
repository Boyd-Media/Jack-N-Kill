#include "Vehicles/TrafficRoadSpline.h"
#include "Components/SplineComponent.h"
#include "Vehicles/TrafficManager.h"

ATrafficRoadSpline::ATrafficRoadSpline()
{
    PrimaryActorTick.bCanEverTick = false;
    Spline = CreateDefaultSubobject<USplineComponent>(TEXT("RoadLane"));
    SetRootComponent(Spline);
}

void ATrafficRoadSpline::BeginPlay()
{
    Super::BeginPlay();
    if (ATrafficManager* LoadedManager = Manager.Get())
    {
        LoadedManager->RegisterRoad(this);
    }
}

void ATrafficRoadSpline::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ATrafficManager* LoadedManager = Manager.Get())
    {
        LoadedManager->UnregisterRoad(this);
    }
    Super::EndPlay(EndPlayReason);
}
