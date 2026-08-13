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

	enum class EPBRMapKind : uint8
	{
		Unknown = 0,
		BaseColor = 1,
		Normal = 2,
		Roughness = 3,
		Metallic = 4,
		Height = 5,
		AO = 6
	};

	struct FPBRMapExportFlags
	{
		bool bBaseColor = true;
		bool bHeight = true;
		bool bNormal = true;
		bool bAO = true;
		bool bRoughness = true;
		bool bMetallic = true;

		bool WantsAnyTexture() const
		{
			return bBaseColor || bHeight || bNormal || bAO || bRoughness || bMetallic;
		}
	};

	struct FPBRTextureImportRequest
	{
		FString DestinationPath = TEXT("/Game/PBRTextureLab");
		FString BaseName = TEXT("PBR");
		EPBRImportConflictPolicy ConflictPolicy = EPBRImportConflictPolicy::Cancel;
		FPBRMapExportFlags ExportFlags;
		bool bMakeSeamless = true;
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

		bool HasAll() const
		{
			return BaseColor && Height && Normal && AO && Roughness && Metallic;
		}

		bool HasAny() const
		{
			return BaseColor || Height || Normal || AO || Roughness || Metallic;
		}
	};

	FString GetStagingRootDirectory();

	EPBRMapKind GuessPBRMapKindFromFilename(const FString& Filename);

	UTexture2D* ImportLocalImageFile(
		const FString& Filename,
		const FString& DestinationPath,
		const FString& DesiredName,
		EPBRMapKind Kind,
		EPBRImportConflictPolicy ConflictPolicy,
		bool bSave,
		FString* OutError = nullptr,
		bool bMakeSeamless = true);

	EPBRImportStatus ImportPBRMapsFromLocalFolder(
		const FString& FolderPath,
		const FString& DestinationPath,
		const FString& BaseName,
		EPBRImportConflictPolicy ConflictPolicy,
		bool bSave,
		FPBRImportedTextures& OutTextures,
		FString* OutError = nullptr,
		bool bMakeSeamless = true);

	EPBRImportStatus ImportPBRMaps(
		const FPBRMaps& Maps,
		const FPBRTextureImportRequest& Request,
		FPBRImportedTextures& OutTextures,
		FString* OutError = nullptr);
}
