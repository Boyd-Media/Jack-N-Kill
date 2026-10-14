#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "JNKPlayerCharacter.generated.h"

class UCombatComponent;

UCLASS()
class JACKNKILL_API AJNKPlayerCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AJNKPlayerCharacter();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UCombatComponent> Combat;

    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    FName CombatTraceSocket = TEXT("weapon_tip");

protected:
    virtual void BeginPlay() override;
};
