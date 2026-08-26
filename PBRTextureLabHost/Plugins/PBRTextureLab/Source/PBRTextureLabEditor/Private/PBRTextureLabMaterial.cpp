#include "PBRTextureLabMaterial.h"

#include <initializer_list>
#include "PBRTextureLabCompat.h"
#include "PBRTextureLabEditorApi.h"
#include "PBRTextureLabPixelCore.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpressionBumpOffset.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialShared.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "ScopedTransaction.h"
#include "Templates/Function.h"
#include "UObject/Package.h"

namespace PBRTextureLab
{
	namespace
	{
		FString GetParentAssetName()
		{
			// Each engine minor version writes its own uasset. A 5.8 parent cannot be
			// fully loaded or saved by 5.6/5.7 ("package only partially loaded").
			return FString::Printf(
				TEXT("M_PBRTextureLabMR_%d%d"),
				PBRTEXTURELAB_ENGINE_MAJOR,
				PBRTEXTURELAB_ENGINE_MINOR);
		}

		FString GetLegacyParentPackageName(const FString& MountedRoot)
		{
			return MountedRoot + TEXT("Materials/M_PBRTextureLabMR");
		}

		void MaterialSetError(FString* OutError, const FString& Message)
		{
			UE_LOG(LogPBRTextureLab, Error, TEXT("%s"), *Message);
			if (OutError)
			{
				*OutError = Message;
			}
		}

		bool MaterialIsGameThread(FString* OutError)
		{
			if (IsInGameThread())
			{
				return true;
			}
			MaterialSetError(OutError, TEXT("PBR material APIs must run on the Game Thread."));
			return false;
		}

		FString MaterialNormalizeContentPath(const FString& InPath)
		{
			FString Path = InPath;
			Path.ReplaceInline(TEXT("\\"), TEXT("/"));
			while (Path.Len() > 1 && Path.EndsWith(TEXT("/")))
			{
				Path.LeftChopInline(1);
			}
			return Path;
		}

		bool MaterialGetExistingPackageFilename(const FString& PackageName, FString& OutFilename)
		{
			if (FPackageName::DoesPackageExist(PackageName, &OutFilename))
			{
				return true;
			}
			if (FPackageName::TryConvertLongPackageNameToFilename(
				PackageName,
				OutFilename,
				FPackageName::GetAssetPackageExtension()))
			{
				return IFileManager::Get().FileExists(*OutFilename);
			}
			return false;
		}

		bool MaterialAssetExists(const FString& PackageName, const FString& AssetName)
		{
			FString UnusedFilename;
			if (MaterialGetExistingPackageFilename(PackageName, UnusedFilename))
			{
				return true;
			}
			return FindObject<UObject>(nullptr, *(PackageName + TEXT(".") + AssetName)) != nullptr;
		}

		void DeletePackageFiles(const FString& Filename)
		{
			const FString CompanionFiles[] = {
				Filename,
				FPaths::ChangeExtension(Filename, TEXT("uexp")),
				FPaths::ChangeExtension(Filename, TEXT("ubulk")),
				FPaths::ChangeExtension(Filename, TEXT("uptnl"))
			};
			for (const FString& File : CompanionFiles)
			{
				if (IFileManager::Get().FileExists(*File))
				{
					IFileManager::Get().Delete(*File, false, true, true);
				}
			}
		}

		void DiscardUnloadablePackage(const FString& PackageName)
		{
			FString Filename;
			const bool bHasFile = MaterialGetExistingPackageFilename(PackageName, Filename);
			UPackage* Package = FindPackage(nullptr, *PackageName);
			if (!Package && bHasFile)
			{
				Package = LoadPackage(nullptr, *PackageName, LOAD_NoWarn | LOAD_Quiet);
			}

			if (Package && Package->IsFullyLoaded() && bHasFile)
			{
				return;
			}

			if (!Package && !bHasFile)
			{
				return;
			}

			if (Package)
			{
				ResetLoaders(Package);
				TArray<UPackage*> ToUnload;
				ToUnload.Add(Package);
				FText UnloadError;
				UPackageTools::UnloadPackages(ToUnload, UnloadError, true);
			}

			if (bHasFile)
			{
				UE_LOG(LogPBRTextureLab, Log, TEXT("Removing unloadable parent material file: %s"), *Filename);
				DeletePackageFiles(Filename);
			}
		}

		void DiscardLegacyUnversionedParent(const FString& MountedRoot, const FString& CurrentPackageName)
		{
			const FString LegacyPackageName = GetLegacyParentPackageName(MountedRoot);
			if (LegacyPackageName == CurrentPackageName)
			{
				return;
			}

			UPackage* LegacyPackage = FindPackage(nullptr, *LegacyPackageName);
			if (LegacyPackage)
			{
				ResetLoaders(LegacyPackage);
				TArray<UPackage*> ToUnload;
				ToUnload.Add(LegacyPackage);
				FText UnloadError;
				UPackageTools::UnloadPackages(ToUnload, UnloadError, true);
			}

			FString LegacyFilename;
			if (MaterialGetExistingPackageFilename(LegacyPackageName, LegacyFilename))
			{
				UE_LOG(LogPBRTextureLab, Log, TEXT("Removing cross-version parent leftover: %s"), *LegacyFilename);
				DeletePackageFiles(LegacyFilename);
			}
		}

		template <typename T>
		T* AddExpression(UMaterial* Material, const int32 X, const int32 Y)
		{
			return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(Material, T::StaticClass(), X, Y));
		}

		bool Connect(
			UMaterialExpression* From,
			const TCHAR* FromOutput,
			UMaterialExpression* To,
			const TCHAR* ToInput,
			FString* OutError)
		{
			if (!UMaterialEditingLibrary::ConnectMaterialExpressions(From, FromOutput, To, ToInput))
			{
				MaterialSetError(OutError, FString::Printf(
					TEXT("Failed to connect %s.%s -> %s.%s"),
					From ? *From->GetClass()->GetName() : TEXT("null"),
					FromOutput,
					To ? *To->GetClass()->GetName() : TEXT("null"),
					ToInput));
				return false;
			}
			return true;
		}

		bool ConnectProperty(
			UMaterialExpression* From,
			const TCHAR* FromOutput,
			const EMaterialProperty Property,
			FString* OutError)
		{
			if (!UMaterialEditingLibrary::ConnectMaterialProperty(From, FromOutput, Property))
			{
				MaterialSetError(OutError, FString::Printf(TEXT("Failed to connect material property %d"), static_cast<int32>(Property)));
				return false;
			}
			return true;
		}

		bool SamplerMatchesTexture(const UMaterialExpressionTextureSampleParameter2D* Sample, const EMaterialSamplerType Expected)
		{
			return Sample
				&& Sample->SamplerType == Expected
				&& Sample->Texture
				&& GetSamplerTypeForTexture(Sample->Texture) == Expected;
		}

		UTexture* LoadEngineTexture(const TCHAR* ObjectPath)
		{
			return LoadObject<UTexture>(nullptr, ObjectPath, nullptr, LOAD_None, nullptr);
		}

