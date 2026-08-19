using UnrealBuildTool;
using System.Collections.Generic;

public class DeadbrickEditorTarget : TargetRules
{
    public DeadbrickEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("Deadbrick");
    }
}
