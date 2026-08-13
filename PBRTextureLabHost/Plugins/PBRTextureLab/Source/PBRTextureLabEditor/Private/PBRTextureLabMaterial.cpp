#include "PBRTextureLabMaterial.h"
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
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionBumpOffset.h"
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

		void SetError(FString* OutError, const FString& Message)
		{
			UE_LOG(LogPBRTextureLab, Error, TEXT("%s"), *Message);
			if (OutError)
			{
				*OutError = Message;
			}
		}

		bool IsGameThread(FString* OutError)
		{
			if (IsInGameThread())
			{
				return true;
			}
			SetError(OutError, TEXT("PBR material APIs must run on the Game Thread."));
			return false;
		}

		FString NormalizeContentPath(const FString& InPath)
		{
			FString Path = InPath;
			Path.ReplaceInline(TEXT("\\"), TEXT("/"));
			while (Path.Len() > 1 && Path.EndsWith(TEXT("/")))
			{
				Path.LeftChopInline(1);
			}
			return Path;
		}

		bool GetExistingPackageFilename(const FString& PackageName, FString& OutFilename)
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

		bool AssetExists(const FString& PackageName, const FString& AssetName)
		{
			FString UnusedFilename;
			if (GetExistingPackageFilename(PackageName, UnusedFilename))
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
			const bool bHasFile = GetExistingPackageFilename(PackageName, Filename);
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
			if (GetExistingPackageFilename(LegacyPackageName, LegacyFilename))
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
				SetError(OutError, FString::Printf(
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
				SetError(OutError, FString::Printf(TEXT("Failed to connect material property %d"), static_cast<int32>(Property)));
				return false;
			}
			return true;
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
				bHasBaseColor |= ParameterName == FName(PBRTEXTURELAB_PARAM_BaseColorTexture);
				bHasNormal |= ParameterName == FName(PBRTEXTURELAB_PARAM_NormalTexture);
				bHasOrm |= ParameterName == FName(PBRTEXTURELAB_PARAM_ORMTexture);
				bHasHeight |= ParameterName == FName(PBRTEXTURELAB_PARAM_HeightTexture);
				bHasNormalStrength |= ParameterName == FName(PBRTEXTURELAB_PARAM_NormalStrength);
				bHasHeightAmount |= ParameterName == FName(PBRTEXTURELAB_PARAM_HeightAmount);
				bHasUvScale |= ParameterName == FName(PBRTEXTURELAB_PARAM_UVScale);
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
				SetError(OutError, TEXT("Failed to create parent material expressions."));
				return false;
			}

			TexCoord->CoordinateIndex = 0;

			UvScale->ParameterName = PBRTEXTURELAB_PARAM_UVScale;
			UvScale->DefaultValue = 1.0f;
			UvScale->Group = TEXT("PBRTextureLab");

			HeightAmount->ParameterName = PBRTEXTURELAB_PARAM_HeightAmount;
			HeightAmount->DefaultValue = 0.0f;
			HeightAmount->Group = TEXT("PBRTextureLab");

			NormalStrength->ParameterName = PBRTEXTURELAB_PARAM_NormalStrength;
			NormalStrength->DefaultValue = 1.0f;
			NormalStrength->Group = TEXT("PBRTextureLab");

			HeightSample->ParameterName = PBRTEXTURELAB_PARAM_HeightTexture;
			HeightSample->SamplerType = SAMPLERTYPE_LinearGrayscale;
			HeightSample->Group = TEXT("PBRTextureLab");
			HeightSample->SetDefaultTexture();

			BaseColor->ParameterName = PBRTEXTURELAB_PARAM_BaseColorTexture;
			BaseColor->SamplerType = SAMPLERTYPE_Color;
			BaseColor->Group = TEXT("PBRTextureLab");
			BaseColor->SetDefaultTexture();

			NormalSample->ParameterName = PBRTEXTURELAB_PARAM_NormalTexture;
			NormalSample->SamplerType = SAMPLERTYPE_Normal;
			NormalSample->Group = TEXT("PBRTextureLab");
			NormalSample->SetDefaultTexture();

			OrmSample->ParameterName = PBRTEXTURELAB_PARAM_ORMTexture;
			OrmSample->SamplerType = SAMPLERTYPE_Masks;
			OrmSample->Group = TEXT("PBRTextureLab");
			OrmSample->SetDefaultTexture();

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

		bool SaveAssetPackage(UObject* Asset, FString* OutError)
		{
			if (!Asset)
			{
				SetError(OutError, TEXT("Cannot save a null material asset."));
				return false;
			}
			TArray<UPackage*> Packages;
			Packages.Add(Asset->GetOutermost());
			if (!UEditorLoadingAndSavingUtils::SavePackages(Packages, false))
			{
				SetError(OutError, FString::Printf(TEXT("Failed to save %s"), *Asset->GetPathName()));
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
		if (!IsGameThread(OutError))
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
				SetError(OutError, FString::Join(Errors, TEXT("; ")));
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
			SetError(OutError, FString::Printf(TEXT("Failed to create package %s"), *PackageName));
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
			SetError(OutError, TEXT("UMaterialFactoryNew failed to create the parent material."));
			return nullptr;
		}

		FAssetRegistryModule::AssetCreated(Material);
		UE_LOG(LogPBRTextureLab, Log, TEXT("Created Metallic/Roughness parent material (not OpenPBR): %s"), *ObjectPath);

		if (!BuildParentGraph(Material, OutError))
		{
			return nullptr;
		}

		Material->PreEditChange(nullptr);
		Material->PostEditChange();
		const TArray<FString> Errors = RecompileParent(Material);
		if (Errors.Num() > 0)
		{
			SetError(OutError, FString::Join(Errors, TEXT("; ")));
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

		if (!IsGameThread(OutError))
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

		if (!Textures.HasAll())
		{
			SetError(OutError, TEXT("CreateMaterialInstance requires all Task 3 textures."));
			return EPBRImportStatus::Failed;
		}

		UMaterial* Parent = GetOrCreateParentMaterial(OutError);
		if (!Parent)
		{
			return EPBRImportStatus::Failed;
		}

		const FString DestinationPath = NormalizeContentPath(Request.DestinationPath);
		if (!DestinationPath.StartsWith(TEXT("/Game")))
		{
			SetError(OutError, FString::Printf(TEXT("Destination path must be under /Game: %s"), *DestinationPath));
			return EPBRImportStatus::Failed;
		}

		FString AssetName = ObjectTools::SanitizeObjectName(Request.BaseName + TEXT("_Inst"));
		if (AssetName.IsEmpty())
		{
			SetError(OutError, TEXT("BaseName is empty after sanitizing."));
			return EPBRImportStatus::Failed;
		}

		FString PackageName = DestinationPath / AssetName;
		if (!FPackageName::IsValidLongPackageName(PackageName))
		{
			SetError(OutError, FString::Printf(TEXT("Invalid package name: %s"), *PackageName));
			return EPBRImportStatus::Failed;
		}

		bool bReplaceExisting = false;
		if (AssetExists(PackageName, AssetName))
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
				if (GetExistingPackageFilename(PackageName, ExistingFilename))
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
				SetError(OutError, TEXT("Failed to create UMaterialInstanceConstant."));
				return EPBRImportStatus::Failed;
			}

			Instance->SetParentEditorOnly(Parent);
			SetMaterialInstanceTexture(Instance, PBRTEXTURELAB_PARAM_BaseColorTexture, Textures.BaseColor);
			SetMaterialInstanceTexture(Instance, PBRTEXTURELAB_PARAM_NormalTexture, Textures.Normal);
			SetMaterialInstanceTexture(Instance, PBRTEXTURELAB_PARAM_ORMTexture, Textures.ORM);
			SetMaterialInstanceTexture(Instance, PBRTEXTURELAB_PARAM_HeightTexture, Textures.Height);
			SetMaterialInstanceScalar(Instance, PBRTEXTURELAB_PARAM_NormalStrength, Request.NormalStrength);
			SetMaterialInstanceScalar(Instance, PBRTEXTURELAB_PARAM_HeightAmount, Request.HeightAmount);
			SetMaterialInstanceScalar(Instance, PBRTEXTURELAB_PARAM_UVScale, Request.UVScale);
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
