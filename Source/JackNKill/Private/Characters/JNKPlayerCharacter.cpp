#include "Characters/JNKPlayerCharacter.h"
#include "Components/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/LightweightInventoryComponent.h"
#include "Components/PlayerMetabolismComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "World/WorldStateManagerSubsystem.h"

AJNKPlayerCharacter::AJNKPlayerCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);
    Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
    Inventory = CreateDefaultSubobject<ULightweightInventoryComponent>(TEXT("Inventory"));
    Metabolism = CreateDefaultSubobject<UPlayerMetabolismComponent>(TEXT("Metabolism"));
}

void AJNKPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() != NM_Client)
    {
        if (GetNetMode() != NM_Standalone)
        {
            // Server weapon sockets must update even when the mesh is not rendered.
            GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        }
        Combat->SetTraceSource(GetMesh(), CombatTraceSocket);
    }
    BindPersistentPlayer();
}

void AJNKPlayerCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    BindPersistentPlayer();
}

void AJNKPlayerCharacter::BindPersistentPlayer()
{
    const APlayerController* Player = Cast<APlayerController>(GetController());
    if (GetNetMode() != NM_Client && Player && Player->IsLocalController() && GetGameInstance())
    {
        GetGameInstance()->GetSubsystem<UWorldStateManagerSubsystem>()->BindPlayerComponents(Inventory, Metabolism);
    }
}
