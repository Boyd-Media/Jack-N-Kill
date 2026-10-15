#include "Vehicles/TrafficManager.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Vehicles/TrafficRoadSpline.h"

ATrafficManager::ATrafficManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.10f;
    TrafficInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("AmbientCars"));
    SetRootComponent(TrafficInstances);
    TrafficInstances->SetMobility(EComponentMobility::Movable);
    TrafficInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TrafficInstances->SetGenerateOverlapEvents(false);
    TrafficInstances->SetCanEverAffectNavigation(false);
    TrafficInstances->SetCastShadow(false);
}

void ATrafficManager::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer)
    {
        SetActorTickEnabled(false);
        return;
    }
    MaxVehicles = FMath::Clamp(MaxVehicles, 1, 256);
    SpawnRadiusCm = FMath::IsFinite(SpawnRadiusCm) ? FMath::Max(1.0f, SpawnRadiusCm) : 15000.0f;
    DespawnRadiusCm = FMath::IsFinite(DespawnRadiusCm)
        ? FMath::Max(SpawnRadiusCm + 1.0f, DespawnRadiusCm) : SpawnRadiusCm + 10000.0f;
    MinimumSpacingCm = FMath::IsFinite(MinimumSpacingCm) ? FMath::Max(1.0f, MinimumSpacingCm) : 1500.0f;
    Random.Initialize(RandomSeed);
    Cars.Reserve(MaxVehicles);
    InstanceTransforms.Reserve(MaxVehicles);
    TrafficInstances->SetStaticMesh(VehicleMesh);
    SetTrackedPlayer(UGameplayStatics::GetPlayerPawn(this, 0));
}

void ATrafficManager::SetTrackedPlayer(APawn* Player)
{
    if (TrackedPlayer.Get() == Player)
    {
        return;
    }
    TrackedPlayer = Player;
    ClearTraffic();
}

void ATrafficManager::RegisterRoad(ATrafficRoadSpline* Road)
{
    if (IsValid(Road))
    {
        Roads.AddUnique(TSoftObjectPtr<ATrafficRoadSpline>(Road));
    }
}

void ATrafficManager::UnregisterRoad(ATrafficRoadSpline* Road)
{
    Roads.Remove(TSoftObjectPtr<ATrafficRoadSpline>(Road));
    Cars.RemoveAll([Road](const FAmbientCar& Car) { return Car.Road.Get() == Road; });
    bInstancesNeedRebuild = true;
}

void ATrafficManager::ClearTraffic()
{
    Cars.Reset();
    InstanceTransforms.Reset();
    TrafficInstances->ClearInstances();
    SpawnElapsed = 0.0f;
    bInstancesNeedRebuild = false;
}

FTransform ATrafficManager::CarTransform(const FAmbientCar& Car) const
{
    const ATrafficRoadSpline* Road = Car.Road.Get();
    FTransform Transform = Road->Spline->GetTransformAtDistanceAlongSpline(
        Car.DistanceCm, ESplineCoordinateSpace::World, false);
    Transform.SetRotation(Transform.GetRotation() * MeshForwardCorrection.Quaternion());
    Transform.AddToTranslation(FVector(0.0f, 0.0f, MeshHeightOffsetCm));
    Transform.SetScale3D(FVector::OneVector);
    return Transform;
}

