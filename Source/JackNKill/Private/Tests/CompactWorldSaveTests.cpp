#if WITH_DEV_AUTOMATION_TESTS
#include "World/CompactWorldSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompactWorldSaveTest,
    "JackNKill.Persistence.CompactRoundTripAndBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCompactWorldSaveTest::RunTest(const FString& Parameters)
{
    FCompactWorldSnapshot Original;
    Original.Generation = 7;
    Original.Metabolism.Fat = 0.81f;
    Original.Metabolism.Muscular = 0.74f;
    Original.Metabolism.DayProgress = 0.45f;
    Original.Metabolism.CaloricIntake = 2400.0f;
    Original.Metabolism.StaminaBurn = 350.0f;
    for (int32 Index = 0; Index < CompactWorldSave::MaxDoors; ++Index)
    {
        FDoorOverride Door;
        Door.Id = FGuid(Index + 1, 1, 2, 3);
        Door.State = static_cast<EPersistentDoorState>(Index % 4);
        Original.Doors.Add(Door);
    }
    for (int32 Index = 0; Index < CompactWorldSave::MaxHouses; ++Index)
    {
        Original.ClearedHouses.Add(FGuid(Index + 1, 4, 5, 6));
    }
    for (int32 Index = 0; Index < ULightweightInventoryComponent::MaxStacks; ++Index)
    {
        FInventoryStack Stack;
        Stack.ItemId = FName(*FString::Printf(TEXT("Item_%d"), Index));
        Stack.Quantity = ULightweightInventoryComponent::MaxQuantity;
        Original.Inventory.Add(Stack);
    }
    TArray<uint8> Payload;
    TestTrue(TEXT("Maximum record counts encode"), CompactWorldSave::Encode(Original, Payload));
    UCompactWorldSaveGame* Save = NewObject<UCompactWorldSaveGame>();
    Save->Payload = Payload;
    TArray<uint8> Serialized;
    TestTrue(TEXT("UE SaveGame serializes"), UGameplayStatics::SaveGameToMemory(Save, Serialized));
    TestTrue(TEXT("Serialized file remains below the one-megabyte safety cap"),
        Serialized.Num() <= CompactWorldSave::MaxSerializedBytes);
    UCompactWorldSaveGame* Loaded = Cast<UCompactWorldSaveGame>(UGameplayStatics::LoadGameFromMemory(Serialized));
    if (!TestTrue(TEXT("Custom version/CRC load passes"), Loaded && Loaded->IsPayloadValid()))
    {
        return false;
    }
    FCompactWorldSnapshot Restored;
    if (!TestTrue(TEXT("Payload decodes"), CompactWorldSave::Decode(Loaded->Payload, Restored)))
    {
        return false;
    }
    TestEqual(TEXT("Generation"), Restored.Generation, Original.Generation);
    TestEqual(TEXT("Door count"), Restored.Doors.Num(), Original.Doors.Num());
    TestEqual(TEXT("Last door ID"), Restored.Doors.Last().Id, Original.Doors.Last().Id);
    TestEqual(TEXT("Last door state"), static_cast<uint8>(Restored.Doors.Last().State), static_cast<uint8>(Original.Doors.Last().State));
    TestEqual(TEXT("House count"), Restored.ClearedHouses.Num(), Original.ClearedHouses.Num());
    TestEqual(TEXT("Inventory quantity"), Restored.Inventory.Last().Quantity, Original.Inventory.Last().Quantity);
    TestEqual(TEXT("Inventory stable ID"), Restored.Inventory.Last().ItemId, Original.Inventory.Last().ItemId);
    TestEqual(TEXT("Fat"), Restored.Metabolism.Fat, Original.Metabolism.Fat);
    TestEqual(TEXT("Muscular"), Restored.Metabolism.Muscular, Original.Metabolism.Muscular);
    TestEqual(TEXT("Target fat"), Restored.Metabolism.TargetFat, Original.Metabolism.TargetFat);
    TestEqual(TEXT("Target muscle"), Restored.Metabolism.TargetMuscular, Original.Metabolism.TargetMuscular);
    TestEqual(TEXT("Day progress"), Restored.Metabolism.DayProgress, Original.Metabolism.DayProgress);
    TestEqual(TEXT("Calories"), Restored.Metabolism.CaloricIntake, Original.Metabolism.CaloricIntake);
    TestEqual(TEXT("Activity burn"), Restored.Metabolism.StaminaBurn, Original.Metabolism.StaminaBurn);
    Original.Doors.Add(Original.Doors[0]);
    TestFalse(TEXT("Excess door count is rejected"), CompactWorldSave::Encode(Original, Payload));
    Original.Doors.Pop();
    Original.Doors[1].Id = Original.Doors[0].Id;
    TestFalse(TEXT("Duplicate persistent IDs are rejected"), CompactWorldSave::Encode(Original, Payload));
    return true;
}
#endif