		UTexture2D* CreatePluginSamplerTexture(const EMaterialSamplerType SamplerType)
		{
			FString Suffix = TEXT("Color");
			TextureCompressionSettings Compression = TC_Default;
			bool bSRGB = true;
			FColor Pixel(255, 255, 255, 255);
			switch (SamplerType)
			{
			case SAMPLERTYPE_Normal:
				Suffix = TEXT("Normal");
				Compression = TC_Normalmap;
				bSRGB = false;
				Pixel = FColor(128, 128, 255, 255);
				break;
			case SAMPLERTYPE_Masks:
				Suffix = TEXT("Masks");
				Compression = TC_Masks;
				bSRGB = false;
				Pixel = FColor(255, 128, 0, 255);
				break;
			case SAMPLERTYPE_LinearGrayscale:
				Suffix = TEXT("Gray");
				Compression = TC_Grayscale;
				bSRGB = false;
				Pixel = FColor(128, 128, 128, 255);
				break;
			default:
				break;
			}

			const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PBRTextureLab"));
			FString Root = Plugin.IsValid() ? Plugin->GetMountedAssetPath() : FString(TEXT("/PBRTextureLab/"));
			Root.ReplaceInline(TEXT("\\"), TEXT("/"));
			if (!Root.EndsWith(TEXT("/")))
			{
				Root += TEXT("/");
			}
			const FString AssetName = FString::Printf(
				TEXT("T_PBRDefault_%s_%d%d"),
				*Suffix,
				PBRTEXTURELAB_ENGINE_MAJOR,
				PBRTEXTURELAB_ENGINE_MINOR);
			const FString PackageName = Root + TEXT("Materials/") + AssetName;
			const FString ObjectPath = PackageName + TEXT(".") + AssetName;
			if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				if (GetSamplerTypeForTexture(Existing) == SamplerType)
				{
					return Existing;
				}
				DiscardUnloadablePackage(PackageName);
			}

			if (Plugin.IsValid())
			{
				IFileManager::Get().MakeDirectory(*(Plugin->GetContentDir() / TEXT("Materials")), true);
			}

			UPackage* Package = CreatePackage(*PackageName);
			if (!Package)
			{
				return nullptr;
			}
			if (!Package->IsFullyLoaded())
			{
				Package->MarkAsFullyLoaded();
			}

