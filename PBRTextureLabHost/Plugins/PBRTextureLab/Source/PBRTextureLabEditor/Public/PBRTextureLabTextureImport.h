#pragma once

#include "CoreMinimal.h"
#include "PBRTextureLabPixelCore.h"

class UTexture2D;

/**
 * Import Task 2 pixel maps as saveable Texture2D assets.
 * Must be called on the Game Thread. Does not create materials.
 */
namespace PBRTextureLab
{
	enum class EPBRImportStatus : uint8
	{
		Success = 0,
		Cancelled = 1,
		NameConflict = 2,
		Failed = 3
	};

	enum class EPBRImportConflictPolicy : uint8
	{
		/** Do not import anything if any destination name already exists. */
		Cancel = 0,
		/** Overwrite existing assets. Requires explicit confirmation from the caller. */
		Replace = 1,
		/** Allocate a unique name via AssetTools. */
		UniqueName = 2
	};

	struct FPBRTextureImportRequest
	{
		FString DestinationPath = TEXT("/Game/PBRTextureLab");
		FString BaseName = TEXT("PBR");
		EPBRImportConflictPolicy ConflictPolicy = EPBRImportConflictPolicy::Cancel;
		bool bSave = true;
		bool bCancelled = false;
	};

	struct FPBRImportedTextures
	{
		UTexture2D* BaseColor = nullptr;
		UTexture2D* Height = nullptr;
		UTexture2D* Normal = nullptr;
		UTexture2D* AO = nullptr;
		UTexture2D* Roughness = nullptr;
		UTexture2D* Metallic = nullptr;
		UTexture2D* ORM = nullptr;

		bool HasAll() const
		{
			return BaseColor && Height && Normal && AO && Roughness && Metallic && ORM;
		}
	};

	FString GetStagingRootDirectory();

	EPBRImportStatus ImportPBRMaps(
		const FPBRMaps& Maps,
		const FPBRTextureImportRequest& Request,
		FPBRImportedTextures& OutTextures,
		FString* OutError = nullptr);
}
