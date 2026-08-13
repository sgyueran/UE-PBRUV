#pragma once

#include "CoreMinimal.h"
#include "PBRTextureLabTextureImport.h"

class UMaterial;
class UMaterialInstanceConstant;

/**
 * UE Metallic/Roughness parent material and constant instances.
 * This is not an OpenPBR implementation.
 * Must be called on the Game Thread.
 */
namespace PBRTextureLab
{
	struct FPBRMaterialInstanceRequest
	{
		FString DestinationPath = TEXT("/Game/PBRTextureLab");
		FString BaseName = TEXT("PBR");
		EPBRImportConflictPolicy ConflictPolicy = EPBRImportConflictPolicy::Cancel;
		float NormalStrength = 1.0f;
		float HeightAmount = 0.0f;
		float UVScale = 1.0f;
		bool bSave = true;
		bool bCancelled = false;
	};

	FString GetParentMaterialObjectPath();

	UMaterial* GetOrCreateParentMaterial(FString* OutError = nullptr);

	EPBRImportStatus CreateMaterialInstance(
		const FPBRImportedTextures& Textures,
		const FPBRMaterialInstanceRequest& Request,
		UMaterialInstanceConstant*& OutInstance,
		FString* OutError = nullptr);
}
