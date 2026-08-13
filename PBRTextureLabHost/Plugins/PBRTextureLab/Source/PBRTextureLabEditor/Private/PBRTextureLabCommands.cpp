#include "PBRTextureLabCommands.h"

#define LOCTEXT_NAMESPACE "PBRTextureLabCommands"

void FPBRTextureLabCommands::RegisterCommands()
{
	UI_COMMAND(
		UVScale100,
		"UV Scale 100x",
		"Apply an absolute 100x UV0 scale to the selected Static Mesh.",
		EUserInterfaceActionType::Button,
		FInputChord());
	UI_COMMAND(
		UVScale500,
		"UV Scale 500x",
		"Apply an absolute 500x UV0 scale to the selected Static Mesh.",
		EUserInterfaceActionType::Button,
		FInputChord());
}

#undef LOCTEXT_NAMESPACE