bool ATrafficManager::TrySpawn(const FVector& PlayerLocation)
{
    if (Roads.IsEmpty() || Cars.Num() >= MaxVehicles)
    {
        return false;
    }
    // Bound sampling cost; find a nearby spline position rather than sampling an
    // entire multi-kilometre road uniformly.
    for (int32 Attempt = 0; Attempt < 12; ++Attempt)
    {
        ATrafficRoadSpline* Road = Roads[Random.RandRange(0, Roads.Num() - 1)].Get();
        if (!IsValid(Road) || !IsValid(Road->Spline)
            || !FMath::IsFinite(Road->SpeedCmPerSecond) || Road->SpeedCmPerSecond <= 0.0f)
        {
            continue;
        }
        const float Length = Road->Spline->GetSplineLength();
        if (Length <= 1.0f)
        {
            continue;
        }
        const float Key = Road->Spline->FindInputKeyClosestToWorldLocation(PlayerLocation);
        float Distance = Road->Spline->GetDistanceAlongSplineAtSplineInputKey(Key)
            + Random.FRandRange(-SpawnRadiusCm, SpawnRadiusCm);
        Distance = Road->Spline->IsClosedLoop()
            ? FMath::Fmod(FMath::Fmod(Distance, Length) + Length, Length)
            : FMath::Clamp(Distance, 0.0f, Length);
        FAmbientCar Candidate;
        Candidate.Road = Road;
        Candidate.DistanceCm = Distance;
        Candidate.SpeedCmPerSecond = Road->SpeedCmPerSecond * Random.FRandRange(0.9f, 1.1f);
        Candidate.WorldTransform = CarTransform(Candidate);
        const FVector Position = Candidate.WorldTransform.GetLocation();
        if (FVector::DistSquared(Position, PlayerLocation) > FMath::Square(SpawnRadiusCm))
        {
            continue;
        }
        bool bCrowded = false;
        for (const FAmbientCar& Other : Cars)
        {
            if (Other.Road.IsValid() && FVector::DistSquared(Position,
                Other.WorldTransform.GetLocation()) < FMath::Square(MinimumSpacingCm))
            {
                bCrowded = true;
                break;
            }
        }
        if (!bCrowded)
        {
            Cars.Add(Candidate);
            bInstancesNeedRebuild = true;
            return true;
        }
    }
    return false;
}

void ATrafficManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!TrackedPlayer.IsValid() || !TrafficInstances->GetStaticMesh())
    {
        if (!Cars.IsEmpty() || TrafficInstances->GetInstanceCount() > 0)
        {
            ClearTraffic();
        }
        return;
    }
    const FVector PlayerLocation = TrackedPlayer->GetActorLocation();
    // Soft references do not force World Partition road actors to stay loaded.
    Roads.RemoveAll([](const TSoftObjectPtr<ATrafficRoadSpline>& Road) { return Road.IsNull(); });
    for (int32 Index = Cars.Num() - 1; Index >= 0; --Index)
    {
        FAmbientCar& Car = Cars[Index];
        const ATrafficRoadSpline* Road = Car.Road.Get();
        bool bRemove = !IsValid(Road) || !IsValid(Road->Spline);
        if (!bRemove)
        {
            const float Length = Road->Spline->GetSplineLength();
            Car.DistanceCm += Car.SpeedCmPerSecond * DeltaSeconds;
            if (Length <= 1.0f || (!Road->Spline->IsClosedLoop() && Car.DistanceCm >= Length))
            {
                bRemove = true;
            }
            else
            {
                Car.DistanceCm = FMath::Fmod(Car.DistanceCm, Length);
                Car.WorldTransform = CarTransform(Car);
                bRemove = FVector::DistSquared(Car.WorldTransform.GetLocation(), PlayerLocation)
                    > FMath::Square(DespawnRadiusCm);
            }
        }
        if (bRemove)
        {
            Cars.RemoveAtSwap(Index);
            bInstancesNeedRebuild = true;
        }
    }
    SpawnElapsed += DeltaSeconds;
    if (SpawnElapsed >= 0.5f)
    {
        SpawnElapsed = 0.0f;
        for (int32 Spawn = 0; Spawn < 4 && Cars.Num() < MaxVehicles; ++Spawn)
        {
            TrySpawn(PlayerLocation);
        }
    }
    InstanceTransforms.Reset();
    for (const FAmbientCar& Car : Cars)
    {
        InstanceTransforms.Add(Car.WorldTransform);
    }
    // Rebuild once after structural changes. Avoid relying on engine-specific
    // ISM RemoveInstance shifting/swap semantics for agent-to-instance mapping.
    if (bInstancesNeedRebuild)
    {
        TrafficInstances->ClearInstances();
        if (!InstanceTransforms.IsEmpty())
        {
            TrafficInstances->AddInstances(InstanceTransforms, false, true);
        }
        bInstancesNeedRebuild = false;
    }
    else if (!InstanceTransforms.IsEmpty())
    {
        TrafficInstances->BatchUpdateInstancesTransforms(0, InstanceTransforms, true, true, true);
    }
}

void ATrafficManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearTraffic();
    Super::EndPlay(EndPlayReason);
}