			UTexture2D* Texture = NewObject<UTexture2D>(Package, FName(*AssetName), RF_Public | RF_Standalone);
			const uint8 Bytes[4] = { Pixel.B, Pixel.G, Pixel.R, Pixel.A };
			Texture->Source.Init(1, 1, 1, 1, TSF_BGRA8, Bytes);
			Texture->CompressionSettings = Compression;
			Texture->SRGB = bSRGB;
			Texture->LODGroup = (SamplerType == SAMPLERTYPE_Normal) ? TEXTUREGROUP_WorldNormalMap : TEXTUREGROUP_World;
			Texture->UpdateResource();
			Texture->PostEditChange();
			FAssetRegistryModule::AssetCreated(Texture);
			Texture->MarkPackageDirty();
			TArray<UPackage*> Packages;
			Packages.Add(Package);
			UEditorLoadingAndSavingUtils::SavePackages(Packages, false);
			return Texture;
		}

		UTexture* ResolveSamplerDefaultTexture(const EMaterialSamplerType SamplerType)
		{
			const TCHAR* Path = TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture");
			switch (SamplerType)
			{
			case SAMPLERTYPE_Normal:
				Path = TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal");
				break;
			case SAMPLERTYPE_Masks:
				Path = TEXT("/Engine/EngineMaterials/DefaultDiffuse_TC_Masks.DefaultDiffuse_TC_Masks");
				break;
			case SAMPLERTYPE_LinearGrayscale:
				Path = TEXT("/Engine/EngineMaterials/BaseFlattenGrayscaleMap.BaseFlattenGrayscaleMap");
				break;
			default:
				break;
			}

			UTexture* Texture = LoadEngineTexture(Path);
			if (Texture && GetSamplerTypeForTexture(Texture) == SamplerType)
			{
				return Texture;
			}

			if (SamplerType == SAMPLERTYPE_LinearGrayscale)
			{
				Texture = LoadEngineTexture(TEXT("/Engine/EngineMaterials/DefaultCalibrationGrayscale.DefaultCalibrationGrayscale"));
				if (Texture && GetSamplerTypeForTexture(Texture) == SamplerType)
				{
					return Texture;
				}
			}

			return CreatePluginSamplerTexture(SamplerType);
		}

		void AssignSamplerDefault(UMaterialExpressionTextureSampleParameter2D* Sample, const EMaterialSamplerType SamplerType)
		{
			if (!Sample)
			{
				return;
			}
			Sample->SamplerType = SamplerType;
			if (UTexture* Texture = ResolveSamplerDefaultTexture(SamplerType))
			{
				Sample->Texture = Texture;
			}
			else
			{
				Sample->SetDefaultTexture();
			}
		}

		bool ParentHasExpectedParameters(UMaterial* Material)
		{
			if (!Material)
			{
				return false;
			}

			bool bHasBaseColor = false;
			bool bHasNormal = false;
			bool bHasRoughness = false;
			bool bHasMetallic = false;
			bool bHasAO = false;
			bool bHasHeight = false;
			bool bHasNormalStrength = false;
			bool bHasHeightAmount = false;
			bool bHasUvScale = false;
			bool bHasBaseColorTint = false;
			bool bHasRoughnessAmount = false;
			bool bHasSpecularAmount = false;
			bool bHasMetallicAmount = false;

			for (UMaterialExpression* Expression : Material->GetExpressions())
			{
				if (!Expression)
				{
					continue;
				}
				const FName ParameterName = Expression->HasAParameterName() ? Expression->GetParameterName() : NAME_None;
				if (const UMaterialExpressionTextureSampleParameter2D* Sample =
					Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
				{
					bHasBaseColor |= ParameterName == FName(PBRTEXTURELAB_PARAM_BaseColorTexture)
						&& SamplerMatchesTexture(Sample, SAMPLERTYPE_Color);
					bHasNormal |= ParameterName == FName(PBRTEXTURELAB_PARAM_NormalTexture)
						&& SamplerMatchesTexture(Sample, SAMPLERTYPE_Normal);
					bHasRoughness |= ParameterName == FName(PBRTEXTURELAB_PARAM_RoughnessTexture)
						&& SamplerMatchesTexture(Sample, SAMPLERTYPE_LinearGrayscale);
					bHasMetallic |= ParameterName == FName(PBRTEXTURELAB_PARAM_MetallicTexture)
						&& SamplerMatchesTexture(Sample, SAMPLERTYPE_LinearGrayscale);
					bHasAO |= ParameterName == FName(PBRTEXTURELAB_PARAM_AOTexture)
						&& SamplerMatchesTexture(Sample, SAMPLERTYPE_LinearGrayscale);
					bHasHeight |= ParameterName == FName(PBRTEXTURELAB_PARAM_HeightTexture)
						&& SamplerMatchesTexture(Sample, SAMPLERTYPE_LinearGrayscale);
				}
				else if (Cast<UMaterialExpressionVectorParameter>(Expression))
				{
					bHasBaseColorTint |= ParameterName == FName(PBRTEXTURELAB_PARAM_BaseColor);
				}
				else
				{
					bHasNormalStrength |= ParameterName == FName(PBRTEXTURELAB_PARAM_NormalStrength);
					bHasHeightAmount |= ParameterName == FName(PBRTEXTURELAB_PARAM_HeightAmount);
					bHasUvScale |= ParameterName == FName(PBRTEXTURELAB_PARAM_UVScale);
					bHasRoughnessAmount |= ParameterName == FName(PBRTEXTURELAB_PARAM_Roughness);
					bHasSpecularAmount |= ParameterName == FName(PBRTEXTURELAB_PARAM_Specular);
					bHasMetallicAmount |= ParameterName == FName(PBRTEXTURELAB_PARAM_Metallic);
				}
			}

			return bHasBaseColor && bHasNormal && bHasRoughness && bHasMetallic && bHasAO && bHasHeight
				&& bHasNormalStrength && bHasHeightAmount && bHasUvScale
				&& bHasBaseColorTint && bHasRoughnessAmount && bHasSpecularAmount && bHasMetallicAmount
				&& FMath::IsNearlyEqual(
					UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Material, PBRTEXTURELAB_PARAM_UVScale),
					1.0f)
				&& FMath::IsNearlyEqual(
					UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Material, PBRTEXTURELAB_PARAM_NormalStrength),
					0.1f);
		}

		bool BuildParentGraph(UMaterial* Material, FString* OutError)
		{
			Material->MaterialDomain = MD_Surface;
			Material->BlendMode = BLEND_Opaque;
			Material->SetShadingModel(MSM_DefaultLit);
			Material->bUseMaterialAttributes = false;

			UMaterialExpressionTextureCoordinate* TexCoord = AddExpression<UMaterialExpressionTextureCoordinate>(Material, -1200, 0);
			UMaterialExpressionScalarParameter* UvScale = AddExpression<UMaterialExpressionScalarParameter>(Material, -1200, 160);
			UMaterialExpressionMultiply* ScaledUv = AddExpression<UMaterialExpressionMultiply>(Material, -900, 0);
			UMaterialExpressionTextureSampleParameter2D* HeightSample = AddExpression<UMaterialExpressionTextureSampleParameter2D>(Material, -700, 280);
			UMaterialExpressionScalarParameter* HeightAmount = AddExpression<UMaterialExpressionScalarParameter>(Material, -700, 480);
			UMaterialExpressionBumpOffset* Bump = AddExpression<UMaterialExpressionBumpOffset>(Material, -400, 80);
			UMaterialExpressionTextureSampleParameter2D* BaseColor = AddExpression<UMaterialExpressionTextureSampleParameter2D>(Material, -100, -200);
			UMaterialExpressionTextureSampleParameter2D* NormalSample = AddExpression<UMaterialExpressionTextureSampleParameter2D>(Material, -100, 80);
			UMaterialExpressionTextureSampleParameter2D* RoughnessSample = AddExpression<UMaterialExpressionTextureSampleParameter2D>(Material, -100, 360);
			UMaterialExpressionTextureSampleParameter2D* MetallicSample = AddExpression<UMaterialExpressionTextureSampleParameter2D>(Material, -100, 560);
			UMaterialExpressionTextureSampleParameter2D* AoSample = AddExpression<UMaterialExpressionTextureSampleParameter2D>(Material, -100, 760);
			UMaterialExpressionScalarParameter* NormalStrength = AddExpression<UMaterialExpressionScalarParameter>(Material, 160, 600);
			UMaterialExpressionVectorParameter* BaseTint = AddExpression<UMaterialExpressionVectorParameter>(Material, 160, -280);
			UMaterialExpressionMultiply* BaseMul = AddExpression<UMaterialExpressionMultiply>(Material, 400, -200);
			UMaterialExpressionScalarParameter* RoughnessAmount = AddExpression<UMaterialExpressionScalarParameter>(Material, 160, 360);
			UMaterialExpressionMultiply* RoughMul = AddExpression<UMaterialExpressionMultiply>(Material, 400, 360);
			UMaterialExpressionScalarParameter* SpecularAmount = AddExpression<UMaterialExpressionScalarParameter>(Material, 160, 200);
			UMaterialExpressionScalarParameter* MetallicAmount = AddExpression<UMaterialExpressionScalarParameter>(Material, 160, 560);
			UMaterialExpressionMultiply* MetalMul = AddExpression<UMaterialExpressionMultiply>(Material, 400, 560);
			UMaterialExpressionConstant3Vector* FlatNormal = AddExpression<UMaterialExpressionConstant3Vector>(Material, 160, -40);
			UMaterialExpressionLinearInterpolate* NormalBlend = AddExpression<UMaterialExpressionLinearInterpolate>(Material, 360, 80);
			UMaterialExpressionNormalize* NormalOut = AddExpression<UMaterialExpressionNormalize>(Material, 600, 80);

			if (!TexCoord || !UvScale || !ScaledUv || !HeightSample || !HeightAmount || !Bump
				|| !BaseColor || !NormalSample || !RoughnessSample || !MetallicSample || !AoSample || !NormalStrength
				|| !BaseTint || !BaseMul || !RoughnessAmount || !RoughMul || !SpecularAmount || !MetallicAmount || !MetalMul
				|| !FlatNormal || !NormalBlend || !NormalOut)
			{
				MaterialSetError(OutError, TEXT("Failed to create parent material expressions."));
				return false;
			}

			TexCoord->CoordinateIndex = 0;

			UvScale->ParameterName = PBRTEXTURELAB_PARAM_UVScale;
			UvScale->DefaultValue = 1.0f;
			UvScale->Group = PBRTEXTURELAB_GROUP_UV;
			UvScale->SortPriority = 8;

			HeightAmount->ParameterName = PBRTEXTURELAB_PARAM_HeightAmount;
			HeightAmount->DefaultValue = 0.0f;
			HeightAmount->Group = PBRTEXTURELAB_GROUP_Normal;
			HeightAmount->SortPriority = 7;

			NormalStrength->ParameterName = PBRTEXTURELAB_PARAM_NormalStrength;
			NormalStrength->DefaultValue = 0.1f;
			NormalStrength->Group = PBRTEXTURELAB_GROUP_Normal;
			NormalStrength->SortPriority = 4;

			BaseTint->ParameterName = PBRTEXTURELAB_PARAM_BaseColor;
			BaseTint->DefaultValue = FLinearColor::White;
			BaseTint->Group = PBRTEXTURELAB_GROUP_Base;
			BaseTint->SortPriority = 1;

			RoughnessAmount->ParameterName = PBRTEXTURELAB_PARAM_Roughness;
			RoughnessAmount->DefaultValue = 1.0f;
			RoughnessAmount->Group = PBRTEXTURELAB_GROUP_Rough;
			RoughnessAmount->SortPriority = 1;

			SpecularAmount->ParameterName = PBRTEXTURELAB_PARAM_Specular;
			SpecularAmount->DefaultValue = 0.5f;
			SpecularAmount->Group = PBRTEXTURELAB_GROUP_Specular;
			SpecularAmount->SortPriority = 0;

			MetallicAmount->ParameterName = PBRTEXTURELAB_PARAM_Metallic;
			MetallicAmount->DefaultValue = 1.0f;
			MetallicAmount->Group = PBRTEXTURELAB_GROUP_Metal;
			MetallicAmount->SortPriority = 1;

			HeightSample->ParameterName = PBRTEXTURELAB_PARAM_HeightTexture;
			HeightSample->Group = PBRTEXTURELAB_GROUP_Normal;
			HeightSample->SortPriority = 5;
			AssignSamplerDefault(HeightSample, SAMPLERTYPE_LinearGrayscale);

			BaseColor->ParameterName = PBRTEXTURELAB_PARAM_BaseColorTexture;
			BaseColor->Group = PBRTEXTURELAB_GROUP_Base;
			BaseColor->SortPriority = 0;
			AssignSamplerDefault(BaseColor, SAMPLERTYPE_Color);

			NormalSample->ParameterName = PBRTEXTURELAB_PARAM_NormalTexture;
			NormalSample->Group = PBRTEXTURELAB_GROUP_Normal;
			NormalSample->SortPriority = 1;
			AssignSamplerDefault(NormalSample, SAMPLERTYPE_Normal);

			RoughnessSample->ParameterName = PBRTEXTURELAB_PARAM_RoughnessTexture;
			RoughnessSample->Group = PBRTEXTURELAB_GROUP_Rough;
			RoughnessSample->SortPriority = 0;
			AssignSamplerDefault(RoughnessSample, SAMPLERTYPE_LinearGrayscale);

			MetallicSample->ParameterName = PBRTEXTURELAB_PARAM_MetallicTexture;
			MetallicSample->Group = PBRTEXTURELAB_GROUP_Metal;
			MetallicSample->SortPriority = 0;
			AssignSamplerDefault(MetallicSample, SAMPLERTYPE_LinearGrayscale);

			AoSample->ParameterName = PBRTEXTURELAB_PARAM_AOTexture;
			AoSample->Group = PBRTEXTURELAB_PARAM_GROUP;
			AoSample->SortPriority = 4;
			AssignSamplerDefault(AoSample, SAMPLERTYPE_LinearGrayscale);

			FlatNormal->Constant = FLinearColor(0.0f, 0.0f, 1.0f, 0.0f);

			// TextureSample pin "Coordinates" is exposed to MaterialEditingLibrary as "UVs".
			if (!Connect(TexCoord, TEXT(""), ScaledUv, TEXT("A"), OutError)
				|| !Connect(UvScale, TEXT(""), ScaledUv, TEXT("B"), OutError)
				|| !Connect(ScaledUv, TEXT(""), HeightSample, TEXT("UVs"), OutError)
				|| !Connect(ScaledUv, TEXT(""), Bump, TEXT("Coordinate"), OutError)
				|| !Connect(HeightSample, TEXT("R"), Bump, TEXT("Height"), OutError)
				|| !Connect(HeightAmount, TEXT(""), Bump, TEXT("HeightRatioInput"), OutError)
				|| !Connect(Bump, TEXT(""), BaseColor, TEXT("UVs"), OutError)
				|| !Connect(Bump, TEXT(""), NormalSample, TEXT("UVs"), OutError)
				|| !Connect(Bump, TEXT(""), RoughnessSample, TEXT("UVs"), OutError)
				|| !Connect(Bump, TEXT(""), MetallicSample, TEXT("UVs"), OutError)
				|| !Connect(Bump, TEXT(""), AoSample, TEXT("UVs"), OutError)
				|| !Connect(FlatNormal, TEXT(""), NormalBlend, TEXT("A"), OutError)
				|| !Connect(NormalSample, TEXT("RGB"), NormalBlend, TEXT("B"), OutError)
				|| !Connect(NormalStrength, TEXT(""), NormalBlend, TEXT("Alpha"), OutError)
				|| !Connect(NormalBlend, TEXT(""), NormalOut, TEXT(""), OutError)
				|| !Connect(BaseColor, TEXT("RGB"), BaseMul, TEXT("A"), OutError)
				|| !Connect(BaseTint, TEXT(""), BaseMul, TEXT("B"), OutError)
				|| !Connect(RoughnessSample, TEXT("R"), RoughMul, TEXT("A"), OutError)
				|| !Connect(RoughnessAmount, TEXT(""), RoughMul, TEXT("B"), OutError)
				|| !Connect(MetallicSample, TEXT("R"), MetalMul, TEXT("A"), OutError)
				|| !Connect(MetallicAmount, TEXT(""), MetalMul, TEXT("B"), OutError)
				|| !ConnectProperty(BaseMul, TEXT(""), MP_BaseColor, OutError)
				|| !ConnectProperty(NormalOut, TEXT(""), MP_Normal, OutError)
				|| !ConnectProperty(RoughMul, TEXT(""), MP_Roughness, OutError)
				|| !ConnectProperty(MetalMul, TEXT(""), MP_Metallic, OutError)
				|| !ConnectProperty(SpecularAmount, TEXT(""), MP_Specular, OutError)
				|| !ConnectProperty(AoSample, TEXT("R"), MP_AmbientOcclusion, OutError))
			{
				return false;
			}

			UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
			return true;
		}

		TArray<FString> RecompileParent(UMaterial* Material)
		{
#if PBRTEXTURELAB_UE_5_8_OR_LATER
			return UMaterialEditingLibrary::RecompileMaterial(Material);
#else
			UMaterialEditingLibrary::RecompileMaterial(Material);
			return TArray<FString>();
#endif
		}

		FName FindExistingParameter(const TArray<FName>& Names, std::initializer_list<const TCHAR*> Aliases)
		{
			for (const TCHAR* Alias : Aliases)
			{
				const FName Candidate(Alias);
				if (Names.Contains(Candidate))
				{
					return Candidate;
				}
			}
			for (const TCHAR* Alias : Aliases)
			{
				const FString AliasString(Alias);
				for (const FName& Name : Names)
				{
					if (Name.ToString().Equals(AliasString, ESearchCase::IgnoreCase))
					{
						return Name;
					}
				}
			}
			return NAME_None;
		}

		enum class EParentMapChannel : uint8
		{
			None = 0,
			BaseColor,
			Normal,
			Roughness,
			Metallic,
			Height,
			AO
		};

		bool NameHasToken(const FString& Name, const TCHAR* Token)
		{
			return Name.Contains(Token, ESearchCase::IgnoreCase);
		}

		EParentMapChannel ClassifyParentMapChannel(const FString& Name)
		{
			if (NameHasToken(Name, TEXT("污垢")) || NameHasToken(Name, TEXT("发光")) || NameHasToken(Name, TEXT("高光"))
				|| NameHasToken(Name, TEXT("缺陷")) || NameHasToken(Name, TEXT("衰减")))
			{
				return EParentMapChannel::None;
			}
			if (NameHasToken(Name, TEXT("基础贴图")) || NameHasToken(Name, TEXT("基础颜色"))
				|| NameHasToken(Name, TEXT("BaseColor")) || NameHasToken(Name, TEXT("Albedo"))
				|| NameHasToken(Name, TEXT("Diffuse")))
			{
				return EParentMapChannel::BaseColor;
			}
			if (NameHasToken(Name, TEXT("法线")) || NameHasToken(Name, TEXT("Normal")))
			{
				return EParentMapChannel::Normal;
			}
			if (NameHasToken(Name, TEXT("粗糙")) || NameHasToken(Name, TEXT("Rough")))
			{
				return EParentMapChannel::Roughness;
			}
			if (NameHasToken(Name, TEXT("金属")) || NameHasToken(Name, TEXT("Metal")))
			{
				return EParentMapChannel::Metallic;
			}
			if (NameHasToken(Name, TEXT("置换")) || NameHasToken(Name, TEXT("高度"))
				|| NameHasToken(Name, TEXT("Height")) || NameHasToken(Name, TEXT("Disp")))
			{
				return EParentMapChannel::Height;
			}
			if (NameHasToken(Name, TEXT("AO")) || NameHasToken(Name, TEXT("AmbientOcclusion"))
				|| NameHasToken(Name, TEXT("环境光")) || NameHasToken(Name, TEXT("遮蔽")))
			{
				return EParentMapChannel::AO;
			}
			return EParentMapChannel::None;
		}

		UTexture* TextureForChannel(const FPBRImportedTextures& Textures, const EParentMapChannel Channel)
		{
			switch (Channel)
			{
			case EParentMapChannel::BaseColor:
				return Textures.BaseColor;
			case EParentMapChannel::Normal:
				return Textures.Normal;
			case EParentMapChannel::Roughness:
				return Textures.Roughness;
			case EParentMapChannel::Metallic:
				return Textures.Metallic;
			case EParentMapChannel::Height:
				return Textures.Height;
			case EParentMapChannel::AO:
				return Textures.AO;
			default:
				return nullptr;
			}
		}

		bool ChannelHasTexture(const FPBRImportedTextures& Textures, const EParentMapChannel Channel)
		{
			return TextureForChannel(Textures, Channel) != nullptr;
		}

		bool ChannelEnabled(const FPBRMapExportFlags& Flags, const EParentMapChannel Channel)
		{
			switch (Channel)
			{
			case EParentMapChannel::BaseColor:
				return Flags.bBaseColor;
			case EParentMapChannel::Normal:
				return Flags.bNormal;
			case EParentMapChannel::Roughness:
				return Flags.bRoughness;
			case EParentMapChannel::Metallic:
				return Flags.bMetallic;
			case EParentMapChannel::Height:
				return Flags.bHeight;
			case EParentMapChannel::AO:
				return Flags.bAO;
			default:
				return true;
			}
		}

		bool ChannelShouldUseTexture(
			const FPBRImportedTextures& Textures,
			const FPBRMaterialInstanceRequest& Request,
			const EParentMapChannel Channel)
		{
			return Channel != EParentMapChannel::None
				&& ChannelEnabled(Request.EnabledMaps, Channel)
				&& ChannelHasTexture(Textures, Channel);
		}

		float StrengthForChannel(const FPBRMaterialInstanceRequest& Request, const EParentMapChannel Channel)
		{
			switch (Channel)
			{
			case EParentMapChannel::Normal:
				return Request.NormalStrength;
			case EParentMapChannel::Roughness:
				return Request.RoughnessStrength;
			case EParentMapChannel::Metallic:
				return Request.MetallicStrength;
			case EParentMapChannel::Height:
				return Request.HeightAmount;
			default:
				return 1.0f;
			}
		}

		bool IsUvScaleParameter(const FString& Name)
		{
			return NameHasToken(Name, TEXT("UV缩放")) || NameHasToken(Name, TEXT("UV大小"))
				|| NameHasToken(Name, TEXT("UVScale")) || Name.Equals(TEXT("整体UV"));
		}

		bool IsTextureSlotName(const FString& Name)
		{
			return !NameHasToken(Name, TEXT("开关")) && !NameHasToken(Name, TEXT("单独"))
				&& !NameHasToken(Name, TEXT("灰度")) && !NameHasToken(Name, TEXT("类型"))
				&& !NameHasToken(Name, TEXT("对比")) && !NameHasToken(Name, TEXT("明度"))
				&& !NameHasToken(Name, TEXT("强度")) && !NameHasToken(Name, TEXT("旋转"));
		}

		void AppendUniqueNames(TArray<FName>& Dest, const TArray<FName>& Src)
		{
			for (const FName& Name : Src)
			{
				Dest.AddUnique(Name);
			}
		}

		void CollectExpressionParameterNames(
			UMaterialInterface* Material,
			TArray<FName>& TextureParams,
			TArray<FName>& ScalarParams,
			TArray<FName>& VectorParams,
			TArray<FName>& SwitchParams)
		{
			UMaterial* Base = Material ? Material->GetMaterial() : nullptr;
			if (!Base)
			{
				return;
			}
			for (UMaterialExpression* Expression : Base->GetExpressions())
			{
				if (!Expression || !Expression->HasAParameterName())
				{
					continue;
				}
				const FName Name = Expression->GetParameterName();
				if (Name.IsNone())
				{
					continue;
				}
				if (Expression->IsA<UMaterialExpressionTextureSampleParameter2D>())
				{
					TextureParams.AddUnique(Name);
				}
				else if (Expression->IsA<UMaterialExpressionScalarParameter>())
				{
					ScalarParams.AddUnique(Name);
				}
				else if (Expression->IsA<UMaterialExpressionVectorParameter>())
				{
					VectorParams.AddUnique(Name);
				}
				else if (Expression->IsA<UMaterialExpressionStaticBoolParameter>())
				{
					SwitchParams.AddUnique(Name);
				}
			}
		}

		void CollectParentParameterNames(
			UMaterialInterface* Material,
			TArray<FName>& TextureParams,
			TArray<FName>& ScalarParams,
			TArray<FName>& VectorParams,
			TArray<FName>& SwitchParams)
		{
			TextureParams.Reset();
			ScalarParams.Reset();
			VectorParams.Reset();
			SwitchParams.Reset();
			if (!Material)
			{
				return;
			}

			TArray<FName> LibraryTextures;
			TArray<FName> LibraryScalars;
			TArray<FName> LibraryVectors;
			TArray<FName> LibrarySwitches;
			UMaterialEditingLibrary::GetTextureParameterNames(Material, LibraryTextures);
			UMaterialEditingLibrary::GetScalarParameterNames(Material, LibraryScalars);
			UMaterialEditingLibrary::GetVectorParameterNames(Material, LibraryVectors);
			UMaterialEditingLibrary::GetStaticSwitchParameterNames(Material, LibrarySwitches);
			AppendUniqueNames(TextureParams, LibraryTextures);
			AppendUniqueNames(ScalarParams, LibraryScalars);
			AppendUniqueNames(VectorParams, LibraryVectors);
			AppendUniqueNames(SwitchParams, LibrarySwitches);
			CollectExpressionParameterNames(Material, TextureParams, ScalarParams, VectorParams, SwitchParams);
		}

		void AppendStaticSwitchInfos(UMaterialInterface* Material, TArray<FMaterialParameterInfo>& OutInfos)
		{
			if (!Material)
			{
				return;
			}
			TArray<FMaterialParameterInfo> Infos;
			TArray<FGuid> Guids;
			Material->GetAllStaticSwitchParameterInfo(Infos, Guids);
			for (const FMaterialParameterInfo& Info : Infos)
			{
				bool bExists = false;
				for (const FMaterialParameterInfo& Existing : OutInfos)
				{
					if (Existing.Name == Info.Name
						&& Existing.Association == Info.Association
						&& Existing.Index == Info.Index)
					{
						bExists = true;
						break;
					}
				}
				if (!bExists)
				{
					OutInfos.Add(Info);
				}
			}
		}

		int32 TextureTitleScore(const FString& Name, const EParentMapChannel Channel, const TCHAR* Preferred)
		{
			if (ClassifyParentMapChannel(Name) != Channel || !IsTextureSlotName(Name))
			{
				return -1;
			}
			int32 Score = 10;
			if (Name.Equals(Preferred, ESearchCase::IgnoreCase))
			{
				Score = 100;
			}
			if (NameHasToken(Name, TEXT("贴图")))
			{
				Score += 20;
			}
			return Score;
		}

		int32 ScalarTitleScore(const FString& Name, const EParentMapChannel Channel, const TCHAR* Token, const TCHAR* Preferred)
		{
			if (NameHasToken(Name, TEXT("开关")) || NameHasToken(Name, TEXT("单独")) || NameHasToken(Name, TEXT("旋转")))
			{
				return -1;
			}
			if (!NameHasToken(Name, Token))
			{
				return -1;
			}
			if (Channel != EParentMapChannel::None && ClassifyParentMapChannel(Name) != Channel)
			{
				return -1;
			}
			int32 Score = 10;
			if (Name.Equals(Preferred, ESearchCase::IgnoreCase))
			{
				Score = 100;
			}
			else if (Name.StartsWith(Preferred))
			{
				Score = 40;
			}
			return Score;
		}

		FString PickBestName(const TArray<FName>& Names, const TFunctionRef<int32(const FString&)>& ScoreFn, const TCHAR* Fallback)
		{
			FString Best = Fallback;
			int32 BestScore = -1;
			for (const FName& Name : Names)
			{
				const FString Text = Name.ToString();
				const int32 Score = ScoreFn(Text);
				if (Score > BestScore)
				{
					BestScore = Score;
					Best = Text;
				}
			}
			return Best;
		}

		void BindGeneratedTextures(UMaterialInstanceConstant* Instance, const FPBRImportedTextures& Textures, const FPBRMaterialInstanceRequest& Request)
		{
			if (!Instance)
			{
				return;
			}

			TArray<FName> TextureParams;
			TArray<FName> ScalarParams;
			TArray<FName> VectorParams;
			TArray<FName> SwitchParams;
			CollectParentParameterNames(Instance, TextureParams, ScalarParams, VectorParams, SwitchParams);
			if (Instance->Parent)
			{
				TArray<FName> ParentTextures;
				TArray<FName> ParentScalars;
				TArray<FName> ParentVectors;
				TArray<FName> ParentSwitches;
				CollectParentParameterNames(Instance->Parent, ParentTextures, ParentScalars, ParentVectors, ParentSwitches);
				AppendUniqueNames(TextureParams, ParentTextures);
				AppendUniqueNames(ScalarParams, ParentScalars);
				AppendUniqueNames(VectorParams, ParentVectors);
				AppendUniqueNames(SwitchParams, ParentSwitches);
			}

			bool bBound[7] = {};
			for (const FName& TextureName : TextureParams)
			{
				const EParentMapChannel Channel = ClassifyParentMapChannel(TextureName.ToString());
				if (!ChannelShouldUseTexture(Textures, Request, Channel))
				{
					continue;
				}
				UTexture* Texture = TextureForChannel(Textures, Channel);
				if (!Texture)
				{
					continue;
				}
				SetMaterialInstanceTexture(Instance, TextureName, Texture);
				bBound[static_cast<uint8>(Channel)] = true;
			}

			auto IsUserEditableLookParam = [](const FString& Name)
			{
				return Name.Equals(TEXT("基础色")) || Name.Equals(TEXT("粗糙度"))
					|| Name.Equals(TEXT("高光度")) || Name.Equals(TEXT("金属度"))
					|| Name.Equals(PBRTEXTURELAB_PARAM_BaseColor)
					|| Name.Equals(PBRTEXTURELAB_PARAM_Roughness)
					|| Name.Equals(PBRTEXTURELAB_PARAM_Specular)
					|| Name.Equals(PBRTEXTURELAB_PARAM_Metallic);
			};

			for (const FName& ScalarName : ScalarParams)
			{
				const FString Name = ScalarName.ToString();
				if (IsUserEditableLookParam(Name))
				{
					continue;
				}
				const EParentMapChannel Channel = ClassifyParentMapChannel(Name);
				if (NameHasToken(Name, TEXT("开关")))
				{
					if (NameHasToken(Name, TEXT("反向")) || NameHasToken(Name, TEXT("类型切换"))
						|| NameHasToken(Name, TEXT("单独")))
					{
						continue;
					}
					if (Channel != EParentMapChannel::None)
					{
						SetMaterialInstanceScalar(
							Instance,
							ScalarName,
							ChannelShouldUseTexture(Textures, Request, Channel) ? 1.0f : 0.0f);
					}
					continue;
				}
				if (NameHasToken(Name, TEXT("强度")) && Channel != EParentMapChannel::None)
				{
					if (ChannelShouldUseTexture(Textures, Request, Channel))
					{
						SetMaterialInstanceScalar(Instance, ScalarName, StrengthForChannel(Request, Channel));
					}
					else if (Channel == EParentMapChannel::Height)
					{
						SetMaterialInstanceScalar(Instance, ScalarName, 0.0f);
					}
					continue;
				}
				if (NameHasToken(Name, TEXT("明度")) && Channel == EParentMapChannel::Roughness)
				{
					if (ChannelShouldUseTexture(Textures, Request, Channel))
					{
						SetMaterialInstanceScalar(Instance, ScalarName, Request.RoughnessBrightness);
					}
					continue;
				}
				if (IsUvScaleParameter(Name) && !NameHasToken(Name, TEXT("单独")) && !NameHasToken(Name, TEXT("旋转")))
				{
					SetMaterialInstanceScalar(Instance, ScalarName, Request.UVScale);
				}
			}

			for (const FName& VectorName : VectorParams)
			{
				const FString Name = VectorName.ToString();
				if (IsUserEditableLookParam(Name))
				{
					continue;
				}
				if (IsUvScaleParameter(Name) && !NameHasToken(Name, TEXT("单独")) && !NameHasToken(Name, TEXT("旋转")))
				{
					SetMaterialInstanceVector(
						Instance,
						VectorName,
						FLinearColor(Request.UVScale, Request.UVScale, 0.0f, 1.0f));
				}
			}

			TArray<FMaterialParameterInfo> SwitchInfos;
			AppendStaticSwitchInfos(Instance, SwitchInfos);
			if (Instance->Parent)
			{
				AppendStaticSwitchInfos(Instance->Parent, SwitchInfos);
			}
			for (const FName& SwitchName : SwitchParams)
			{
				bool bExists = false;
				for (const FMaterialParameterInfo& Existing : SwitchInfos)
				{
					if (Existing.Name == SwitchName)
					{
						bExists = true;
						break;
					}
				}
				if (!bExists)
				{
					SwitchInfos.Add(FMaterialParameterInfo(SwitchName));
				}
			}

			for (const FMaterialParameterInfo& SwitchInfo : SwitchInfos)
			{
				const FString Name = SwitchInfo.Name.ToString();
				if (NameHasToken(Name, TEXT("单独UV")) || NameHasToken(Name, TEXT("自发光"))
					|| NameHasToken(Name, TEXT("污垢")) || NameHasToken(Name, TEXT("缺陷"))
					|| NameHasToken(Name, TEXT("高光")) || NameHasToken(Name, TEXT("发光"))
					|| NameHasToken(Name, TEXT("反向")) || NameHasToken(Name, TEXT("类型切换")))
				{
					continue;
				}
				const EParentMapChannel Channel = ClassifyParentMapChannel(Name);
				if (Channel != EParentMapChannel::None)
				{
					SetMaterialInstanceStaticSwitch(
						Instance,
						SwitchInfo,
						ChannelShouldUseTexture(Textures, Request, Channel));
				}
			}

			UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
		}

		bool SaveAssetPackage(UObject* Asset, FString* OutError)
		{
			if (!Asset)
			{
				MaterialSetError(OutError, TEXT("Cannot save a null material asset."));
				return false;
			}
			TArray<UPackage*> Packages;
			Packages.Add(Asset->GetOutermost());
			if (!UEditorLoadingAndSavingUtils::SavePackages(Packages, false))
			{
				MaterialSetError(OutError, FString::Printf(TEXT("Failed to save %s"), *Asset->GetPathName()));
				return false;
			}
			return true;
		}
	}

	FString FPBRParentParamTitles::Get(const EPBRParentParamSlot Slot) const
	{
		switch (Slot)
		{
		case EPBRParentParamSlot::BaseColorTexture: return BaseColorTexture;
		case EPBRParentParamSlot::NormalTexture: return NormalTexture;
		case EPBRParentParamSlot::RoughnessTexture: return RoughnessTexture;
		case EPBRParentParamSlot::MetallicTexture: return MetallicTexture;
		case EPBRParentParamSlot::HeightTexture: return HeightTexture;
		case EPBRParentParamSlot::AOTexture: return AOTexture;
		case EPBRParentParamSlot::NormalStrength: return NormalStrength;
		case EPBRParentParamSlot::RoughnessStrength: return RoughnessStrength;
		case EPBRParentParamSlot::MetallicStrength: return MetallicStrength;
		case EPBRParentParamSlot::HeightAmount: return HeightAmount;
		case EPBRParentParamSlot::UVScale: return UVScale;
		case EPBRParentParamSlot::RoughnessBrightness: return RoughnessBrightness;
		default: return FString();
		}
	}

	FPBRParentParamTitles InspectParentParamTitles(UMaterialInterface* Parent)
	{
		FPBRParentParamTitles Titles;
		if (!Parent)
		{
			return Titles;
		}

		TArray<FName> TextureParams;
		TArray<FName> ScalarParams;
		TArray<FName> VectorParams;
		TArray<FName> SwitchParams;
		CollectParentParameterNames(Parent, TextureParams, ScalarParams, VectorParams, SwitchParams);

		auto PickTexture = [&TextureParams](const EParentMapChannel Channel, const TCHAR* Preferred) -> FString
		{
			return PickBestName(TextureParams, [Channel, Preferred](const FString& Name)
			{
				return TextureTitleScore(Name, Channel, Preferred);
			}, Preferred);
		};
		auto PickScalar = [&ScalarParams, &VectorParams](const EParentMapChannel Channel, const TCHAR* Token, const TCHAR* Preferred) -> FString
		{
			const FString FromScalar = PickBestName(ScalarParams, [Channel, Token, Preferred](const FString& Name)
			{
				return ScalarTitleScore(Name, Channel, Token, Preferred);
			}, Preferred);
			if (!FromScalar.Equals(Preferred))
			{
				return FromScalar;
			}
			return PickBestName(VectorParams, [Channel, Token, Preferred](const FString& Name)
			{
				return ScalarTitleScore(Name, Channel, Token, Preferred);
			}, Preferred);
		};

		Titles.BaseColorTexture = PickTexture(EParentMapChannel::BaseColor, TEXT("基础贴图"));
		Titles.NormalTexture = PickTexture(EParentMapChannel::Normal, TEXT("法线贴图"));
		Titles.RoughnessTexture = PickTexture(EParentMapChannel::Roughness, TEXT("粗糙贴图"));
		Titles.MetallicTexture = PickTexture(EParentMapChannel::Metallic, TEXT("金属贴图"));
		Titles.HeightTexture = PickTexture(EParentMapChannel::Height, TEXT("置换贴图"));
		Titles.AOTexture = PickTexture(EParentMapChannel::AO, TEXT("AO"));
		Titles.NormalStrength = PickScalar(EParentMapChannel::Normal, TEXT("强度"), TEXT("法线强度"));
		Titles.RoughnessStrength = PickScalar(EParentMapChannel::Roughness, TEXT("强度"), TEXT("粗糙强度"));
		Titles.MetallicStrength = PickScalar(EParentMapChannel::Metallic, TEXT("强度"), TEXT("金属强度"));
		Titles.HeightAmount = PickScalar(EParentMapChannel::Height, TEXT("强度"), TEXT("置换强度"));
		Titles.UVScale = PickScalar(EParentMapChannel::None, TEXT("UV缩放"), TEXT("UV缩放"));
		if (Titles.UVScale.Equals(TEXT("UV缩放")))
		{
			Titles.UVScale = PickScalar(EParentMapChannel::None, TEXT("UV大小"), TEXT("UV缩放"));
		}
		Titles.RoughnessBrightness = PickScalar(EParentMapChannel::Roughness, TEXT("明度"), TEXT("粗糙明度"));
		return Titles;
	}

	FString GetBundledParentsFolder()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PBRTextureLab"));
		FString Root = Plugin.IsValid() ? Plugin->GetMountedAssetPath() : FString(TEXT("/PBRTextureLab/"));
		Root.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (!Root.EndsWith(TEXT("/")))
		{
			Root += TEXT("/");
		}
		return Root + TEXT("Parents");
	}

	TArray<FPBRBundledParentDesc> GetBundledParentCatalog()
	{
		static const TCHAR* Names[] = {
			TEXT("000基础材质"),
			TEXT("000基础布纹"),
			TEXT("000基础布纹1混合法线"),
			TEXT("000基础次表面"),
			TEXT("000基础窗纱"),
			TEXT("000基础自发光呼吸"),
			TEXT("000基础贴花"),
			TEXT("000基础透光布纹"),
			TEXT("000基础镂空发光"),
			TEXT("000天空盒"),
			TEXT("00基础贴花粗糙"),
			TEXT("0自发光运动2"),
			TEXT("0运动")
		};

		TArray<FPBRBundledParentDesc> Catalog;
		const FString Folder = GetBundledParentsFolder();
		for (const TCHAR* Name : Names)
		{
			FPBRBundledParentDesc Entry;
			Entry.DisplayName = Name;
			Entry.ObjectPath = Folder / Name + TEXT(".") + Name;
			Catalog.Add(MoveTemp(Entry));
		}
		return Catalog;
	}

	UMaterialInterface* LoadBundledParentByName(const FString& DisplayName)
	{
		if (DisplayName.IsEmpty())
		{
			return nullptr;
		}
		for (const FPBRBundledParentDesc& Entry : GetBundledParentCatalog())
		{
			if (!Entry.DisplayName.Equals(DisplayName))
			{
				continue;
			}
			if (UMaterialInterface* Found = LoadObject<UMaterialInterface>(
				nullptr, *Entry.ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return Found;
			}
		}
		return nullptr;
	}

	UMaterialInterface* FindPreferredUserParent()
	{
		if (UMaterialInterface* Bundled = LoadBundledParentByName(TEXT("000基础材质")))
		{
			return Bundled;
		}
		if (UMaterialInterface* BundledCloth = LoadBundledParentByName(TEXT("000基础布纹")))
		{
			return BundledCloth;
		}

		static const TCHAR* ExactPaths[] = {
			TEXT("/Game/材质/MAT/000基础材质.000基础材质"),
			TEXT("/Game/材质/MAT/000基础材质"),
			TEXT("/Game/材质/MAT/000基础布纹.000基础布纹"),
			TEXT("/Game/材质/MAT/000基础布纹")
		};
		for (const TCHAR* Path : ExactPaths)
		{
			if (UMaterialInterface* Found = LoadObject<UMaterialInterface>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return Found;
			}
		}

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPath(TEXT("/Game"), Assets, true, false);
		UMaterialInterface* ClothParent = nullptr;
		for (const FAssetData& Asset : Assets)
		{
			if (!Asset.IsInstanceOf(UMaterialInterface::StaticClass()))
			{
				continue;
			}
			if (Asset.AssetName == FName(TEXT("000基础材质")))
			{
				if (UMaterialInterface* Found = Cast<UMaterialInterface>(Asset.GetAsset()))
				{
					return Found;
				}
			}
			if (!ClothParent && Asset.AssetName == FName(TEXT("000基础布纹")))
			{
				ClothParent = Cast<UMaterialInterface>(Asset.GetAsset());
			}
		}
		return ClothParent;
	}

	FString GetParentMaterialObjectPath()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("PBRTextureLab");
		FString Root = Plugin.IsValid() ? Plugin->GetMountedAssetPath() : FString(TEXT("/PBRTextureLab/"));
		Root.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (!Root.EndsWith(TEXT("/")))
		{
			Root += TEXT("/");
		}
		const FString AssetName = GetParentAssetName();
		return Root + TEXT("Materials/") + AssetName + TEXT(".") + AssetName;
	}

	UMaterial* GetOrCreateParentMaterial(FString* OutError)
	{
		if (!MaterialIsGameThread(OutError))
		{
			return nullptr;
		}

		const FString ObjectPath = GetParentMaterialObjectPath();
		const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("PBRTextureLab");
		FString MountedRoot = Plugin.IsValid() ? Plugin->GetMountedAssetPath() : FString(TEXT("/PBRTextureLab/"));
		MountedRoot.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (!MountedRoot.EndsWith(TEXT("/")))
		{
			MountedRoot += TEXT("/");
		}
		DiscardLegacyUnversionedParent(MountedRoot, PackageName);

		if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			if (ParentHasExpectedParameters(Existing))
			{
				return Existing;
			}
			UE_LOG(LogPBRTextureLab, Log, TEXT("Rebuilding Metallic/Roughness parent material (not OpenPBR): %s"), *ObjectPath);
			UMaterialEditingLibrary::DeleteAllMaterialExpressions(Existing);
			if (!BuildParentGraph(Existing, OutError))
			{
				return nullptr;
			}
			Existing->PreEditChange(nullptr);
			Existing->PostEditChange();
			const TArray<FString> Errors = RecompileParent(Existing);
			if (Errors.Num() > 0)
			{
				MaterialSetError(OutError, FString::Join(Errors, TEXT("; ")));
				return nullptr;
			}
			Existing->MarkPackageDirty();
			if (!SaveAssetPackage(Existing, OutError))
			{
				return nullptr;
			}
			return Existing;
		}

		DiscardUnloadablePackage(PackageName);

		if (Plugin.IsValid())
		{
			IFileManager::Get().MakeDirectory(*(Plugin->GetContentDir() / TEXT("Materials")), true);
		}

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			MaterialSetError(OutError, FString::Printf(TEXT("Failed to create package %s"), *PackageName));
			return nullptr;
		}
		if (!Package->IsFullyLoaded())
		{
			Package->MarkAsFullyLoaded();
		}

		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		UMaterial* Material = Cast<UMaterial>(Factory->FactoryCreateNew(
			UMaterial::StaticClass(),
			Package,
			FName(*AssetName),
			RF_Public | RF_Standalone | RF_Transactional,
			nullptr,
			GWarn));
		if (!Material)
		{
			MaterialSetError(OutError, TEXT("UMaterialFactoryNew failed to create the parent material."));
			return nullptr;
		}

		FAssetRegistryModule::AssetCreated(Material);
		UE_LOG(LogPBRTextureLab, Log, TEXT("Created Metallic/Roughness parent material (not OpenPBR): %s"), *ObjectPath);

		UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);
		if (!BuildParentGraph(Material, OutError))
		{
			return nullptr;
		}

		Material->PreEditChange(nullptr);
		Material->PostEditChange();
		const TArray<FString> Errors = RecompileParent(Material);
		if (Errors.Num() > 0)
		{
			MaterialSetError(OutError, FString::Join(Errors, TEXT("; ")));
			return nullptr;
		}

		Material->MarkPackageDirty();
		if (!SaveAssetPackage(Material, OutError))
		{
			return nullptr;
		}
		return Material;
	}

	EPBRImportStatus CreateMaterialInstance(
		const FPBRImportedTextures& Textures,
		const FPBRMaterialInstanceRequest& Request,
		UMaterialInstanceConstant*& OutInstance,
		FString* OutError)
	{
		OutInstance = nullptr;

		if (!MaterialIsGameThread(OutError))
		{
			return EPBRImportStatus::Failed;
		}

		if (Request.bCancelled)
		{
			if (OutError)
			{
				*OutError = TEXT("Material instance creation cancelled.");
			}
			return EPBRImportStatus::Cancelled;
		}

		if (!Textures.HasAny())
		{
			UE_LOG(LogPBRTextureLab, Log, TEXT("CreateMaterialInstance using parent defaults; no generated textures bound."));
		}

		if (Request.bModifyExisting)
		{
			UMaterialInstanceConstant* Existing = Request.ExistingInstance;
			if (!IsValid(Existing))
			{
				MaterialSetError(OutError, TEXT("Modify requires an existing material instance."));
				return EPBRImportStatus::Failed;
			}

			UMaterialInterface* Parent = Request.ParentMaterial;
			if (!Parent)
			{
				Parent = Existing->Parent;
			}
			if (!Parent)
			{
				Parent = FindPreferredUserParent();
			}
			if (!Parent)
			{
				Parent = GetOrCreateParentMaterial(OutError);
			}
			if (!Parent)
			{
				return EPBRImportStatus::Failed;
			}

			{
				FScopedTransaction Transaction(NSLOCTEXT("PBRTextureLab", "ModifyMaterialInstance", "Modify PBR Texture Lab Material Instance"));
				Existing->Modify();
				Existing->SetParentEditorOnly(Parent);
				BindGeneratedTextures(Existing, Textures, Request);
				Existing->PostEditChange();
				Existing->MarkPackageDirty();
			}

			if (Request.bSave && !SaveAssetPackage(Existing, OutError))
			{
				return EPBRImportStatus::Failed;
			}

			OutInstance = Existing;
			UE_LOG(LogPBRTextureLab, Log, TEXT("Modified Metallic/Roughness material instance: %s"), *Existing->GetPathName());
			return EPBRImportStatus::Success;
		}

		UMaterialInterface* Parent = Request.ParentMaterial;
		if (!Parent)
		{
			Parent = FindPreferredUserParent();
		}
		if (!Parent)
		{
			Parent = GetOrCreateParentMaterial(OutError);
		}
		if (!Parent)
		{
			return EPBRImportStatus::Failed;
		}

		const FString DestinationPath = MaterialNormalizeContentPath(Request.DestinationPath);
		if (!DestinationPath.StartsWith(TEXT("/Game")))
		{
			MaterialSetError(OutError, FString::Printf(TEXT("Destination path must be under /Game: %s"), *DestinationPath));
			return EPBRImportStatus::Failed;
		}

		const FString RequestedInstanceName = Request.InstanceName.IsEmpty()
			? Request.BaseName + TEXT("_Inst")
			: Request.InstanceName;
		FString AssetName = ObjectTools::SanitizeObjectName(RequestedInstanceName);
		if (AssetName.IsEmpty())
		{
			MaterialSetError(OutError, TEXT("Material instance name is empty after sanitizing."));
			return EPBRImportStatus::Failed;
		}

		FString PackageName = DestinationPath / AssetName;
		if (!FPackageName::IsValidLongPackageName(PackageName))
		{
			MaterialSetError(OutError, FString::Printf(TEXT("Invalid package name: %s"), *PackageName));
			return EPBRImportStatus::Failed;
		}

		bool bReplaceExisting = false;
		if (MaterialAssetExists(PackageName, AssetName))
		{
			switch (Request.ConflictPolicy)
			{
			case EPBRImportConflictPolicy::Cancel:
				if (OutError)
				{
					*OutError = FString::Printf(TEXT("Name conflict: %s"), *PackageName);
				}
				UE_LOG(LogPBRTextureLab, Warning, TEXT("CreateMaterialInstance name conflict (cancel): %s"), *PackageName);
				return EPBRImportStatus::NameConflict;
			case EPBRImportConflictPolicy::Replace:
				bReplaceExisting = true;
				break;
			case EPBRImportConflictPolicy::UniqueName:
				{
					FString UniquePackage;
					FString UniqueName;
					GetAssetTools().CreateUniqueAssetName(PackageName, TEXT(""), UniquePackage, UniqueName);
					PackageName = UniquePackage;
					AssetName = UniqueName;
				}
				break;
			}
		}

		UMaterialInstanceConstant* Instance = nullptr;
		{
			FScopedTransaction Transaction(NSLOCTEXT("PBRTextureLab", "CreateMaterialInstance", "Create PBR Texture Lab Material Instance"));

			if (bReplaceExisting)
			{
				// Do not LoadObject a leftover instance that still hard-refs a deleted
				// cross-version parent; unload + recreate instead.
				if (UPackage* ExistingPackage = FindPackage(nullptr, *PackageName))
				{
					ResetLoaders(ExistingPackage);
					TArray<UPackage*> ToUnload;
					ToUnload.Add(ExistingPackage);
					FText UnloadError;
					UPackageTools::UnloadPackages(ToUnload, UnloadError, true);
				}
				FString ExistingFilename;
				if (MaterialGetExistingPackageFilename(PackageName, ExistingFilename))
				{
					DeletePackageFiles(ExistingFilename);
				}
			}

			if (!Instance)
			{
				UPackage* Package = CreatePackage(*PackageName);
				UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
				Factory->InitialParent = Parent;
				Instance = Cast<UMaterialInstanceConstant>(Factory->FactoryCreateNew(
					UMaterialInstanceConstant::StaticClass(),
					Package,
					FName(*AssetName),
					RF_Public | RF_Standalone | RF_Transactional,
					nullptr,
					GWarn));
				if (Instance)
				{
					FAssetRegistryModule::AssetCreated(Instance);
				}
			}

			if (!Instance)
			{
				MaterialSetError(OutError, TEXT("Failed to create UMaterialInstanceConstant."));
				return EPBRImportStatus::Failed;
			}

			Instance->SetParentEditorOnly(Parent);
			BindGeneratedTextures(Instance, Textures, Request);
			Instance->PostEditChange();
			Instance->MarkPackageDirty();
		}

		if (Request.bSave && !SaveAssetPackage(Instance, OutError))
		{
			TArray<UObject*> Created;
			Created.Add(Instance);
			ObjectTools::ForceDeleteObjects(Created, false);
			return EPBRImportStatus::Failed;
		}

		OutInstance = Instance;
		UE_LOG(LogPBRTextureLab, Log, TEXT("Created Metallic/Roughness material instance (not OpenPBR): %s"), *Instance->GetPathName());
		return EPBRImportStatus::Success;
	}
}
