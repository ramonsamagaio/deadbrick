using UnrealBuildTool;
using System.IO;

public class PhysX5 : ModuleRules
{
    public PhysX5(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
        string SdkRoot = Path.Combine(ProjectRoot, "ThirdParty", "PhysX5", "SDK");
        string IncludeDir = Path.Combine(SdkRoot, "include");
        string LibDir = Path.Combine(SdkRoot, "lib", "Win64");
        string BinDir = Path.Combine(SdkRoot, "bin", "Win64");
        string BridgeIncludeDir = Path.Combine(ProjectRoot, "External", "PhysX5Bridge", "include");
        string BridgeLibrary = Path.Combine(LibDir, "DeadbrickPhysXBridge.lib");

        if (Target.Platform != UnrealTargetPlatform.Win64)
        {
            PublicDefinitions.Add("WITH_DEADBRICK_PHYSX5=0");
            return;
        }

        string[] RequiredPhysXLibraries = new string[]
        {
            "PhysX_64.lib",
            "PhysXCommon_64.lib",
            "PhysXFoundation_64.lib",
            "PhysXExtensions_static_64.lib"
        };

        string[] RequiredDlls = new string[]
        {
            "PhysX_64.dll",
            "PhysXCommon_64.dll",
            "PhysXFoundation_64.dll"
        };

        bool bSdkReady = Directory.Exists(IncludeDir)
            && File.Exists(Path.Combine(IncludeDir, "PxPhysicsAPI.h"))
            && Directory.Exists(BridgeIncludeDir)
            && File.Exists(Path.Combine(BridgeIncludeDir, "DeadbrickPhysXBridge.h"))
            && File.Exists(BridgeLibrary);

        foreach (string Library in RequiredPhysXLibraries)
        {
            bSdkReady &= File.Exists(Path.Combine(LibDir, Library));
        }
        foreach (string Dll in RequiredDlls)
        {
            bSdkReady &= File.Exists(Path.Combine(BinDir, Dll));
        }

        if (!bSdkReady)
        {
            PublicDefinitions.Add("WITH_DEADBRICK_PHYSX5=0");
            return;
        }

        PublicDefinitions.Add("WITH_DEADBRICK_PHYSX5=1");

        // Never expose NVIDIA PhysX headers to Unreal translation units. UE 5.8's Chaos compatibility
        // layer still forward-declares physx::PxQuat/PxTransform, which conflicts with PhysX 5.8 typedefs.
        // DEADBRICK talks to PhysX exclusively through this plain-C isolation bridge.
        PublicSystemIncludePaths.Add(BridgeIncludeDir);
        PublicAdditionalLibraries.Add(BridgeLibrary);

        foreach (string Library in RequiredPhysXLibraries)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, Library));
        }

        foreach (string Dll in RequiredDlls)
        {
            PublicDelayLoadDLLs.Add(Dll);
            RuntimeDependencies.Add("$(TargetOutputDir)/" + Dll, Path.Combine(BinDir, Dll));
        }
    }
}
