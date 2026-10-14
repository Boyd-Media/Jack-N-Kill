#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CoopSessionSubsystem.generated.h"

class UNetDriver;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCoopTravelError, const FString&, Message);

// Explicit opt-in direct-IP networking; no online service/login is required.
UCLASS()
class JACKNKILL_API UCoopSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, Category = "Co-op")
    bool bCoopEnabled = false;

    UPROPERTY(BlueprintReadOnly, Category = "Co-op")
    bool bTravelPending = false;

    UPROPERTY(BlueprintAssignable, Category = "Co-op")
    FCoopTravelError OnTravelError;

    UFUNCTION(BlueprintCallable, Category = "Co-op")
    bool SetCoopEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Co-op")
    bool IsNetworkActive() const;

    UFUNCTION(BlueprintCallable, Category = "Co-op")
    bool StartOfflineGame(FName MapPackage);

    UFUNCTION(BlueprintCallable, Category = "Co-op")
    bool HostGame(FName MapPackage);

    UFUNCTION(BlueprintCallable, Category = "Co-op")
    bool JoinByIP(const FString& Address);

    // Also disconnects a client or closes a host's listen server.
    UFUNCTION(BlueprintCallable, Category = "Co-op")
    bool ReturnToOfflineMenu(FName MenuMapPackage);

    static bool NormalizeJoinAddress(const FString& Address, FString& OutEndpoint);
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    FDelegateHandle NetworkFailureHandle;
    FDelegateHandle TravelFailureHandle;
    FDelegateHandle MapLoadedHandle;

    bool ValidateMap(FName MapPackage);
    void HandleNetworkFailure(UWorld* World, UNetDriver* Driver,
        ENetworkFailure::Type FailureType, const FString& Error);
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType,
        const FString& Error);
    void HandleMapLoaded(UWorld* World);
};
