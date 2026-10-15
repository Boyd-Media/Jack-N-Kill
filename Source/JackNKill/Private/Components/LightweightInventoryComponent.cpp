#include "Components/LightweightInventoryComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

ULightweightInventoryComponent::ULightweightInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

bool ULightweightInventoryComponent::CanMutate() const
{
    return GetWorld() && GetWorld()->GetNetMode() != NM_Client
        && IsValid(GetOwner()) && GetOwner()->HasAuthority();
}

bool ULightweightInventoryComponent::IsValidItemId(FName Id)
{
    const FString Text = Id.ToString();
    if (Id.IsNone() || Text.Len() > 64)
    {
        return false;
    }
    for (TCHAR Ch : Text)
    {
        if (!((Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('A') && Ch <= TEXT('Z'))
            || (Ch >= TEXT('0') && Ch <= TEXT('9')) || Ch == TEXT('_')))
        {
            return false;
        }
    }
    return true;
}

bool ULightweightInventoryComponent::ValidateStacks(const TArray<FInventoryStack>& Stacks)
{
    if (Stacks.Num() > MaxStacks)
    {
        return false;
    }
    TSet<FName> Seen;
    for (const FInventoryStack& Stack : Stacks)
    {
        if (!IsValidItemId(Stack.ItemId) || Stack.Quantity < 1 || Stack.Quantity > MaxQuantity
            || Seen.Contains(Stack.ItemId))
        {
            return false;
        }
        Seen.Add(Stack.ItemId);
    }
    return true;
}

int32 ULightweightInventoryComponent::GetQuantity(FName ItemId) const
{
    const FInventoryStack* Stack = Items.FindByPredicate([ItemId](const FInventoryStack& Entry) { return Entry.ItemId == ItemId; });
    return Stack ? Stack->Quantity : 0;
}

bool ULightweightInventoryComponent::AddItem(FName ItemId, int32 Quantity)
{
    if (!CanMutate() || !IsValidItemId(ItemId) || Quantity < 1 || Quantity > MaxQuantity)
    {
        return false;
    }
    FInventoryStack* Stack = Items.FindByPredicate([ItemId](const FInventoryStack& Entry) { return Entry.ItemId == ItemId; });
    if (Stack)
    {
        if (Quantity > MaxQuantity - Stack->Quantity)
        {
            return false;
        }
        Stack->Quantity += Quantity;
    }
    else
    {
        if (Items.Num() >= MaxStacks)
        {
            return false;
        }
        FInventoryStack Entry;
        Entry.ItemId = ItemId;
        Entry.Quantity = Quantity;
        Items.Add(Entry);
    }
    OnRep_Items();
    return true;
}

bool ULightweightInventoryComponent::RemoveItem(FName ItemId, int32 Quantity)
{
    const int32 Index = Items.IndexOfByPredicate([ItemId](const FInventoryStack& Entry) { return Entry.ItemId == ItemId; });
    if (!CanMutate() || Index == INDEX_NONE || Quantity < 1 || Quantity > Items[Index].Quantity)
    {
        return false;
    }
    Items[Index].Quantity -= Quantity;
    if (Items[Index].Quantity == 0)
    {
        Items.RemoveAtSwap(Index);
    }
    OnRep_Items();
    return true;
}

bool ULightweightInventoryComponent::RestoreSnapshot(const TArray<FInventoryStack>& Snapshot)
{
    if (!CanMutate() || !ValidateStacks(Snapshot))
    {
        return false;
    }
    Items = Snapshot;
    OnRep_Items();
    return true;
}

void ULightweightInventoryComponent::OnRep_Items()
{
    OnInventoryChanged.Broadcast();
}

void ULightweightInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ULightweightInventoryComponent, Items, COND_OwnerOnly);
}
