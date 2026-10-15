#include "World/CompactWorldSaveGame.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
    constexpr uint32 Magic = 0x4A4E4B53;
    constexpr uint16 Version = 1;

    bool ValidMetabolism(const FPlayerMetabolismSnapshot& M)
    {
        const float Weights[] = {M.Fat, M.Muscular, M.TargetFat, M.TargetMuscular};
        for (float Weight : Weights)
        {
            if (!FMath::IsFinite(Weight) || Weight < 0.0f || Weight > 1.0f)
            {
                return false;
            }
        }
        return FMath::IsFinite(M.DayProgress) && M.DayProgress >= 0.0f && M.DayProgress < 1.0f
            && FMath::IsFinite(M.CaloricIntake) && M.CaloricIntake >= 0.0f && M.CaloricIntake <= 100000.0f
            && FMath::IsFinite(M.StaminaBurn) && M.StaminaBurn >= 0.0f && M.StaminaBurn <= 100000.0f;
    }

    bool ValidSnapshot(const FCompactWorldSnapshot& S)
    {
        if (S.Generation == 0 || S.Doors.Num() > CompactWorldSave::MaxDoors
            || S.ClearedHouses.Num() > CompactWorldSave::MaxHouses
            || !ULightweightInventoryComponent::ValidateStacks(S.Inventory) || !ValidMetabolism(S.Metabolism))
        {
            return false;
        }
        TSet<FGuid> Doors, Houses;
        for (const FDoorOverride& Door : S.Doors)
        {
            if (!Door.Id.IsValid() || static_cast<uint8>(Door.State) > 3 || Doors.Contains(Door.Id))
            {
                return false;
            }
            Doors.Add(Door.Id);
        }
        for (const FGuid& Id : S.ClearedHouses)
        {
            if (!Id.IsValid() || Houses.Contains(Id))
            {
                return false;
            }
            Houses.Add(Id);
        }
        return true;
    }

    void SerializeMetabolism(FArchive& Ar, FPlayerMetabolismSnapshot& M)
    {
        Ar << M.Fat << M.Muscular << M.TargetFat << M.TargetMuscular
            << M.DayProgress << M.CaloricIntake << M.StaminaBurn;
    }
}

bool CompactWorldSave::Encode(const FCompactWorldSnapshot& Snapshot, TArray<uint8>& Bytes)
{
    Bytes.Reset();
    if (!ValidSnapshot(Snapshot))
    {
        return false;
    }
    FMemoryWriter Ar(Bytes, true);
    uint64 Generation = Snapshot.Generation;
    Ar << Generation;
    int32 Count = Snapshot.Doors.Num();
    Ar << Count;
    for (const FDoorOverride& Door : Snapshot.Doors)
    {
        FGuid Id = Door.Id;
        uint8 State = static_cast<uint8>(Door.State);
        Ar << Id << State;
    }
    Count = Snapshot.ClearedHouses.Num();
    Ar << Count;
    for (FGuid Id : Snapshot.ClearedHouses)
    {
        Ar << Id;
    }
    Count = Snapshot.Inventory.Num();
    Ar << Count;
    for (const FInventoryStack& Stack : Snapshot.Inventory)
    {
        const FString Text = Stack.ItemId.ToString();
        uint8 Length = static_cast<uint8>(Text.Len());
        Ar << Length;
        for (TCHAR Character : Text)
        {
            uint8 Byte = static_cast<uint8>(Character);
            Ar << Byte;
        }
        uint16 Quantity = static_cast<uint16>(Stack.Quantity);
        Ar << Quantity;
    }
    FPlayerMetabolismSnapshot Metabolism = Snapshot.Metabolism;
    SerializeMetabolism(Ar, Metabolism);
    return !Ar.IsError() && Bytes.Num() <= MaxPayloadBytes;
}

bool CompactWorldSave::Decode(const TArray<uint8>& Bytes, FCompactWorldSnapshot& Snapshot)
{
    if (Bytes.IsEmpty() || Bytes.Num() > MaxPayloadBytes)
    {
        return false;
    }
    FCompactWorldSnapshot Result;
    FMemoryReader Ar(Bytes, true);
    Ar << Result.Generation;
    int32 Count = 0;
    Ar << Count;
    if (Ar.IsError() || Count < 0 || Count > MaxDoors)
    {
        return false;
    }
    Result.Doors.SetNum(Count);
    for (FDoorOverride& Door : Result.Doors)
    {
        uint8 State = 0;
        Ar << Door.Id << State;
        Door.State = static_cast<EPersistentDoorState>(State);
    }
    Ar << Count;
    if (Ar.IsError() || Count < 0 || Count > MaxHouses)
    {
        return false;
    }
    Result.ClearedHouses.SetNum(Count);
    for (FGuid& Id : Result.ClearedHouses)
    {
        Ar << Id;
    }
    Ar << Count;
    if (Ar.IsError() || Count < 0 || Count > ULightweightInventoryComponent::MaxStacks)
    {
        return false;
    }
    for (int32 Index = 0; Index < Count; ++Index)
    {
        uint8 Length = 0;
        Ar << Length;
        if (Ar.IsError() || Length < 1 || Length > 64)
        {
            return false;
        }
        ANSICHAR Buffer[65] = {};
        Ar.Serialize(Buffer, Length);
        for (int32 CharacterIndex = 0; CharacterIndex < Length; ++CharacterIndex)
        {
            const uint8 Ch = static_cast<uint8>(Buffer[CharacterIndex]);
            if (!((Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z')
                || (Ch >= '0' && Ch <= '9') || Ch == '_'))
            {
                return false;
            }
        }
        uint16 Quantity = 0;
        Ar << Quantity;
        if (Ar.IsError())
        {
            return false;
        }
        FInventoryStack Stack;
        Stack.ItemId = FName(ANSI_TO_TCHAR(Buffer));
        Stack.Quantity = Quantity;
        Result.Inventory.Add(Stack);
    }
    SerializeMetabolism(Ar, Result.Metabolism);
    if (Ar.IsError() || Ar.Tell() != Bytes.Num() || !ValidSnapshot(Result))
    {
        return false;
    }
    Snapshot = MoveTemp(Result);
    return true;
}

void UCompactWorldSaveGame::Serialize(FArchive& Ar)
{
    bPayloadValid = false;
    if (Ar.IsLoading() && Ar.TotalSize() > CompactWorldSave::MaxSerializedBytes)
    {
        Ar.SetError();
        return;
    }
    Super::Serialize(Ar);
    uint32 StoredMagic = Magic;
    uint16 StoredVersion = Version;
    int32 Size = Payload.Num();
    if (Ar.IsSaving() && (Size < 1 || Size > CompactWorldSave::MaxPayloadBytes))
    {
        Ar.SetError();
        return;
    }
    uint32 Checksum = Ar.IsSaving() ? FCrc::MemCrc32(Payload.GetData(), Payload.Num()) : 0;
    Ar << StoredMagic << StoredVersion << Size << Checksum;
    if (Ar.IsError() || StoredMagic != Magic || StoredVersion != Version
        || Size < 1 || Size > CompactWorldSave::MaxPayloadBytes)
    {
        Ar.SetError();
        Payload.Reset();
        return;
    }
    if (Ar.IsLoading())
    {
        Payload.SetNumUninitialized(Size);
    }
    Ar.Serialize(Payload.GetData(), Size);
    bPayloadValid = !Ar.IsError() && FCrc::MemCrc32(Payload.GetData(), Payload.Num()) == Checksum;
    if (!bPayloadValid)
    {
        Payload.Reset();
        Ar.SetError();
    }
}
