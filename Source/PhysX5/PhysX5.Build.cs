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

        if (Target.Platform != UnrealTargetPlatform.Win64)
        {
            PublicDefinitions.Add("WITH_DEADBRICK_PHYSX5=0");
            return;
        }

        string[] RequiredLibraries = new string[]
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

        bool bSdkReady = Directory.Exists(IncludeDir) && File.Exists(Path.Combine(IncludeDir, "PxPhysicsAPI.h"));
        foreach (string Library in RequiredLibraries)
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
        PublicSystemIncludePaths.Add(IncludeDir);

        foreach (string Library in RequiredLibraries)
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
