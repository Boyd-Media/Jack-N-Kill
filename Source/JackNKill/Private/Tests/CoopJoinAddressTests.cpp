#if WITH_DEV_AUTOMATION_TESTS
#include "Framework/CoopSessionSubsystem.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoopJoinAddressTest,
    "JackNKill.Network.JoinAddressValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoopJoinAddressTest::RunTest(const FString& Parameters)
{
    FString Endpoint;
    TestTrue(TEXT("IPv4 defaults to the game port"),
        UCoopSessionSubsystem::NormalizeJoinAddress(TEXT("192.168.1.10"), Endpoint));
    TestEqual(TEXT("Default endpoint"), Endpoint, FString(TEXT("192.168.1.10:7777")));
    TestTrue(TEXT("Explicit port and surrounding whitespace"),
        UCoopSessionSubsystem::NormalizeJoinAddress(TEXT(" 127.0.0.1:7788 "), Endpoint));
    TestEqual(TEXT("Explicit endpoint"), Endpoint, FString(TEXT("127.0.0.1:7788")));
    const TCHAR* Invalid[] = {
        TEXT(""), TEXT("localhost"), TEXT("127.0.0.1?listen"),
        TEXT("127.0.0.1:7777?game=Other"), TEXT("256.1.1.1"),
        TEXT("1.2.3"), TEXT("1.2.3."), TEXT("0.0.0.0"),
        TEXT("127.0.0.1:0"), TEXT("127.0.0.1:65536"),
        TEXT("127.0.0.1:77.7"), TEXT("127.0.0.1:"), TEXT("::1")
    };
    for (const TCHAR* Address : Invalid)
    {
        TestFalse(FString::Printf(TEXT("Reject invalid/option-bearing address: %s"), Address),
            UCoopSessionSubsystem::NormalizeJoinAddress(Address, Endpoint));
        TestTrue(TEXT("Failed parse clears endpoint"), Endpoint.IsEmpty());
    }
    return true;
}
#endif
