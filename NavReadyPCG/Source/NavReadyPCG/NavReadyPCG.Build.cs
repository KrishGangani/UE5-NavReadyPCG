// P08 NavReadyPCG - primary game module: demo workload and benchmark harness only.
// The reusable pipeline lives in the NavReadyPCGCore plugin; this module depends on it, never the reverse.

using UnrealBuildTool;

public class NavReadyPCG : ModuleRules
{
	public NavReadyPCG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		// Private: no public header of this module includes a plugin header yet (Phase 4 has no classes).
		PrivateDependencyModuleNames.AddRange(new string[] { "NavReadyPCGCore" });
	}
}
