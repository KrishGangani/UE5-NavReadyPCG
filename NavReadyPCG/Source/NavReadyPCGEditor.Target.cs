// P08 NavReadyPCG - editor target.

using UnrealBuildTool;

public class NavReadyPCGEditorTarget : TargetRules
{
	public NavReadyPCGEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("NavReadyPCG");
	}
}
