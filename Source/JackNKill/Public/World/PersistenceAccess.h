#pragma once
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "World/WorldStateManagerSubsystem.h"
#include "Systems/GameplayRules.h"

namespace JNK
{
    inline bool CanMutateDuringPersistence(const UWorld* World, bool bRestore = false)
    {
        if (!World || World->GetNetMode() == NM_Client)
        {
            return false;
        }
        const UGameInstance* Instance = World->GetGameInstance();
        const UWorldStateManagerSubsystem* Manager = Instance
            ? Instance->GetSubsystem<UWorldStateManagerSubsystem>() : nullptr;
        return !Manager || Rules::CanMutateDuringLoad(Manager->IsLoading(), bRestore, Manager->IsApplyingPlayerRestore());
    }
}
