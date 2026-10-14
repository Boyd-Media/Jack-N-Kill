#include "Characters/JNKPlayerCharacter.h"
#include "Components/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"

AJNKPlayerCharacter::AJNKPlayerCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);
    Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
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
}
