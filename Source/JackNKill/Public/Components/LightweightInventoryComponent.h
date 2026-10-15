#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Systems/GameplayRules.h"
#include "LightweightInventoryComponent.generated.h"

USTRUCT(BlueprintType)
struct JACKNKILL_API FInventoryStack
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ItemId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Quantity = 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryChanged);

UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class JACKNKILL_API ULightweightInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    ULightweightInventoryComponent();
    static constexpr int32 MaxStacks = JNK::Rules::MaxInventoryStacks;
    static constexpr int32 MaxQuantity = JNK::Rules::MaxItemQuantity;
    static bool IsValidItemId(FName Id);
    static bool ValidateStacks(const TArray<FInventoryStack>& Stacks);

    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FInventoryChanged OnInventoryChanged;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
    bool AddItem(FName ItemId, int32 Quantity);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
    bool RemoveItem(FName ItemId, int32 Quantity);
    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetQuantity(FName ItemId) const;
    UFUNCTION(BlueprintPure, Category = "Inventory")
    TArray<FInventoryStack> GetSnapshot() const { return Items; }
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
    bool RestoreSnapshot(const TArray<FInventoryStack>& Snapshot);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    UPROPERTY(ReplicatedUsing = OnRep_Items)
    TArray<FInventoryStack> Items;
    bool CanMutate(bool bRestore = false) const;
    UFUNCTION()
    void OnRep_Items();
};
