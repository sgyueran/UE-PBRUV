#include "PBRTextureLabMaterial.h"

#include <initializer_list>
#include "PBRTextureLabCompat.h"
#include "PBRTextureLabEditorApi.h"
#include "PBRTextureLabPixelCore.h"

#include "AssetRegistry/AssetRegistryModule.h"
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
#include "Materials/MaterialExpressionUtils.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialShared.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "ScopedTransaction.h"
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
				&& MaterialExpressionUtils::GetSamplerTypeForTexture(Sample->Texture) == Expected;
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
				if (MaterialExpressionUtils::GetSamplerTypeForTexture(Existing) == SamplerType)
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
			if (Texture && MaterialExpressionUtils::GetSamplerTypeForTexture(Texture) == SamplerType)
			{
				return Texture;
			}

			if (SamplerType == SAMPLERTYPE_LinearGrayscale)
			{
				Texture = LoadEngineTexture(TEXT("/Engine/EngineMaterials/DefaultCalibrationGrayscale.DefaultCalibrationGrayscale"));
				if (Texture && MaterialExpressionUtils::GetSamplerTypeForTexture(Texture) == SamplerType)
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
			bool bHasOrm = false;
			bool bHasHeight = false;
			bool bHasNormalStrength = false;
			bool bHasHeightAmount = false;
			bool bHasUvScale = false;

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
					bHasOrm |= ParameterName == FName(PBRTEXTURELAB_PARAM_ORMTexture)
						&& SamplerMatchesTexture(Sample, SAMPLERTYPE_Masks);
					bHasHeight |= ParameterName == FName(PBRTEXTURELAB_PARAM_HeightTexture)
						&& SamplerMatchesTexture(Sample, SAMPLERTYPE_LinearGrayscale);
				}
				else
				{
					bHasNormalStrength |= ParameterName == FName(PBRTEXTURELAB_PARAM_NormalStrength);
					bHasHeightAmount |= ParameterName == FName(PBRTEXTURELAB_PARAM_HeightAmount);
					bHasUvScale |= ParameterName == FName(PBRTEXTURELAB_PARAM_UVScale);
				}
			}

			return bHasBaseColor && bHasNormal && bHasOrm && bHasHeight
				&& bHasNormalStrength && bHasHeightAmount && bHasUvScale
				&& FMath::IsNearlyEqual(
					UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Material, PBRTEXTURELAB_PARAM_UVScale),
					1.0f);
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
			UMaterialExpressionTextureSampleParameter2D* OrmSample = AddExpression<UMaterialExpressionTextureSampleParameter2D>(Material, -100, 360);
			UMaterialExpressionScalarParameter* NormalStrength = AddExpression<UMaterialExpressionScalarParameter>(Material, -100, 600);
			UMaterialExpressionConstant3Vector* FlatNormal = AddExpression<UMaterialExpressionConstant3Vector>(Material, 160, -40);
			UMaterialExpressionLinearInterpolate* NormalBlend = AddExpression<UMaterialExpressionLinearInterpolate>(Material, 360, 80);
			UMaterialExpressionNormalize* NormalOut = AddExpression<UMaterialExpressionNormalize>(Material, 600, 80);

			if (!TexCoord || !UvScale || !ScaledUv || !HeightSample || !HeightAmount || !Bump
				|| !BaseColor || !NormalSample || !OrmSample || !NormalStrength
				|| !FlatNormal || !NormalBlend || !NormalOut)
			{
				MaterialSetError(OutError, TEXT("Failed to create parent material expressions."));
				return false;
			}

			TexCoord->CoordinateIndex = 0;

			UvScale->ParameterName = PBRTEXTURELAB_PARAM_UVScale;
			UvScale->DefaultValue = 1.0f;
			UvScale->Group = PBRTEXTURELAB_PARAM_GROUP;
			UvScale->SortPriority = 6;

			HeightAmount->ParameterName = PBRTEXTURELAB_PARAM_HeightAmount;
			HeightAmount->DefaultValue = 0.0f;
			HeightAmount->Group = PBRTEXTURELAB_PARAM_GROUP;
			HeightAmount->SortPriority = 5;

			NormalStrength->ParameterName = PBRTEXTURELAB_PARAM_NormalStrength;
			NormalStrength->DefaultValue = 1.0f;
			NormalStrength->Group = PBRTEXTURELAB_PARAM_GROUP;
			NormalStrength->SortPriority = 4;

			HeightSample->ParameterName = PBRTEXTURELAB_PARAM_HeightTexture;
			HeightSample->Group = PBRTEXTURELAB_PARAM_GROUP;
			HeightSample->SortPriority = 3;
			AssignSamplerDefault(HeightSample, SAMPLERTYPE_LinearGrayscale);

			BaseColor->ParameterName = PBRTEXTURELAB_PARAM_BaseColorTexture;
			BaseColor->Group = PBRTEXTURELAB_PARAM_GROUP;
			BaseColor->SortPriority = 0;
			AssignSamplerDefault(BaseColor, SAMPLERTYPE_Color);

			NormalSample->ParameterName = PBRTEXTURELAB_PARAM_NormalTexture;
			NormalSample->Group = PBRTEXTURELAB_PARAM_GROUP;
			NormalSample->SortPriority = 1;
			AssignSamplerDefault(NormalSample, SAMPLERTYPE_Normal);

			OrmSample->ParameterName = PBRTEXTURELAB_PARAM_ORMTexture;
			OrmSample->Group = PBRTEXTURELAB_PARAM_GROUP;
			OrmSample->SortPriority = 2;
			AssignSamplerDefault(OrmSample, SAMPLERTYPE_Masks);

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
				|| !Connect(Bump, TEXT(""), OrmSample, TEXT("UVs"), OutError)
				|| !Connect(FlatNormal, TEXT(""), NormalBlend, TEXT("A"), OutError)
				|| !Connect(NormalSample, TEXT("RGB"), NormalBlend, TEXT("B"), OutError)
				|| !Connect(NormalStrength, TEXT(""), NormalBlend, TEXT("Alpha"), OutError)
				|| !Connect(NormalBlend, TEXT(""), NormalOut, TEXT(""), OutError)
				|| !ConnectProperty(BaseColor, TEXT("RGB"), MP_BaseColor, OutError)
				|| !ConnectProperty(NormalOut, TEXT(""), MP_Normal, OutError)
				|| !ConnectProperty(OrmSample, TEXT("R"), MP_AmbientOcclusion, OutError)
				|| !ConnectProperty(OrmSample, TEXT("G"), MP_Roughness, OutError)
				|| !ConnectProperty(OrmSample, TEXT("B"), MP_Metallic, OutError))
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

		void BindTextureIfPresent(
			UMaterialInstanceConstant* Instance,
			const TArray<FName>& TextureParams,
			const TArray<FName>& ScalarParams,
			UTexture* Texture,
			std::initializer_list<const TCHAR*> TextureAliases,
			std::initializer_list<const TCHAR*> EnableSwitchAliases)
		{
			if (!Instance || !Texture)
			{
				return;
			}
			const FName TextureName = FindExistingParameter(TextureParams, TextureAliases);
			if (TextureName.IsNone())
			{
				return;
			}
			SetMaterialInstanceTexture(Instance, TextureName, Texture);
			const FName SwitchName = FindExistingParameter(ScalarParams, EnableSwitchAliases);
			if (!SwitchName.IsNone())
			{
				SetMaterialInstanceScalar(Instance, SwitchName, 1.0f);
			}
		}

		void BindGeneratedTextures(UMaterialInstanceConstant* Instance, const FPBRImportedTextures& Textures, const FPBRMaterialInstanceRequest& Request)
		{
			if (!Instance)
			{
				return;
			}

			TArray<FName> TextureParams;
			TArray<FName> ScalarParams;
			UMaterialEditingLibrary::GetTextureParameterNames(Instance, TextureParams);
			UMaterialEditingLibrary::GetScalarParameterNames(Instance, ScalarParams);

			BindTextureIfPresent(Instance, TextureParams, ScalarParams, Textures.BaseColor,
				{ PBRTEXTURELAB_PARAM_BaseColorTexture, TEXT("基础贴图"), TEXT("BaseColorTexture"), TEXT("BaseColor"), TEXT("Diffuse"), TEXT("Albedo") },
				{ TEXT("基础贴图开关") });
			BindTextureIfPresent(Instance, TextureParams, ScalarParams, Textures.Normal,
				{ PBRTEXTURELAB_PARAM_NormalTexture, TEXT("法线贴图"), TEXT("NormalTexture"), TEXT("Normal") },
				{ TEXT("法线贴图开关") });
			BindTextureIfPresent(Instance, TextureParams, ScalarParams, Textures.Height,
				{ PBRTEXTURELAB_PARAM_HeightTexture, TEXT("置换贴图"), TEXT("HeightTexture"), TEXT("Height"), TEXT("Displacement") },
				{ TEXT("置换贴图开关") });
			BindTextureIfPresent(Instance, TextureParams, ScalarParams, Textures.Roughness,
				{ TEXT("粗糙贴图"), TEXT("粗糙度"), TEXT("Roughness") },
				{ TEXT("粗糙贴图开关") });
			BindTextureIfPresent(Instance, TextureParams, ScalarParams, Textures.Metallic,
				{ TEXT("金属贴图"), TEXT("金属度"), TEXT("Metallic") },
				{ TEXT("金属贴图开关") });
			BindTextureIfPresent(Instance, TextureParams, ScalarParams, Textures.AO,
				{ TEXT("AO"), TEXT("AmbientOcclusion"), TEXT("环境光遮蔽") },
				{});
			BindTextureIfPresent(Instance, TextureParams, ScalarParams, Textures.ORM,
				{ PBRTEXTURELAB_PARAM_ORMTexture, TEXT("ORMTexture"), TEXT("ORM贴图") },
				{});

			const FName NormalStrengthName = FindExistingParameter(ScalarParams,
				{ PBRTEXTURELAB_PARAM_NormalStrength, TEXT("法线强度"), TEXT("NormalStrength") });
			if (!NormalStrengthName.IsNone())
			{
				SetMaterialInstanceScalar(Instance, NormalStrengthName, Request.NormalStrength);
			}
			const FName HeightAmountName = FindExistingParameter(ScalarParams,
				{ PBRTEXTURELAB_PARAM_HeightAmount, TEXT("置换强度"), TEXT("高度强度"), TEXT("HeightAmount") });
			if (!HeightAmountName.IsNone())
			{
				SetMaterialInstanceScalar(Instance, HeightAmountName, Request.HeightAmount);
			}
			const FName UvScaleName = FindExistingParameter(ScalarParams,
				{ PBRTEXTURELAB_PARAM_UVScale, TEXT("UV缩放"), TEXT("UVScale") });
			if (!UvScaleName.IsNone())
			{
				SetMaterialInstanceScalar(Instance, UvScaleName, Request.UVScale);
			}
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

		UMaterialInterface* Parent = Request.ParentMaterial;
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
