#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "World/CompactWorldSaveGame.h"
#include "WorldStateManagerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPersistenceCompleted,
    bool, bSuccess, const FString&, Message, int32, SerializedBytes);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWorldStateRestored);

UCLASS()
class JACKNKILL_API UWorldStateManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, Category = "Persistence")
    bool bBusy = false;
    UFUNCTION(BlueprintPure, Category = "Persistence")
    bool IsLoading() const { return bLoading; }
    bool IsApplyingPlayerRestore() const { return bApplyingPlayerRestore; }
    UPROPERTY(BlueprintAssignable, Category = "Persistence")
    FPersistenceCompleted OnSaveCompleted;
    UPROPERTY(BlueprintAssignable, Category = "Persistence")
    FPersistenceCompleted OnLoadCompleted;
    UPROPERTY(BlueprintAssignable, Category = "Persistence")
    FWorldStateRestored OnWorldStateRestored;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "World State")
    bool SetDoorState(FGuid Id, EPersistentDoorState State, EPersistentDoorState AuthoredDefault);
    UFUNCTION(BlueprintPure, Category = "World State")
    EPersistentDoorState GetDoorState(FGuid Id, EPersistentDoorState AuthoredDefault) const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "World State")
    bool MarkHouseCleared(FGuid Id);
    UFUNCTION(BlueprintPure, Category = "World State")
    bool IsHouseCleared(FGuid Id) const { return ClearedHouses.Contains(Id); }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Persistence")
    bool BindPlayerComponents(ULightweightInventoryComponent* Inventory, UPlayerMetabolismComponent* Metabolism);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Persistence")
    bool SaveAsync(const FString& Profile = TEXT("PlayerWorld"));
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Persistence")
    bool LoadAsync(const FString& Profile = TEXT("PlayerWorld"));
    virtual void Deinitialize() override;

private:
    TMap<FGuid, EPersistentDoorState> DoorOverrides;
    TSet<FGuid> ClearedHouses;
    TWeakObjectPtr<ULightweightInventoryComponent> PlayerInventory;
    TWeakObjectPtr<UPlayerMetabolismComponent> PlayerMetabolism;
    UPROPERTY(Transient)
    TObjectPtr<UCompactWorldSaveGame> PendingSave;
    uint64 Generation = 0;
    FString ActiveProfile;
    FString LoadingProfile;
    int32 ReadSlotIndex = 0;
    int32 PendingByteCount = 0;
    TOptional<FCompactWorldSnapshot> BestLoaded;
    bool bLoading = false;
    bool bShuttingDown = false;
    bool bPendingPlayerRestore = false;
    bool bApplyingPlayerRestore = false;
    TArray<FInventoryStack> LoadedInventory;
    FPlayerMetabolismSnapshot LoadedMetabolism;

    bool IsAuthority() const;
    static bool ValidProfile(const FString& Profile);
    bool ApplyPendingPlayer();
    void HandleEncodedSave(bool bValid, TArray<uint8> Bytes, uint64 SavedGeneration);
    void HandleSaveCompleted(const FString& Slot, int32 UserIndex, bool bSuccess);
    void HandleLoadedGame(const FString& Slot, int32 UserIndex, USaveGame* Loaded);
    void HandleDecodedLoad(bool bValid, FCompactWorldSnapshot Snapshot);
};
