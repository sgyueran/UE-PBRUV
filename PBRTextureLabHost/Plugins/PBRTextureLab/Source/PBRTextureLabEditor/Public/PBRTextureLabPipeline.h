#pragma once

#include "CoreMinimal.h"
#include "PBRTextureLabMaterial.h"
#include "PBRTextureLabPixelCore.h"
#include "PBRTextureLabTextureImport.h"

class UMaterialInstanceConstant;
class UMaterialInterface;
class UTexture2D;
struct FImage;

/**
 * Task 7 integration: load a Texture2D or local image, generate maps, import, create MIC.
 * Source decode and pixel generation may run off the Game Thread.
 * Import and material creation must run on the Game Thread.
 */
namespace PBRTextureLab
{
	constexpr int32 MaxOutputDimension = 4096;

	struct FPBRGenerateRequest
	{
		UTexture2D* SourceTexture = nullptr;
		FString LocalImagePath;
		int32 OutputWidth = 0;
		int32 OutputHeight = 0;
		FPBRPixelParams PixelParams;
		FString DestinationPath = TEXT("/Game/PBRTextureLab");
		FString BaseName = TEXT("PBR");
		FString MaterialInstanceName;
		UMaterialInterface* ParentMaterial = nullptr;
		EPBRImportConflictPolicy ConflictPolicy = EPBRImportConflictPolicy::UniqueName;
		FPBRMapExportFlags ExportFlags;
		float MaterialNormalStrength = 0.1f;
		float MaterialRoughnessStrength = 1.0f;
		float MaterialMetallicStrength = 1.0f;
		float MaterialHeightAmount = 0.0f;
		float MaterialUVScale = 1.0f;
		float MaterialRoughnessBrightness = 0.0f;
		UMaterialInstanceConstant* ExistingInstance = nullptr;
		bool bModifyExisting = false;
		bool bCreateMaterial = true;
		bool bCopyExistingTexturesToFolder = true;
		bool bMakeSeamless = true;
		bool bSave = true;
		bool bCancelled = false;
	};

	struct FPBRGenerateResult
	{
		FPBRImageRgba8 WorkingImage;
		FPBRMaps Maps;
		FPBRImportedTextures Textures;
		UMaterialInstanceConstant* MaterialInstance = nullptr;
		FString OutputFolder;
		FString CreatedMaterialName;
		FString Disclaimer;
	};

	bool ConvertImageToRgba8(const FImage& Image, FPBRImageRgba8& OutImage, FString* OutError = nullptr);

	bool ReadSourceTexture2D(UTexture2D* Texture, FPBRImageRgba8& OutImage, FString* OutError = nullptr);

	bool ReadSourceLocalFile(const FString& Filename, FPBRImageRgba8& OutImage, FString* OutError = nullptr);

	bool WriteRgba8Png(const FPBRImageRgba8& Image, const FString& Filename, FString* OutError = nullptr);

	bool PrepareWorkingImage(
		const FPBRImageRgba8& Source,
		int32 OutputWidth,
		int32 OutputHeight,
		FPBRImageRgba8& OutImage,
		FString* OutError = nullptr);

	bool LoadGenerateSource(
		const FPBRGenerateRequest& Request,
		FPBRImageRgba8& OutImage,
		FString* OutError = nullptr);

	EPBRImportStatus GenerateAndImportFromSource(
		const FPBRGenerateRequest& Request,
		FPBRGenerateResult& OutResult,
		FString* OutError = nullptr);

	EPBRImportStatus CreateMaterialFromExistingTextures(
		const FPBRGenerateRequest& Request,
		const FPBRImportedTextures& ExistingTextures,
		FPBRGenerateResult& OutResult,
		FString* OutError = nullptr);
}
