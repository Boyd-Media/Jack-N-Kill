#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "JNKPlayerCharacter.generated.h"

class UCombatComponent;
class ULightweightInventoryComponent;
class UPlayerMetabolismComponent;

UCLASS()
class JACKNKILL_API AJNKPlayerCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AJNKPlayerCharacter();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UCombatComponent> Combat;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TObjectPtr<ULightweightInventoryComponent> Inventory;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Metabolism")
    TObjectPtr<UPlayerMetabolismComponent> Metabolism;

    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    FName CombatTraceSocket = TEXT("weapon_tip");

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;

private:
    void BindPersistentPlayer();
};
