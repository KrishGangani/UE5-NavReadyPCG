// P08 NavReadyPCG - primary game module: demo workload and benchmark harness only.
// The reusable pipeline lives in the NavReadyPCGCore plugin; this module depends on it, never the reverse.

using UnrealBuildTool;

public class NavReadyPCG : ModuleRules
{
	public NavReadyPCG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		// Private: the public headers only forward-declare PCG and navigation types, so only .cpp files need these include paths.
		// PCG: cell actors, the code-built graph and the PCG world actor (S1). NavigationSystem: the arena's navigation bounds and
		// path queries (S1). NavReadyPCGCore: no class uses it yet; it stays private until a public header includes one of its headers.
		PrivateDependencyModuleNames.AddRange(new string[] { "NavReadyPCGCore", "PCG", "NavigationSystem" });
	}
}
