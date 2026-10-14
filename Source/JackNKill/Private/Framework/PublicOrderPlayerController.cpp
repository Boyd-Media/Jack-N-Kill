#include "Framework/PublicOrderPlayerController.h"
#include "AI/PublicOrderAIController.h"

bool APublicOrderPlayerController::BeginBribePrompt(APublicOrderAIController* Officer,
    const FGuid& RequestId, int32 Credits, float TimeoutSeconds)
{
    if (!HasAuthority() || !IsValid(Officer) || !RequestId.IsValid()
        || PendingRequest.IsValid() || Credits <= 0 || TimeoutSeconds <= 0.0f)
    {
        return false;
    }
    PendingRequest = RequestId;
    PendingOfficer = Officer;
    ClientShowBribePrompt(RequestId, Credits, TimeoutSeconds);
    return true;
}

void APublicOrderPlayerController::CloseBribePrompt(const FGuid& RequestId, bool bPaid)
{
    if (!HasAuthority() || PendingRequest != RequestId)
    {
        return;
    }
    PendingRequest.Invalidate();
    PendingOfficer.Reset();
    ClientCloseBribePrompt(RequestId, bPaid);
}

void APublicOrderPlayerController::SubmitBribeResponse(FGuid RequestId, bool bAccept)
{
    if (IsLocalController())
    {
        ServerSubmitBribeResponse(RequestId, bAccept);
    }
}

void APublicOrderPlayerController::ServerSubmitBribeResponse_Implementation(
    FGuid RequestId, bool bAccept)
{
    if (!RequestId.IsValid() || PendingRequest != RequestId)
    {
        return;
    }
    if (APublicOrderAIController* Officer = PendingOfficer.Get())
    {
        Officer->ResolveBribe(RequestId, this, bAccept);
    }
    else
    {
        CloseBribePrompt(RequestId, false);
    }
}

bool APublicOrderPlayerController::TrySpendBribeCredits_Implementation(int32 Amount)
{
    // Connect a real authoritative wallet before enabling successful bribes.
    return false;
}

void APublicOrderPlayerController::ClientShowBribePrompt_Implementation(
    FGuid RequestId, int32 Credits, float TimeoutSeconds)
{
    OnBribePrompt.Broadcast(RequestId, Credits, TimeoutSeconds);
}

void APublicOrderPlayerController::ClientCloseBribePrompt_Implementation(
    FGuid RequestId, bool bPaid)
{
    OnBribeClosed.Broadcast(RequestId, bPaid);
}

void APublicOrderPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        if (APublicOrderAIController* Officer = PendingOfficer.Get())
        {
            Officer->ClearEncounter();
        }
    }
    Super::EndPlay(EndPlayReason);
}
