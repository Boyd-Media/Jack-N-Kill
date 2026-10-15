#include "World/WorldStateManagerSubsystem.h"
#include "Async/Async.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

bool UWorldStateManagerSubsystem::IsAuthority() const
{
    return GetWorld() && GetWorld()->GetNetMode() != NM_Client;
}

bool UWorldStateManagerSubsystem::ValidProfile(const FString& Profile)
{
    if (Profile.IsEmpty() || Profile.Len() > 32)
    {
        return false;
    }
    for (TCHAR Ch : Profile)
    {
        if (!((Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('A') && Ch <= TEXT('Z'))
            || (Ch >= TEXT('0') && Ch <= TEXT('9')) || Ch == TEXT('_')))
        {
            return false;
        }
    }
    return true;
}

bool UWorldStateManagerSubsystem::SetDoorState(FGuid Id, EPersistentDoorState State,
    EPersistentDoorState AuthoredDefault)
{
    if (!IsAuthority() || bLoading || !Id.IsValid() || static_cast<uint8>(State) > 3
        || static_cast<uint8>(AuthoredDefault) > 3)
    {
        return false;
    }
    if (State == AuthoredDefault)
    {
        DoorOverrides.Remove(Id);
        return true;
    }
    if (!DoorOverrides.Contains(Id) && DoorOverrides.Num() >= CompactWorldSave::MaxDoors)
    {
        return false;
    }
    DoorOverrides.Add(Id, State);
    return true;
}

EPersistentDoorState UWorldStateManagerSubsystem::GetDoorState(FGuid Id,
    EPersistentDoorState AuthoredDefault) const
{
    const EPersistentDoorState* State = DoorOverrides.Find(Id);
    return State ? *State : AuthoredDefault;
}

bool UWorldStateManagerSubsystem::MarkHouseCleared(FGuid Id)
{
    if (!IsAuthority() || bLoading || !Id.IsValid()
        || (!ClearedHouses.Contains(Id) && ClearedHouses.Num() >= CompactWorldSave::MaxHouses))
    {
        return false;
    }
    ClearedHouses.Add(Id);
    return true;
}

bool UWorldStateManagerSubsystem::BindPlayerComponents(ULightweightInventoryComponent* Inventory,
    UPlayerMetabolismComponent* Metabolism)
{
    if (!IsAuthority() || !IsValid(Inventory) || !IsValid(Metabolism)
        || Inventory->GetOwner() != Metabolism->GetOwner()
        || Inventory->GetWorld() != GetWorld() || !Inventory->GetOwner()->HasAuthority())
    {
        return false;
    }
    PlayerInventory = Inventory;
    PlayerMetabolism = Metabolism;
    return ApplyPendingPlayer();
}

bool UWorldStateManagerSubsystem::ApplyPendingPlayer()
{
    if (!bPendingPlayerRestore || !PlayerInventory.IsValid() || !PlayerMetabolism.IsValid())
    {
        return true;
    }
    if (!PlayerMetabolism->RestoreSnapshot(LoadedMetabolism)
        || !PlayerInventory->RestoreSnapshot(LoadedInventory))
    {
        return false;
    }
    bPendingPlayerRestore = false;
    LoadedInventory.Reset();
    return true;
}

bool UWorldStateManagerSubsystem::SaveAsync(const FString& Profile)
{
    if (!IsAuthority() || bBusy || bShuttingDown || !ValidProfile(Profile)
        || !PlayerInventory.IsValid() || !PlayerMetabolism.IsValid())
    {
        return false;
    }
    if (ActiveProfile != Profile)
    {
        // Prevent a new generation counter overwriting just one half of an
        // existing profile and then losing to its older higher-generation half.
        if (UGameplayStatics::DoesSaveGameExist(Profile + TEXT("_A"), 0)
            || UGameplayStatics::DoesSaveGameExist(Profile + TEXT("_B"), 0))
        {
            OnSaveCompleted.Broadcast(false, TEXT("Load an existing profile before saving over it."), 0);
            return false;
        }
        ActiveProfile = Profile;
        Generation = 0;
    }
    if (Generation == MAX_uint64)
    {
        return false;
    }
    FCompactWorldSnapshot Snapshot;
    Snapshot.Generation = Generation + 1;
    for (const TPair<FGuid, EPersistentDoorState>& Door : DoorOverrides)
    {
        FDoorOverride Entry;
        Entry.Id = Door.Key;
        Entry.State = Door.Value;
        Snapshot.Doors.Add(Entry);
    }
    Snapshot.ClearedHouses = ClearedHouses.Array();
    Snapshot.Inventory = PlayerInventory->GetSnapshot();
    Snapshot.Metabolism = PlayerMetabolism->GetSnapshot();
    bBusy = true;
    const TWeakObjectPtr<UWorldStateManagerSubsystem> WeakThis(this);
    Async(EAsyncExecution::ThreadPool, [WeakThis, Snapshot = MoveTemp(Snapshot)]() mutable
    {
        TArray<uint8> Bytes;
        const bool bValid = CompactWorldSave::Encode(Snapshot, Bytes);
        const uint64 SavedGeneration = Snapshot.Generation;
        AsyncTask(ENamedThreads::GameThread, [WeakThis, bValid, Bytes = MoveTemp(Bytes), SavedGeneration]() mutable
        {
            if (WeakThis.IsValid() && !WeakThis->bShuttingDown)
            {
                WeakThis->HandleEncodedSave(bValid, MoveTemp(Bytes), SavedGeneration);
            }
        });
    });
    return true;
}

