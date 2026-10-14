#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "JNKLocalGameMode.generated.h"

// Works in standalone by default and on the authority when explicitly hosted.
UCLASS()
class JACKNKILL_API AJNKLocalGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AJNKLocalGameMode();
};
