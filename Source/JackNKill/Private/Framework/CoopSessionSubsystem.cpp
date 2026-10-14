#include "Framework/CoopSessionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

void UCoopSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // Startup never hosts, joins, or creates an online session automatically.
    bCoopEnabled = false;
    if (GEngine)
    {
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this,
            &UCoopSessionSubsystem::HandleNetworkFailure);
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this,
            &UCoopSessionSubsystem::HandleTravelFailure);
    }
    MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this,
        &UCoopSessionSubsystem::HandleMapLoaded);
}

bool UCoopSessionSubsystem::IsNetworkActive() const
{
    return GetWorld() && GetWorld()->GetNetMode() != NM_Standalone;
}

bool UCoopSessionSubsystem::SetCoopEnabled(bool bEnabled)
{
    if (bTravelPending || IsNetworkActive())
    {
        OnTravelError.Broadcast(TEXT("Leave the current session before changing the menu mode."));
        return false;
    }
    bCoopEnabled = bEnabled;
    return true;
}

bool UCoopSessionSubsystem::ValidateMap(FName MapPackage)
{
    const FString Package = MapPackage.ToString();
    if (!GetWorld() || !FPackageName::IsValidLongPackageName(Package)
        || !FPackageName::DoesPackageExist(Package))
    {
        OnTravelError.Broadcast(TEXT("Choose an existing cooked map using its full package path."));
        return false;
    }
    return true;
}

bool UCoopSessionSubsystem::StartOfflineGame(FName MapPackage)
{
    if (bTravelPending || !ValidateMap(MapPackage))
    {
        return false;
    }
    bCoopEnabled = false;
    bTravelPending = true;
    // Absolute travel with empty options discards an inherited ?listen option.
    UGameplayStatics::OpenLevel(this, MapPackage, true, TEXT(""));
    return true;
}

bool UCoopSessionSubsystem::HostGame(FName MapPackage)
{
    if (!bCoopEnabled || bTravelPending || IsNetworkActive())
    {
        OnTravelError.Broadcast(TEXT("Enable co-op from an offline menu before hosting."));
        return false;
    }
    if (!ValidateMap(MapPackage))
    {
        return false;
    }
    bTravelPending = true;
    UGameplayStatics::OpenLevel(this, MapPackage, true, TEXT("listen"));
    return true;
}

bool UCoopSessionSubsystem::NormalizeJoinAddress(const FString& Address, FString& OutEndpoint)
{
    OutEndpoint.Reset();
    TArray<FString> Parts;
    Address.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(":"), false);
    if (Parts.Num() < 1 || Parts.Num() > 2)
    {
        return false;
    }
    // Accept only literal IPv4 addresses, never arbitrary travel options/URLs.
    TArray<FString> Octets;
    Parts[0].ParseIntoArray(Octets, TEXT("."), false);
    if (Octets.Num() != 4)
    {
        return false;
    }
    for (const FString& Octet : Octets)
    {
        if (Octet.IsEmpty() || Octet.Len() > 3)
        {
            return false;
        }
        for (TCHAR Character : Octet)
        {
            if (Character < TEXT('0') || Character > TEXT('9'))
            {
                return false;
            }
        }
        if (FCString::Atoi(*Octet) > 255)
        {
            return false;
        }
    }
    FIPv4Address IP;
    if (!FIPv4Address::Parse(Parts[0], IP) || IP.Value == 0)
    {
        return false;
    }
    int32 Port = 7777;
    if (Parts.Num() == 2)
    {
        if (Parts[1].IsEmpty() || Parts[1].Len() > 5)
        {
            return false;
        }
        for (TCHAR Character : Parts[1])
        {
            if (Character < TEXT('0') || Character > TEXT('9'))
            {
                return false;
            }
        }
        Port = FCString::Atoi(*Parts[1]);
        if (Port < 1 || Port > 65535)
        {
            return false;
        }
    }
    OutEndpoint = FString::Printf(TEXT("%s:%d"), *IP.ToString(), Port);
    return true;
}

bool UCoopSessionSubsystem::JoinByIP(const FString& Address)
{
    if (!bCoopEnabled || bTravelPending || IsNetworkActive())
    {
        OnTravelError.Broadcast(TEXT("Enable co-op from an offline menu before joining."));
        return false;
    }
    FString Endpoint;
    APlayerController* Player = GetGameInstance()->GetFirstLocalPlayerController();
    if (!NormalizeJoinAddress(Address, Endpoint) || !Player)
    {
        OnTravelError.Broadcast(TEXT("Enter an IPv4 address and optional port, such as 192.168.1.10:7777."));
        return false;
    }
    bTravelPending = true;
    Player->ClientTravel(Endpoint, TRAVEL_Absolute);
    return true;
}

bool UCoopSessionSubsystem::ReturnToOfflineMenu(FName MenuMapPackage)
{
    return StartOfflineGame(MenuMapPackage);
}

void UCoopSessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* Driver,
    ENetworkFailure::Type FailureType, const FString& Error)
{
    if (World && World->GetGameInstance() == GetGameInstance())
    {
        bTravelPending = false;
        OnTravelError.Broadcast(Error);
    }
}

void UCoopSessionSubsystem::HandleTravelFailure(UWorld* World,
    ETravelFailure::Type FailureType, const FString& Error)
{
    if (World && World->GetGameInstance() == GetGameInstance())
    {
        bTravelPending = false;
        OnTravelError.Broadcast(Error);
    }
}

void UCoopSessionSubsystem::HandleMapLoaded(UWorld* World)
{
    if (World && World->GetGameInstance() == GetGameInstance())
    {
        bTravelPending = false;
    }
}

void UCoopSessionSubsystem::Deinitialize()
{
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    }
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
    Super::Deinitialize();
}
