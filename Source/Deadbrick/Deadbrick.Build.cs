using UnrealBuildTool;

public class Deadbrick : ModuleRules
{
    public Deadbrick(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // UE 5.8 no longer resolves our module-root subfolders (AI, Combat, Player, World)
        // through legacy parent include paths. Explicitly expose the module root so includes such as
        // "World/DestructibleVoxelWorld.h" also resolve from UHT-generated translation units.
        PublicIncludePaths.Add(ModuleDirectory);
        PrivateIncludePaths.Add(ModuleDirectory);

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "AIModule",
            "GameplayTasks",
            "NavigationSystem",
            "ProceduralMeshComponent"
        });
    }
}
