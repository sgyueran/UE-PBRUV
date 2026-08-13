#pragma once

#include "CoreMinimal.h"
#include "PBRTextureLabTextureImport.h"

class UMaterial;
class UMaterialInterface;
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
		FString InstanceName;
		UMaterialInterface* ParentMaterial = nullptr;
		EPBRImportConflictPolicy ConflictPolicy = EPBRImportConflictPolicy::Cancel;
		float NormalStrength = 0.1f;
		FPBRMapExportFlags EnabledMaps;
		float RoughnessStrength = 1.0f;
		float MetallicStrength = 1.0f;
		float HeightAmount = 0.0f;
		float UVScale = 1.0f;
		float RoughnessBrightness = 0.0f;
		UMaterialInstanceConstant* ExistingInstance = nullptr;
		bool bModifyExisting = false;
		bool bSave = true;
		bool bCancelled = false;
	};

	enum class EPBRParentParamSlot : uint8
	{
		BaseColorTexture,
		NormalTexture,
		RoughnessTexture,
		MetallicTexture,
		HeightTexture,
		AOTexture,
		NormalStrength,
		RoughnessStrength,
		MetallicStrength,
		HeightAmount,
		UVScale,
		RoughnessBrightness
	};

	/** Custom-parameter titles copied from the selected parent. Generator UI uses these 1:1. */
	struct FPBRParentParamTitles
	{
		FString BaseColorTexture = TEXT("基础贴图");
		FString NormalTexture = TEXT("法线贴图");
		FString RoughnessTexture = TEXT("粗糙贴图");
		FString MetallicTexture = TEXT("金属贴图");
		FString HeightTexture = TEXT("置换贴图");
		FString AOTexture = TEXT("AO");
		FString NormalStrength = TEXT("法线强度");
		FString RoughnessStrength = TEXT("粗糙强度");
		FString MetallicStrength = TEXT("金属强度");
		FString HeightAmount = TEXT("置换强度");
		FString UVScale = TEXT("UV缩放");
		FString RoughnessBrightness = TEXT("粗糙明度");

		FString Get(EPBRParentParamSlot Slot) const;
	};

	FPBRParentParamTitles InspectParentParamTitles(UMaterialInterface* Parent);

	struct FPBRBundledParentDesc
	{
		FString DisplayName;
		FString ObjectPath;
	};

	FString GetBundledParentsFolder();

	TArray<FPBRBundledParentDesc> GetBundledParentCatalog();

	UMaterialInterface* LoadBundledParentByName(const FString& DisplayName);

	/** Prefer bundled plugin 母球, then /Game 000基础材质. */
	UMaterialInterface* FindPreferredUserParent();

	FString GetParentMaterialObjectPath();

	UMaterial* GetOrCreateParentMaterial(FString* OutError = nullptr);

	EPBRImportStatus CreateMaterialInstance(
		const FPBRImportedTextures& Textures,
		const FPBRMaterialInstanceRequest& Request,
		UMaterialInstanceConstant*& OutInstance,
		FString* OutError = nullptr);
}
