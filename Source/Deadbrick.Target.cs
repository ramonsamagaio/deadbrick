using UnrealBuildTool;
using System.Collections.Generic;

public class DeadbrickTarget : TargetRules
{
    public DeadbrickTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("Deadbrick");
    }
}
