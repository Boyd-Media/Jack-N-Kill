using UnrealBuildTool;
using System.Collections.Generic;

public class JackNKillTarget : TargetRules
{
    public JackNKillTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        ExtraModuleNames.Add("JackNKill");
    }
}
