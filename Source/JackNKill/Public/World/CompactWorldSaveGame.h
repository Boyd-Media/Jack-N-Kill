#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Components/LightweightInventoryComponent.h"
#include "Components/PlayerMetabolismComponent.h"
#include "CompactWorldSaveGame.generated.h"

UENUM(BlueprintType)
enum class EPersistentDoorState : uint8
{
    ClosedUnlocked, ClosedLocked, Open, Broken
};

struct FDoorOverride
{
    FGuid Id;
    EPersistentDoorState State = EPersistentDoorState::ClosedUnlocked;
};

// Plain data only: safe to encode/decode on a worker without touching actors.
struct FCompactWorldSnapshot
{
    uint64 Generation = 0;
    TArray<FDoorOverride> Doors;
    TArray<FGuid> ClearedHouses;
    TArray<FInventoryStack> Inventory;
    FPlayerMetabolismSnapshot Metabolism;
};

namespace CompactWorldSave
{
    constexpr int32 MaxDoors = 16384;
    constexpr int32 MaxHouses = 4096;
    constexpr int32 MaxPayloadBytes = 512 * 1024;
    // Stronger than the requested 5 MB limit, leaving platform wrapper headroom.
    constexpr int32 MaxSerializedBytes = 1000000;
    JACKNKILL_API bool Encode(const FCompactWorldSnapshot& Snapshot, TArray<uint8>& Bytes);
    JACKNKILL_API bool Decode(const TArray<uint8>& Bytes, FCompactWorldSnapshot& Snapshot);
}

UCLASS()
class JACKNKILL_API UCompactWorldSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    // Transient bypasses automatic tagged-property serialization. Serialize()
    // explicitly writes bounded bytes with version/CRC, avoiding actor graphs.
    UPROPERTY(Transient)
    TArray<uint8> Payload;

    virtual void Serialize(FArchive& Ar) override;
    bool IsPayloadValid() const { return bPayloadValid; }
private:
    bool bPayloadValid = false;
};
