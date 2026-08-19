using UnrealBuildTool;
using System.Collections.Generic;

public class DeadbrickTarget : TargetRules
{
    public DeadbrickTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("Deadbrick");
    }
}
