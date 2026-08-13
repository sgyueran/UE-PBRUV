using UnrealBuildTool;

public class PBRTextureLabEditor : ModuleRules
{
	public PBRTextureLabEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new[]
		{
			System.IO.Path.Combine(ModuleDirectory, "Public")
		});

		PrivateIncludePaths.AddRange(new[]
		{
			System.IO.Path.Combine(ModuleDirectory, "Private")
		});

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"EditorFramework",
			"ToolMenus",
			"AssetTools",
			"AssetRegistry",
			"MeshDescription",
			"StaticMeshDescription",
			"LevelEditor",
			"ContentBrowser",
			"Projects"
		});
	}
}
