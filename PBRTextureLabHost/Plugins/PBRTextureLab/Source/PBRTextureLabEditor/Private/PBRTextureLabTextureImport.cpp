#include "PBRTextureLabTextureImport.h"
#include "PBRTextureLabEditorApi.h"

#include "AssetImportTask.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "IImageWrapperModule.h"
#include "ImageCore.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

namespace PBRTextureLab
{
	namespace
	{
		struct FChannelJob
		{
			const TCHAR* Suffix = nullptr;
			const FPBRImageRgba8* Image = nullptr;
			UTexture2D** OutTexture = nullptr;
			TextureCompressionSettings Compression = TC_Default;
			bool bSRGB = false;
			TextureGroup Group = TEXTUREGROUP_World;
			ETextureSourceColorSpace ColorSpace = ETextureSourceColorSpace::Linear;
		};

		class FStagingSession
		{
		public:
			explicit FStagingSession(const FString& InDirectory)
				: Directory(InDirectory)
			{
			}

			~FStagingSession()
			{
				Cleanup();
			}

			const FString& GetDirectory() const
			{
				return Directory;
			}

			void Cleanup()
			{
				if (!Directory.IsEmpty() && IFileManager::Get().DirectoryExists(*Directory))
				{
					IFileManager::Get().DeleteDirectory(*Directory, false, true);
				}
			}

		private:
			FString Directory;
		};

		bool IsGameThread(FString* OutError)
		{
			if (IsInGameThread())
			{
				return true;
			}
			const FString Message = TEXT("ImportPBRMaps must run on the Game Thread.");
			UE_LOG(LogPBRTextureLab, Error, TEXT("%s"), *Message);
			if (OutError)
			{
				*OutError = Message;
			}
			return false;
		}

		void SetError(FString* OutError, const FString& Message)
		{
			UE_LOG(LogPBRTextureLab, Error, TEXT("%s"), *Message);
			if (OutError)
			{
				*OutError = Message;
			}
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
			const FString ObjectPath = PackageName + TEXT(".") + AssetName;
			return FindObject<UObject>(nullptr, *ObjectPath) != nullptr;
		}

		bool PrepareExistingPackageForReplace(const FString& PackageName, FString* OutError)
		{
			FString Filename;
			if (!GetExistingPackageFilename(PackageName, Filename))
			{
				return true;
			}

			UPackage* Package = FindPackage(nullptr, *PackageName);
			if (!Package)
			{
				Package = LoadPackage(nullptr, *PackageName, LOAD_NoWarn | LOAD_Quiet);
			}

			if (Package && Package->IsFullyLoaded())
			{
				return true;
			}

			if (Package)
			{
				ResetLoaders(Package);
			}

			const FString CompanionFiles[] = {
				Filename,
				FPaths::ChangeExtension(Filename, TEXT("uexp")),
				FPaths::ChangeExtension(Filename, TEXT("ubulk")),
				FPaths::ChangeExtension(Filename, TEXT("uptnl"))
			};
			for (const FString& File : CompanionFiles)
			{
				if (IFileManager::Get().FileExists(*File)
					&& !IFileManager::Get().Delete(*File, false, true, true))
				{
					SetError(OutError, FString::Printf(TEXT("Cannot replace unloadable package file: %s"), *File));
					return false;
				}
			}

			UE_LOG(LogPBRTextureLab, Warning,
				TEXT("Replaced unloadable existing package by deleting %s"), *Filename);
			return true;
		}

		bool EncodePng(const FPBRImageRgba8& Image, const bool bSRGB, const FString& Filename, FString* OutError)
		{
			FImage Encoded(Image.Width, Image.Height, ERawImageFormat::BGRA8, bSRGB ? EGammaSpace::sRGB : EGammaSpace::Linear);
			TArrayView64<FColor> Pixels = Encoded.AsBGRA8();
			for (int32 Index = 0; Index < Image.Pixels.Num(); ++Index)
			{
				Pixels[Index] = Image.Pixels[Index];
			}

			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
			TArray64<uint8> Compressed;
			if (!ImageWrapperModule.CompressImage(Compressed, EImageFormat::PNG, Encoded, static_cast<int32>(EImageCompressionQuality::Uncompressed)))
			{
				SetError(OutError, FString::Printf(TEXT("ImageWrapper failed to encode PNG: %s"), *Filename));
				return false;
			}
			if (!FFileHelper::SaveArrayToFile(Compressed, *Filename))
			{
				SetError(OutError, FString::Printf(TEXT("Failed to write staging file: %s"), *Filename));
				return false;
			}
			return true;
		}

		void ApplyImportSettings(UTexture2D* Texture, const FChannelJob& Job)
		{
			if (!Texture)
			{
				return;
			}
			Texture->CompressionSettings = Job.Compression;
			Texture->SRGB = Job.bSRGB;
			Texture->LODGroup = Job.Group;
			Texture->PostEditChange();
			Texture->MarkPackageDirty();
		}

