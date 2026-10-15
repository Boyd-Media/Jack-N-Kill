#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PublicOrderPlayerController.generated.h"

class APublicOrderAIController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPublicOrderBribePrompt,
    FGuid, RequestId, int32, Credits, float, TimeoutSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPublicOrderBribeClosed,
    FGuid, RequestId, bool, bPaid);

// Integrate this bridge into your final player-controller class.
UCLASS()
class JACKNKILL_API APublicOrderPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category = "Public Order|UI")
    FPublicOrderBribePrompt OnBribePrompt;

    UPROPERTY(BlueprintAssignable, Category = "Public Order|UI")
    FPublicOrderBribeClosed OnBribeClosed;

    UFUNCTION(BlueprintCallable, Category = "Public Order|UI")
    void SubmitBribeResponse(FGuid RequestId, bool bAccept);

    // Override on the server: check balance and debit atomically, without latent
    // work. Default uses the pawn inventory's Credits stack. Never accept a
    // client-supplied balance.
    UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category = "Public Order|Economy")
    bool TrySpendBribeCredits(int32 Amount);
    virtual bool TrySpendBribeCredits_Implementation(int32 Amount);

    bool BeginBribePrompt(APublicOrderAIController* Officer, const FGuid& RequestId,
        int32 Credits, float TimeoutSeconds);
    void CloseBribePrompt(const FGuid& RequestId, bool bPaid);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FGuid PendingRequest;
    TWeakObjectPtr<APublicOrderAIController> PendingOfficer;
    void HandleBribeResponse(FGuid RequestId, bool bAccept);

    UFUNCTION(Client, Reliable)
    void ClientShowBribePrompt(FGuid RequestId, int32 Credits, float TimeoutSeconds);

    UFUNCTION(Client, Reliable)
    void ClientCloseBribePrompt(FGuid RequestId, bool bPaid);

    UFUNCTION(Server, Reliable)
    void ServerSubmitBribeResponse(FGuid RequestId, bool bAccept);
};
