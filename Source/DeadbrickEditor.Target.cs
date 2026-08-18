using UnrealBuildTool;
using System.Collections.Generic;

public class DeadbrickEditorTarget : TargetRules
{
    public DeadbrickEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("Deadbrick");
    }
}
