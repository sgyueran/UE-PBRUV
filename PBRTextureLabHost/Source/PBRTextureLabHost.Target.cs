using UnrealBuildTool;

public class PBRTextureLabHostTarget : TargetRules
{
	public PBRTextureLabHostTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// Latest tracks each engine's shared UnrealEditor environment (5.6=V5, 5.7=V6, 5.8=V7).
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PBRTextureLabHost");
	}
}