void UWorldStateManagerSubsystem::HandleEncodedSave(bool bValid, TArray<uint8> Bytes, uint64 SavedGeneration)
{
    if (!bValid)
    {
        bBusy = false;
        OnSaveCompleted.Broadcast(false, TEXT("Snapshot validation or payload size limit failed."), 0);
        return;
    }
    PendingSave = Cast<UCompactWorldSaveGame>(UGameplayStatics::CreateSaveGameObject(UCompactWorldSaveGame::StaticClass()));
    PendingSave->Payload = MoveTemp(Bytes);
    TArray<uint8> Serialized;
    if (!UGameplayStatics::SaveGameToMemory(PendingSave, Serialized) || !PendingSave->IsPayloadValid()
        || Serialized.Num() > CompactWorldSave::MaxSerializedBytes)
    {
        PendingSave = nullptr;
        bBusy = false;
        OnSaveCompleted.Broadcast(false, TEXT("Serialized SaveGame exceeded the one-megabyte safety cap."), 0);
        return;
    }
    PendingByteCount = Serialized.Num();
    const FString Slot = ActiveProfile + (SavedGeneration % 2 ? TEXT("_A") : TEXT("_B"));
    UGameplayStatics::AsyncSaveGameToSlot(PendingSave, Slot, 0,
        FAsyncSaveGameToSlotDelegate::CreateUObject(this, &UWorldStateManagerSubsystem::HandleSaveCompleted));
}

void UWorldStateManagerSubsystem::HandleSaveCompleted(const FString& Slot, int32 UserIndex, bool bSuccess)
{
    if (bShuttingDown)
    {
        return;
    }
    if (bSuccess)
    {
        ++Generation;
    }
    PendingSave = nullptr;
    bBusy = false;
    OnSaveCompleted.Broadcast(bSuccess, bSuccess ? TEXT("") : TEXT("Platform save write failed."), PendingByteCount);
}

bool UWorldStateManagerSubsystem::LoadAsync(const FString& Profile)
{
    if (!IsAuthority() || bBusy || bShuttingDown || !ValidProfile(Profile))
    {
        return false;
    }
    LoadingProfile = Profile;
    ReadSlotIndex = 0;
    BestLoaded.Reset();
    bBusy = bLoading = true;
    UGameplayStatics::AsyncLoadGameFromSlot(Profile + TEXT("_A"), 0,
        FAsyncLoadGameFromSlotDelegate::CreateUObject(this, &UWorldStateManagerSubsystem::HandleLoadedGame));
    return true;
}

void UWorldStateManagerSubsystem::HandleLoadedGame(const FString& Slot, int32 UserIndex, USaveGame* Loaded)
{
    if (bShuttingDown)
    {
        return;
    }
    UCompactWorldSaveGame* Save = Cast<UCompactWorldSaveGame>(Loaded);
    if (!Save || !Save->IsPayloadValid())
    {
        HandleDecodedLoad(false, FCompactWorldSnapshot());
        return;
    }
    TArray<uint8> Bytes = Save->Payload;
    const TWeakObjectPtr<UWorldStateManagerSubsystem> WeakThis(this);
    Async(EAsyncExecution::ThreadPool, [WeakThis, Bytes = MoveTemp(Bytes)]() mutable
    {
        FCompactWorldSnapshot Snapshot;
        const bool bValid = CompactWorldSave::Decode(Bytes, Snapshot);
        AsyncTask(ENamedThreads::GameThread, [WeakThis, bValid, Snapshot = MoveTemp(Snapshot)]() mutable
        {
            if (WeakThis.IsValid() && !WeakThis->bShuttingDown)
            {
                WeakThis->HandleDecodedLoad(bValid, MoveTemp(Snapshot));
            }
        });
    });
}

void UWorldStateManagerSubsystem::HandleDecodedLoad(bool bValid, FCompactWorldSnapshot Snapshot)
{
    if (bValid && (!BestLoaded.IsSet() || Snapshot.Generation > BestLoaded.GetValue().Generation))
    {
        BestLoaded = MoveTemp(Snapshot);
    }
    if (ReadSlotIndex++ == 0)
    {
        UGameplayStatics::AsyncLoadGameFromSlot(LoadingProfile + TEXT("_B"), 0,
            FAsyncLoadGameFromSlotDelegate::CreateUObject(this, &UWorldStateManagerSubsystem::HandleLoadedGame));
        return;
    }
    if (!BestLoaded.IsSet())
    {
        bBusy = bLoading = false;
        OnLoadCompleted.Broadcast(false, TEXT("No valid save generation found; current state was preserved."), 0);
        return;
    }
    DoorOverrides.Reset();
    for (const FDoorOverride& Door : BestLoaded.GetValue().Doors)
    {
        DoorOverrides.Add(Door.Id, Door.State);
    }
    ClearedHouses.Reset();
    for (const FGuid& Id : BestLoaded.GetValue().ClearedHouses)
    {
        ClearedHouses.Add(Id);
    }
    LoadedInventory = MoveTemp(BestLoaded.GetValue().Inventory);
    LoadedMetabolism = BestLoaded.GetValue().Metabolism;
    Generation = BestLoaded.GetValue().Generation;
    ActiveProfile = LoadingProfile;
    BestLoaded.Reset();
    bPendingPlayerRestore = true;
    const bool bApplied = ApplyPendingPlayer();
    bBusy = bLoading = false;
    OnWorldStateRestored.Broadcast();
    OnLoadCompleted.Broadcast(bApplied, bApplied ? TEXT("") : TEXT("World loaded; player component restore failed."), 0);
}

void UWorldStateManagerSubsystem::Deinitialize()
{
    bShuttingDown = true;
    Super::Deinitialize();
}
