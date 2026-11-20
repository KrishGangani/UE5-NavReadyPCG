// P08 NavReadyPCG - game target (standalone/packaged builds).

using UnrealBuildTool;

public class NavReadyPCGTarget : TargetRules
{
	public NavReadyPCGTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("NavReadyPCG");
	}
}