		void CollectCreatedTextures(const FPBRImportedTextures& Textures, TArray<UObject*>& OutObjects)
		{
			if (Textures.BaseColor) { OutObjects.Add(Textures.BaseColor); }
			if (Textures.Height) { OutObjects.Add(Textures.Height); }
			if (Textures.Normal) { OutObjects.Add(Textures.Normal); }
			if (Textures.AO) { OutObjects.Add(Textures.AO); }
			if (Textures.Roughness) { OutObjects.Add(Textures.Roughness); }
			if (Textures.Metallic) { OutObjects.Add(Textures.Metallic); }
			if (Textures.ORM) { OutObjects.Add(Textures.ORM); }
		}

		void RollbackCreated(const TArray<UTexture2D*>& NewlyCreated)
		{
			TArray<UObject*> ToDelete;
			for (UTexture2D* Texture : NewlyCreated)
			{
				if (Texture)
				{
					ToDelete.Add(Texture);
				}
			}
			if (ToDelete.Num() > 0)
			{
				ObjectTools::ForceDeleteObjects(ToDelete, false);
			}
		}

		UTexture2D* FindImportedTexture(UAssetImportTask* Task, const FString& ExpectedName)
		{
			if (!Task)
			{
				return nullptr;
			}
			for (UObject* Object : Task->GetObjects())
			{
				if (UTexture2D* Texture = Cast<UTexture2D>(Object))
				{
					if (Texture->GetName() == ExpectedName)
					{
						return Texture;
					}
				}
			}
			if (Task->GetObjects().Num() == 1)
			{
				return Cast<UTexture2D>(Task->GetObjects()[0]);
			}
			return nullptr;
		}
	}

