#include "PBRTextureLabAssetBrowserUtils.h"

#include "Animation/AnimSequenceBase.h"
#include "Blueprint/BlueprintSupport.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "PBRTextureLabAssetBrowser"

namespace
{
	void AddClassPath(FARFilter& Filter, UClass* Class)
	{
		if (Class)
		{
			Filter.ClassPaths.Add(Class->GetClassPathName());
		}
	}
}

namespace PBRTextureLabAssetBrowser
{
	FARFilter MakeFilter(const EPBRTextureLabAssetCategory Category)
	{
		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;

		switch (Category)
		{
		case EPBRTextureLabAssetCategory::Blueprints:
		case EPBRTextureLabAssetCategory::BlueprintInterfaces:
			AddClassPath(Filter, UBlueprint::StaticClass());
			break;
		case EPBRTextureLabAssetCategory::Materials:
			AddClassPath(Filter, UMaterialInterface::StaticClass());
			AddClassPath(Filter, UMaterialFunctionInterface::StaticClass());
			break;
		case EPBRTextureLabAssetCategory::Models:
			AddClassPath(Filter, UStaticMesh::StaticClass());
			AddClassPath(Filter, USkeletalMesh::StaticClass());
			break;
		case EPBRTextureLabAssetCategory::Textures:
			AddClassPath(Filter, UTexture::StaticClass());
			break;
		case EPBRTextureLabAssetCategory::Animations:
			AddClassPath(Filter, UAnimSequenceBase::StaticClass());
			break;
		case EPBRTextureLabAssetCategory::Audio:
			AddClassPath(Filter, USoundBase::StaticClass());
			break;
		case EPBRTextureLabAssetCategory::Data:
			AddClassPath(Filter, UDataAsset::StaticClass());
			AddClassPath(Filter, UDataTable::StaticClass());
			break;
		case EPBRTextureLabAssetCategory::All:
		default:
			break;
		}

		return Filter;
	}

	bool ShouldFilterAsset(const EPBRTextureLabAssetCategory Category, const FAssetData& AssetData)
	{
		if (Category != EPBRTextureLabAssetCategory::BlueprintInterfaces)
		{
			return false;
		}

		FString BlueprintType;
		return !AssetData.GetTagValue(FBlueprintTags::BlueprintType, BlueprintType)
			|| BlueprintType != TEXT("BPType_Interface");
	}

	FText GetCategoryLabel(const EPBRTextureLabAssetCategory Category)
	{
		switch (Category)
		{
		case EPBRTextureLabAssetCategory::Blueprints:
			return LOCTEXT("Blueprints", "Blueprints");
		case EPBRTextureLabAssetCategory::BlueprintInterfaces:
			return LOCTEXT("BlueprintInterfaces", "Blueprint Interfaces");
		case EPBRTextureLabAssetCategory::Materials:
			return LOCTEXT("Materials", "Materials");
		case EPBRTextureLabAssetCategory::Models:
			return LOCTEXT("Models", "Models");
		case EPBRTextureLabAssetCategory::Textures:
			return LOCTEXT("Textures", "Textures");
		case EPBRTextureLabAssetCategory::Animations:
			return LOCTEXT("Animations", "Animations");
		case EPBRTextureLabAssetCategory::Audio:
			return LOCTEXT("Audio", "Audio");
		case EPBRTextureLabAssetCategory::Data:
			return LOCTEXT("Data", "Data");
		case EPBRTextureLabAssetCategory::All:
		default:
			return LOCTEXT("All", "All Assets");
		}
	}
}

#undef LOCTEXT_NAMESPACE
