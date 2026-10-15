#include "AI/MobileAIDirector.h"

#include "AI/StalkerAIController.h"
#include "Curves/CurveFloat.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "World/StormWeatherController.h"
#include "Systems/GameplayRules.h"

AMobileAIDirector::AMobileAIDirector()
{
    PrimaryActorTick.bCanEverTick = false;
    Agents.Reserve(8);
    Players.Reserve(4);
}

bool AMobileAIDirector::IsDirectorAuthority() const
{
    return GetWorld() && GetNetMode() != NM_Client && HasAuthority();
}

void AMobileAIDirector::BeginPlay()
{
    Super::BeginPlay();
    if (!IsDirectorAuthority())
    {
        return;
    }
    for (TActorIterator<AMobileAIDirector> It(GetWorld()); It; ++It)
    {
        if (*It != this && It->bOwnsAdmission)
        {
            UE_LOG(LogTemp, Error, TEXT("Duplicate AI Director disabled; use one admission owner per world."));
            return;
        }
    }
    bOwnsAdmission = true;
    CycleStartTime = GetWorld()->GetTimeSeconds();
    NextSpawnTime = CycleStartTime + 30.0;
    if (IsValid(Weather))
    {
        Weather->OnStormChanged.AddDynamic(this, &AMobileAIDirector::HandleStormChanged);
    }
    GetWorld()->GetTimerManager().SetTimer(DecisionTimer, this, &AMobileAIDirector::Evaluate, 2.0f, true);
}

void AMobileAIDirector::UpdatePlayerStatus(APawn* Player, float Health, float Ammo)
{
    if (!IsDirectorAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld()
        || !FMath::IsFinite(Health) || !FMath::IsFinite(Ammo))
    {
        return;
    }
    FPlayerStatus* Status = Players.FindByPredicate([Player](const FPlayerStatus& Entry)
    {
        return Entry.Player.Get() == Player;
    });
    if (!Status)
    {
        FPlayerStatus NewStatus;
        NewStatus.Player = Player;
        NewStatus.LastEncounter = GetWorld()->GetTimeSeconds();
        Status = &Players.Add_GetRef(NewStatus);
    }
    Status->Health = FMath::Clamp(Health, 0.0f, 1.0f);
    Status->Ammo = FMath::Clamp(Ammo, 0.0f, 1.0f);
    Status->LastTelemetry = GetWorld()->GetTimeSeconds();
}

void AMobileAIDirector::NotifyEncounterStarted(APawn* Player)
{
    if (IsDirectorAuthority())
    {
        for (FPlayerStatus& Status : Players)
        {
            if (Status.Player.Get() == Player)
            {
                Status.LastEncounter = GetWorld()->GetTimeSeconds();
            }
        }
    }
}

void AMobileAIDirector::RemovePlayerStatus(APawn* Player)
{
    if (IsDirectorAuthority())
    {
        const TWeakObjectPtr<APawn> Key(Player);
        Players.RemoveAll([Key](const FPlayerStatus& Status) { return Status.Player == Key; });
    }
}

FGuid AMobileAIDirector::RequestAgentSlot(EDirectorAgentKind Kind)
{
    const TArray<FGuid> Handles = RequestAgentSlots(Kind, 1);
    return Handles.IsEmpty() ? FGuid() : Handles[0];
}

void AMobileAIDirector::PruneAgentSlots()
{
    const double Now = GetWorld()->GetTimeSeconds();
    Agents.RemoveAll([Now](const FAgentSlot& Entry)
    {
        return Entry.bReserved ? Now >= Entry.ReservationExpiresAt : !Entry.Agent.IsValid();
    });
}

TArray<FGuid> AMobileAIDirector::RequestAgentSlots(EDirectorAgentKind Kind, int32 Count)
{
    TArray<FGuid> Handles;
    if (!IsDirectorAuthority() || !bOwnsAdmission
        || (Kind != EDirectorAgentKind::AlienStalker && Kind != EDirectorAgentKind::PublicOrderPatrol
            && Kind != EDirectorAgentKind::OtherHuman))
    {
        return Handles;
    }
    PruneAgentSlots();
    int32 Humans = 0;
    for (const FAgentSlot& Entry : Agents)
    {
        Humans += Entry.Kind != EDirectorAgentKind::AlienStalker ? 1 : 0;
    }
    if (!JNK::Rules::CanReserveAgents(Agents.Num(), Humans, Agents.Num() - Humans, Count,
        Kind == EDirectorAgentKind::AlienStalker, FMath::Clamp(MaxAlienAgents, 1, 2)))
    {
        return Handles;
    }
    Handles.Reserve(Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FAgentSlot Entry;
        Entry.Handle = FGuid::NewGuid();
        Entry.Kind = Kind;
        Entry.ReservationExpiresAt = GetWorld()->GetTimeSeconds() + 5.0;
        Agents.Add(Entry);
        Handles.Add(Entry.Handle);
    }
    return Handles;
}

