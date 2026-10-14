#include "Framework/JNKLocalGameMode.h"
#include "Characters/JNKPlayerCharacter.h"
#include "Framework/PublicOrderPlayerController.h"

AJNKLocalGameMode::AJNKLocalGameMode()
{
    DefaultPawnClass = AJNKPlayerCharacter::StaticClass();
    PlayerControllerClass = APublicOrderPlayerController::StaticClass();
}
