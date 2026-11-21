// NavReadyPCGCore - the reusable runtime module (cell registry, navigation commit, readiness tracking, determinism hash).
// It must not depend on any game module.

using UnrealBuildTool;

public class NavReadyPCGCore : ModuleRules
{
	public NavReadyPCGCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		// The two engine systems this plugin coordinates. Declared in Phase 4, before any code uses them, to prove early that
		// a launcher (binary) engine links them into both the editor and the packaged game (03_Implementation_Roadmap.md F2).
		PrivateDependencyModuleNames.AddRange(new string[] { "PCG", "NavigationSystem" });

		// S3: the readiness probe includes NavMesh/RecastNavMeshGenerator.h, which includes Recast's own headers
		// ("Recast/Recast.h"). They belong to the Navmesh module, a private dependency of NavigationSystem
		// (NavigationSystem.Build.cs:42), so they are not visible to us through NavigationSystem.
		PrivateDependencyModuleNames.Add("Navmesh");
	}
}