bool AMobileAIDirector::ActivateReservedAgent(FGuid Handle, APawn* Agent)
{
    if (!IsDirectorAuthority() || !IsValid(Agent) || Agent->GetWorld() != GetWorld())
    {
        return false;
    }
    FAgentSlot* Slot = Agents.FindByPredicate([Handle](const FAgentSlot& Entry) { return Entry.Handle == Handle; });
    if (!Slot || !Slot->bReserved || GetWorld()->GetTimeSeconds() >= Slot->ReservationExpiresAt
        || Agents.ContainsByPredicate([Agent](const FAgentSlot& Entry) { return Entry.Agent.Get() == Agent; }))
    {
        return false;
    }
    Slot->Agent = Agent;
    Slot->bReserved = false;
    Agent->OnEndPlay.AddDynamic(this, &AMobileAIDirector::HandleAgentEndPlay);
    if (Slot->Kind == EDirectorAgentKind::AlienStalker)
    {
        ApplyAgentWeather(Agent);
    }
    return true;
}

void AMobileAIDirector::ReleaseAgentSlot(FGuid Handle)
{
    if (!IsDirectorAuthority())
    {
        return;
    }
    const int32 Index = Agents.IndexOfByPredicate([Handle](const FAgentSlot& Entry) { return Entry.Handle == Handle; });
    if (Index != INDEX_NONE)
    {
        if (Agents[Index].Agent.IsValid() && Agents[Index].Agent->GetController())
        {
            UE_LOG(LogTemp, Warning, TEXT("AI slot release rejected: deactivate/unpossess the agent first."));
            return;
        }
        if (Agents[Index].Agent.IsValid())
        {
            Agents[Index].Agent->OnEndPlay.RemoveDynamic(this, &AMobileAIDirector::HandleAgentEndPlay);
        }
        Agents.RemoveAtSwap(Index);
    }
}

void AMobileAIDirector::Evaluate()
{
    const double Now = GetWorld()->GetTimeSeconds();
    PruneAgentSlots();
    Players.RemoveAll([](const FPlayerStatus& Status) { return !Status.Player.IsValid(); });
    // Controllers can be created after pawn admission; reapply cheaply so new
    // controllers also receive the current storm without another weather event.
    for (const FAgentSlot& Slot : Agents)
    {
        if (!Slot.bReserved && Slot.Kind == EDirectorAgentKind::AlienStalker && Slot.Agent.IsValid())
        {
            ApplyAgentWeather(Slot.Agent.Get());
        }
    }
    if (Players.IsEmpty())
    {
        PlayerStress = 0.0f;
        DesiredTension = 0.0f;
        return;
    }
    PlayerStress = 0.0f;
    APawn* FocusPlayer = nullptr;
    for (const FPlayerStatus& Status : Players)
    {
        // Stale telemetry is not evidence the player has recovered.
        if (Now - Status.LastTelemetry > 10.0)
        {
            PlayerStress = 1.0f;
            DesiredTension = 0.0f;
            return;
        }
        const float Stress = static_cast<float>(JNK::Rules::Stress(Status.Health, Status.Ammo, Now - Status.LastEncounter));
        if (!FocusPlayer || Stress > PlayerStress)
        {
            PlayerStress = Stress;
            FocusPlayer = Status.Player.Get();
        }
    }
    if (PlayerStress >= 0.75f)
    {
        RecoveryUntil = Now + 20.0;
    }
    if (Now < RecoveryUntil)
    {
        DesiredTension = 0.0f;
        CycleStartTime = Now;
        return;
    }
    const float Phase = static_cast<float>(FMath::Fmod(Now - CycleStartTime, 120.0) / 120.0);
    const float CurveValue = TensionCurve ? TensionCurve->GetFloatValue(Phase)
        : (Phase < 0.65f ? Phase / 0.65f : (1.0f - Phase) / 0.35f);
    DesiredTension = (FMath::IsFinite(CurveValue) ? FMath::Clamp(CurveValue, 0.0f, 1.0f) : 0.0f)
        * (1.0f - PlayerStress);
    if (DesiredTension >= 0.55f && Now >= NextSpawnTime)
    {
        const float AlienChance = IsValid(Weather) && Weather->GetStormIntensity() > 0.7f ? 0.65f : 0.40f;
        const EDirectorAgentKind Kind = FMath::FRand() < AlienChance
            ? EDirectorAgentKind::AlienStalker : EDirectorAgentKind::PublicOrderPatrol;
        const bool bSpawned = SpawnEncounter(Kind, FocusPlayer);
        NextSpawnTime = Now + (bSpawned ? 45.0 : 10.0);
    }
}