	FString GetStagingRootDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PBRTextureLab"), TEXT("Staging")));
	}

	EPBRImportStatus ImportPBRMaps(
		const FPBRMaps& Maps,
		const FPBRTextureImportRequest& Request,
		FPBRImportedTextures& OutTextures,
		FString* OutError)
	{
		OutTextures = FPBRImportedTextures();

		if (!IsGameThread(OutError))
		{
			return EPBRImportStatus::Failed;
		}

		if (Request.bCancelled)
		{
			if (OutError)
			{
				*OutError = TEXT("Import cancelled.");
			}
			UE_LOG(LogPBRTextureLab, Log, TEXT("ImportPBRMaps cancelled before work started."));
			return EPBRImportStatus::Cancelled;
		}

		if (!IsValidImage(Maps.BaseColor) || !IsValidImage(Maps.Height) || !IsValidImage(Maps.Normal)
			|| !IsValidImage(Maps.AO) || !IsValidImage(Maps.Roughness) || !IsValidImage(Maps.Metallic)
			|| !IsValidImage(Maps.ORM))
		{
			SetError(OutError, TEXT("ImportPBRMaps rejected invalid pixel maps."));
			return EPBRImportStatus::Failed;
		}

		const FString DestinationPath = NormalizeContentPath(Request.DestinationPath);
		if (!DestinationPath.StartsWith(TEXT("/Game")))
		{
			SetError(OutError, FString::Printf(TEXT("Destination path must be under /Game: %s"), *DestinationPath));
			return EPBRImportStatus::Failed;
		}

		const FString SanitizedBase = ObjectTools::SanitizeObjectName(Request.BaseName);
		if (SanitizedBase.IsEmpty())
		{
			SetError(OutError, TEXT("BaseName is empty after sanitizing."));
			return EPBRImportStatus::Failed;
		}

		FChannelJob Jobs[] = {
			{ TEXT("_BaseColor"), &Maps.BaseColor, &OutTextures.BaseColor, TC_Default, true, TEXTUREGROUP_World, ETextureSourceColorSpace::SRGB },
			{ TEXT("_Height"), &Maps.Height, &OutTextures.Height, TC_Grayscale, false, TEXTUREGROUP_World, ETextureSourceColorSpace::Linear },
			{ TEXT("_Normal"), &Maps.Normal, &OutTextures.Normal, TC_Normalmap, false, TEXTUREGROUP_WorldNormalMap, ETextureSourceColorSpace::Linear },
			{ TEXT("_AO"), &Maps.AO, &OutTextures.AO, TC_Grayscale, false, TEXTUREGROUP_World, ETextureSourceColorSpace::Linear },
			{ TEXT("_Roughness"), &Maps.Roughness, &OutTextures.Roughness, TC_Grayscale, false, TEXTUREGROUP_World, ETextureSourceColorSpace::Linear },
			{ TEXT("_Metallic"), &Maps.Metallic, &OutTextures.Metallic, TC_Grayscale, false, TEXTUREGROUP_World, ETextureSourceColorSpace::Linear },
			{ TEXT("_ORM"), &Maps.ORM, &OutTextures.ORM, TC_Masks, false, TEXTUREGROUP_World, ETextureSourceColorSpace::Linear }
		};

		struct FResolvedName
		{
			FString AssetName;
			FString PackageName;
			bool bReplaceExisting = false;
		};
		FResolvedName Resolved[UE_ARRAY_COUNT(Jobs)];

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Jobs); ++Index)
		{
			FString AssetName = ObjectTools::SanitizeObjectName(SanitizedBase + Jobs[Index].Suffix);
			FString PackageName = DestinationPath / AssetName;
			if (!FPackageName::IsValidLongPackageName(PackageName))
			{
				SetError(OutError, FString::Printf(TEXT("Invalid package name: %s"), *PackageName));
				return EPBRImportStatus::Failed;
			}

			const bool bExists = AssetExists(PackageName, AssetName);
			if (bExists)
			{
				switch (Request.ConflictPolicy)
				{
				case EPBRImportConflictPolicy::Cancel:
					if (OutError)
					{
						*OutError = FString::Printf(TEXT("Name conflict: %s"), *PackageName);
					}
					UE_LOG(LogPBRTextureLab, Warning, TEXT("ImportPBRMaps name conflict (cancel): %s"), *PackageName);
					return EPBRImportStatus::NameConflict;
				case EPBRImportConflictPolicy::Replace:
					if (!PrepareExistingPackageForReplace(PackageName, OutError))
					{
						return EPBRImportStatus::Failed;
					}
					Resolved[Index].bReplaceExisting = AssetExists(PackageName, AssetName);
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

			Resolved[Index].AssetName = AssetName;
			Resolved[Index].PackageName = PackageName;
		}

		const FString SessionDir = FPaths::Combine(GetStagingRootDirectory(), FGuid::NewGuid().ToString(EGuidFormats::Digits));
		FStagingSession Staging(SessionDir);
		IFileManager::Get().MakeDirectory(*SessionDir, true);

		TArray<UAssetImportTask*> Tasks;
		TArray<UTexture2D*> NewlyCreated;
		Tasks.Reserve(UE_ARRAY_COUNT(Jobs));

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Jobs); ++Index)
		{
			const FString StagingFile = FPaths::Combine(SessionDir, Resolved[Index].AssetName + TEXT(".png"));
			if (!EncodePng(*Jobs[Index].Image, Jobs[Index].bSRGB, StagingFile, OutError))
			{
				return EPBRImportStatus::Failed;
			}

			UTextureFactory* Factory = NewObject<UTextureFactory>(GetTransientPackage());
			Factory->bCreateMaterial = 0;
			Factory->CompressionSettings = Jobs[Index].Compression;
			Factory->ColorSpaceMode = Jobs[Index].ColorSpace;
			Factory->LODGroup = Jobs[Index].Group;
			Factory->bFlipNormalMapGreenChannel = 0;
			UTextureFactory::SuppressImportOverwriteDialog(true);

			UAssetImportTask* Task = NewObject<UAssetImportTask>(GetTransientPackage());
			Task->Filename = StagingFile;
			Task->DestinationPath = DestinationPath;
			Task->DestinationName = Resolved[Index].AssetName;
			Task->bReplaceExisting = Resolved[Index].bReplaceExisting;
			Task->bReplaceExistingSettings = Resolved[Index].bReplaceExisting;
			Task->bAutomated = true;
			Task->bAsync = false;
			Task->bSave = false;
			Task->Factory = Factory;
			Tasks.Add(Task);
		}

		{
			FScopedTransaction Transaction(NSLOCTEXT("PBRTextureLab", "ImportPBRMaps", "Import PBR Texture Lab Maps"));
			ImportAssetTasks(Tasks);
		}

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Jobs); ++Index)
		{
			UTexture2D* Texture = FindImportedTexture(Tasks[Index], Resolved[Index].AssetName);
			if (!Texture)
			{
				RollbackCreated(NewlyCreated);
				OutTextures = FPBRImportedTextures();
				SetError(OutError, FString::Printf(TEXT("Import failed for %s"), *Resolved[Index].AssetName));
				return EPBRImportStatus::Failed;
			}
			*Jobs[Index].OutTexture = Texture;
			if (!Resolved[Index].bReplaceExisting)
			{
				NewlyCreated.Add(Texture);
			}
			ApplyImportSettings(Texture, Jobs[Index]);
		}

		if (Request.bSave)
		{
			TArray<UPackage*> Packages;
			TArray<UObject*> Created;
			CollectCreatedTextures(OutTextures, Created);
			for (UObject* Object : Created)
			{
				Packages.AddUnique(Object->GetOutermost());
			}
			if (!UEditorLoadingAndSavingUtils::SavePackages(Packages, false))
			{
				RollbackCreated(NewlyCreated);
				OutTextures = FPBRImportedTextures();
				SetError(OutError, TEXT("Failed to save imported texture packages."));
				return EPBRImportStatus::Failed;
			}
		}

		UE_LOG(LogPBRTextureLab, Log, TEXT("Imported PBR maps to %s/%s*"), *DestinationPath, *SanitizedBase);
		return EPBRImportStatus::Success;
	}
}
