#include "PBRTextureLabCommands.h"

#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "PBRTextureLabCommands"

void FPBRTextureLabCommands::RegisterCommands()
{
	UI_COMMAND(
		UVScale100,
		"Set UV Tiling 100x100",
		"Scale the selected Static Mesh UV0 by (100, 100) so the material tiles 100x100. Default LOD0 only. Remap this shortcut in Editor Preferences.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::One, false, true, true, false));
	UI_COMMAND(
		UVScale500,
		"Set UV Tiling 500x500",
		"Scale the selected Static Mesh UV0 by (500, 500) so the material tiles 500x500. Default LOD0 only. Remap this shortcut in Editor Preferences.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Five, false, true, true, false));
	UI_COMMAND(
		OpenNomadTab,
		"PBR Texture Lab",
		"Open the PBR Texture Lab Nomad Tab to generate Metallic/Roughness maps.",
		EUserInterfaceActionType::Button,
		FInputChord());
	UI_COMMAND(
		OpenAssetBrowserTab,
		"PBR Asset Browser",
		"Open the PBR asset browser for Blueprints, interfaces, materials, models and other common assets.",
		EUserInterfaceActionType::Button,
		FInputChord());
}

#undef LOCTEXT_NAMESPACE