bool AMobileAIDirector::SpawnEncounter(EDirectorAgentKind Kind, APawn* FocusPlayer)
{
    TSubclassOf<APawn> Class = Kind == EDirectorAgentKind::AlienStalker ? AlienStalkerClass : PublicOrderPatrolClass;
    if (!Class || SpawnPoints.IsEmpty())
    {
        return false;
    }
    for (int32 Attempt = 0; Attempt < 8; ++Attempt)
    {
        ATargetPoint* Point = SpawnPoints[FMath::RandRange(0, SpawnPoints.Num() - 1)].Get();
        if (!IsValid(Point))
        {
            continue;
        }
        bool bNearSomeone = false;
        bool bTooClose = false;
        for (const FPlayerStatus& Status : Players)
        {
            const double Distance = FVector::DistSquared(Point->GetActorLocation(), Status.Player->GetActorLocation());
            bTooClose |= Distance < FMath::Square(4000.0);
            bNearSomeone |= Distance <= FMath::Square(15000.0);
        }
        if (bTooClose || !bNearSomeone)
        {
            continue;
        }
        const FGuid Handle = RequestAgentSlot(Kind);
        if (!Handle.IsValid())
        {
            return false;
        }
        APawn* Pawn = GetWorld()->SpawnActorDeferred<APawn>(Class, Point->GetActorTransform(),
            this, nullptr, ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
        if (Pawn)
        {
            UGameplayStatics::FinishSpawningActor(Pawn, Point->GetActorTransform());
        }
        if (!IsValid(Pawn) || !ActivateReservedAgent(Handle, Pawn))
        {
            if (IsValid(Pawn))
            {
                Pawn->Destroy();
            }
            ReleaseAgentSlot(Handle);
            continue;
        }
        if (AStalkerAIController* Controller = Cast<AStalkerAIController>(Pawn->GetController()))
        {
            Controller->SetTrackedPlayer(FocusPlayer);
        }
        OnEncounterSpawned.Broadcast(Pawn, Kind);
        return true;
    }
    return false;
}

void AMobileAIDirector::ApplyAgentWeather(APawn* Agent)
{
    if (AStalkerAIController* Controller = Cast<AStalkerAIController>(Agent->GetController()))
    {
        Controller->SetStormAggression(IsValid(Weather) ? Weather->GetStalkerAggression() : 1.0f);
    }
}

void AMobileAIDirector::HandleStormChanged(float Intensity, float NoiseScale, float Aggression)
{
    for (const FAgentSlot& Slot : Agents)
    {
        if (!Slot.bReserved && Slot.Kind == EDirectorAgentKind::AlienStalker && Slot.Agent.IsValid())
        {
            ApplyAgentWeather(Slot.Agent.Get());
        }
    }
}

void AMobileAIDirector::HandleAgentEndPlay(AActor* Agent, EEndPlayReason::Type Reason)
{
    const TWeakObjectPtr<APawn> Key(Cast<APawn>(Agent));
    Agents.RemoveAll([Key](const FAgentSlot& Slot) { return Slot.Agent == Key; });
}

void AMobileAIDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bOwnsAdmission = false;
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(DecisionTimer);
    }
    if (IsValid(Weather))
    {
        Weather->OnStormChanged.RemoveDynamic(this, &AMobileAIDirector::HandleStormChanged);
    }
    for (const FAgentSlot& Slot : Agents)
    {
        if (Slot.Agent.IsValid())
        {
            Slot.Agent->OnEndPlay.RemoveDynamic(this, &AMobileAIDirector::HandleAgentEndPlay);
        }
    }
    Super::EndPlay(EndPlayReason);
}
