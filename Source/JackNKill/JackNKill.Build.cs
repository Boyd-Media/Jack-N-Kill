using UnrealBuildTool;

public class JackNKill : ModuleRules
{
    public JackNKill(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
            "AIModule", "GameplayTasks", "NavigationSystem", "ChaosVehicles",
            "PhysicsCore", "Networking", "Sockets"
        });
    }
}
