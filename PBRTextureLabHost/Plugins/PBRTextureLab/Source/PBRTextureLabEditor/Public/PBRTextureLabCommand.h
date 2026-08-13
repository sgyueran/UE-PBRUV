#pragma once

#include "CoreMinimal.h"
#include "PBRTextureLabUV.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * Level Editor / Content Browser command entry for Task 5 UV presets.
 * Must be called on the Game Thread.
 */
namespace PBRTextureLab
{
	enum class EPBRUVCommandStatus : uint8
	{
		Success = 0,
		Cancelled = 1,
		EmptySelection = 2,
		MixedSelection = 3,
		Unsupported = 4,
		BaselineMismatch = 5,
		Failed = 6
	};

	enum class EPBRUVEditChoice : uint8
	{
		Prompt = 0,
		DuplicateAndRebind = 1,
		ModifySource = 2,
		Cancel = 3
	};

	struct FPBRUVSelectionItem
	{
		UStaticMesh* Mesh = nullptr;
		TArray<UStaticMeshComponent*> SelectedComponents;
	};

	struct FPBRUVSelection
	{
		TArray<FPBRUVSelectionItem> Items;
		int32 NonStaticMeshCount = 0;
		int32 ConsideredCount = 0;
	};

	struct FPBRUVCommandRequest
	{
		EPBRUVPreset Preset = EPBRUVPreset::Scale100;
		EPBRUVEditChoice EditChoice = EPBRUVEditChoice::Prompt;
		bool bSave = false;
	};

	FName GetUVCommandContextName();
	FName GetUVScale100CommandName();
	FName GetUVScale500CommandName();

	FPBRUVSelection GatherUVSelection();

	int32 CountStaticMeshComponentUsers(const UStaticMesh* Mesh);

	EPBRUVCommandStatus ExecuteUVCommand(
		const FPBRUVCommandRequest& Request,
		FString* OutError = nullptr);

	EPBRUVCommandStatus ExecuteUVCommand(
		const FPBRUVCommandRequest& Request,
		const FPBRUVSelection& Selection,
		FString* OutError = nullptr);

	void ExecuteRegisteredUVCommand(EPBRUVPreset Preset);
}
