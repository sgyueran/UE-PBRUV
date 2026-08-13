using UnrealBuildTool;

public class PBRTextureLabHostEditorTarget : TargetRules
{
	public PBRTextureLabHostEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// Latest tracks each engine's shared UnrealEditor environment (5.6=V5, 5.7=V6, 5.8=V7).
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PBRTextureLabHost");
	}
}
