#include "PBRTextureLabTextureImport.h"
#include "PBRTextureLabEditorApi.h"

#include "AssetImportTask.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "IImageWrapperModule.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "Templates/UniquePtr.h"
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
			bool bEnabled = true;
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

		bool ImportIsGameThread(FString* OutError)
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

		void ImportSetError(FString* OutError, const FString& Message)
		{
			UE_LOG(LogPBRTextureLab, Error, TEXT("%s"), *Message);
			if (OutError)
			{
				*OutError = Message;
			}
		}

		FString ImportNormalizeContentPath(const FString& InPath)
		{
			FString Path = InPath;
			Path.ReplaceInline(TEXT("\\"), TEXT("/"));
			while (Path.Len() > 1 && Path.EndsWith(TEXT("/")))
			{
				Path.LeftChopInline(1);
			}
			return Path;
		}

		bool ImportGetExistingPackageFilename(const FString& PackageName, FString& OutFilename)
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

		bool ImportAssetExists(const FString& PackageName, const FString& AssetName)
		{
			FString UnusedFilename;
			if (ImportGetExistingPackageFilename(PackageName, UnusedFilename))
			{
				return true;
			}
			const FString ObjectPath = PackageName + TEXT(".") + AssetName;
			return FindObject<UObject>(nullptr, *ObjectPath) != nullptr;
		}

		bool PrepareExistingPackageForReplace(const FString& PackageName, FString* OutError)
		{
			FString Filename;
			if (!ImportGetExistingPackageFilename(PackageName, Filename))
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
					ImportSetError(OutError, FString::Printf(TEXT("Cannot replace unloadable package file: %s"), *File));
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
				ImportSetError(OutError, FString::Printf(TEXT("ImageWrapper failed to encode PNG: %s"), *Filename));
				return false;
			}
			if (!FFileHelper::SaveArrayToFile(Compressed, *Filename))
			{
				ImportSetError(OutError, FString::Printf(TEXT("Failed to write staging file: %s"), *Filename));
				return false;
			}
			return true;
		}

		void ApplySeamlessAddressing(UTexture2D* Texture)
		{
			if (!Texture)
			{
				return;
			}
			Texture->AddressX = TA_Wrap;
			Texture->AddressY = TA_Wrap;
		}

		bool CopyImageToRgba8(const FImage& Image, FPBRImageRgba8& OutImage, FString* OutError)
		{
			FImage Bgra;
			Image.CopyTo(Bgra, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
			if (Bgra.Format != ERawImageFormat::BGRA8 || Bgra.SizeX <= 0 || Bgra.SizeY <= 0)
			{
				ImportSetError(OutError, TEXT("Converted image is not a valid BGRA8 buffer."));
				return false;
			}
			const TArrayView64<const FColor> Colors = Bgra.AsBGRA8();
			const int32 PixelCount = Bgra.SizeX * Bgra.SizeY;
			if (Colors.Num() < PixelCount)
			{
				ImportSetError(OutError, TEXT("Converted BGRA buffer is smaller than Width*Height."));
				return false;
			}
			OutImage.Width = Bgra.SizeX;
			OutImage.Height = Bgra.SizeY;
			OutImage.Pixels.SetNumUninitialized(PixelCount);
			FMemory::Memcpy(OutImage.Pixels.GetData(), Colors.GetData(), sizeof(FColor) * PixelCount);
			return true;
		}

		bool MakeLocalFileSeamless(
			const FString& Filename,
			const EPBRMapKind Kind,
			const FString& StagingFile,
			FString* OutError)
		{
			TArray64<uint8> FileData;
			if (!FFileHelper::LoadFileToArray(FileData, *Filename))
			{
				ImportSetError(OutError, FString::Printf(TEXT("Failed to read image file: %s"), *Filename));
				return false;
			}
			FImage Decoded;
			if (!FImageUtils::DecompressImage(FileData.GetData(), FileData.Num(), Decoded))
			{
				ImportSetError(OutError, FString::Printf(TEXT("Failed to decode image file: %s"), *Filename));
				return false;
			}
			FPBRImageRgba8 Image;
			if (!CopyImageToRgba8(Decoded, Image, OutError))
			{
				return false;
			}
			if (!MakeSeamlessImage(Image, Kind == EPBRMapKind::Normal))
			{
				ImportSetError(OutError, TEXT("Failed to convert imported image to a seamless tile."));
				return false;
			}
			const bool bSRGB = Kind == EPBRMapKind::Unknown || Kind == EPBRMapKind::BaseColor;
			return EncodePng(Image, bSRGB, StagingFile, OutError);
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
			ApplySeamlessAddressing(Texture);
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

	namespace
	{
		bool HasDelimitedToken(const FString& Stem, const TCHAR* Token)
		{
			const FString Needle = FString(Token).ToLower();
			if (Needle.IsEmpty())
			{
				return false;
			}
			if (Stem.Equals(Needle))
			{
				return true;
			}
			FString Normalized = Stem;
			Normalized.ReplaceInline(TEXT("-"), TEXT("_"));
			Normalized.ReplaceInline(TEXT(" "), TEXT("_"));
			Normalized.ReplaceInline(TEXT("."), TEXT("_"));
			TArray<FString> Parts;
			Normalized.ParseIntoArray(Parts, TEXT("_"), true);
			for (const FString& Part : Parts)
			{
				if (Part.Equals(Needle))
				{
					return true;
				}
			}
			return false;
		}

		bool ContainsIgnoreCase(const FString& Stem, const TCHAR* Token)
		{
			return Stem.Contains(FString(Token), ESearchCase::IgnoreCase);
		}

		TextureCompressionSettings CompressionForKind(EPBRMapKind Kind)
		{
			switch (Kind)
			{
			case EPBRMapKind::Normal:
				return TC_Normalmap;
			case EPBRMapKind::BaseColor:
				return TC_Default;
			default:
				return TC_Grayscale;
			}
		}

		bool SRGBForKind(EPBRMapKind Kind)
		{
			return Kind == EPBRMapKind::BaseColor;
		}

		TextureGroup GroupForKind(EPBRMapKind Kind)
		{
			return Kind == EPBRMapKind::Normal ? TEXTUREGROUP_WorldNormalMap : TEXTUREGROUP_World;
		}

		ETextureSourceColorSpace ColorSpaceForKind(EPBRMapKind Kind)
		{
			return Kind == EPBRMapKind::BaseColor ? ETextureSourceColorSpace::SRGB : ETextureSourceColorSpace::Linear;
		}

		void ApplyKindSettings(UTexture2D* Texture, EPBRMapKind Kind)
		{
			if (!Texture || Kind == EPBRMapKind::Unknown)
			{
				return;
			}
			Texture->CompressionSettings = CompressionForKind(Kind);
			Texture->SRGB = SRGBForKind(Kind);
			Texture->LODGroup = GroupForKind(Kind);
			Texture->AddressX = TA_Wrap;
			Texture->AddressY = TA_Wrap;
			Texture->PostEditChange();
			Texture->MarkPackageDirty();
		}

		bool IsSupportedLocalImageExtension(const FString& Extension)
		{
			return Extension.Equals(TEXT("png"), ESearchCase::IgnoreCase)
				|| Extension.Equals(TEXT("jpg"), ESearchCase::IgnoreCase)
				|| Extension.Equals(TEXT("jpeg"), ESearchCase::IgnoreCase)
				|| Extension.Equals(TEXT("bmp"), ESearchCase::IgnoreCase)
				|| Extension.Equals(TEXT("tga"), ESearchCase::IgnoreCase);
		}

		UTexture2D** TextureSlotForKind(FPBRImportedTextures& Textures, EPBRMapKind Kind)
		{
			switch (Kind)
			{
			case EPBRMapKind::BaseColor:
				return &Textures.BaseColor;
			case EPBRMapKind::Normal:
				return &Textures.Normal;
			case EPBRMapKind::Roughness:
				return &Textures.Roughness;
			case EPBRMapKind::Metallic:
				return &Textures.Metallic;
			case EPBRMapKind::Height:
				return &Textures.Height;
			case EPBRMapKind::AO:
				return &Textures.AO;
			default:
				return nullptr;
			}
		}
	}

	EPBRMapKind GuessPBRMapKindFromFilename(const FString& Filename)
	{
		const FString Stem = FPaths::GetBaseFilename(Filename).ToLower();
		if (Stem.IsEmpty())
		{
			return EPBRMapKind::Unknown;
		}

		if (HasDelimitedToken(Stem, TEXT("normal"))
			|| HasDelimitedToken(Stem, TEXT("norm"))
			|| HasDelimitedToken(Stem, TEXT("nrm"))
			|| HasDelimitedToken(Stem, TEXT("nor"))
			|| HasDelimitedToken(Stem, TEXT("n"))
			|| ContainsIgnoreCase(Stem, TEXT("法线")))
		{
			return EPBRMapKind::Normal;
		}
		if (HasDelimitedToken(Stem, TEXT("roughness"))
			|| HasDelimitedToken(Stem, TEXT("rough"))
			|| HasDelimitedToken(Stem, TEXT("rgh"))
			|| HasDelimitedToken(Stem, TEXT("r"))
			|| ContainsIgnoreCase(Stem, TEXT("粗糙度"))
			|| ContainsIgnoreCase(Stem, TEXT("粗糙")))
		{
			return EPBRMapKind::Roughness;
		}
		if (HasDelimitedToken(Stem, TEXT("metallic"))
			|| HasDelimitedToken(Stem, TEXT("metalness"))
			|| HasDelimitedToken(Stem, TEXT("metal"))
			|| HasDelimitedToken(Stem, TEXT("met"))
			|| HasDelimitedToken(Stem, TEXT("m"))
			|| ContainsIgnoreCase(Stem, TEXT("金属度"))
			|| ContainsIgnoreCase(Stem, TEXT("金属")))
		{
			return EPBRMapKind::Metallic;
		}
		if (HasDelimitedToken(Stem, TEXT("height"))
			|| HasDelimitedToken(Stem, TEXT("disp"))
			|| HasDelimitedToken(Stem, TEXT("displacement"))
			|| HasDelimitedToken(Stem, TEXT("bump"))
			|| HasDelimitedToken(Stem, TEXT("h"))
			|| ContainsIgnoreCase(Stem, TEXT("置换"))
			|| ContainsIgnoreCase(Stem, TEXT("高度")))
		{
			return EPBRMapKind::Height;
		}
		if (HasDelimitedToken(Stem, TEXT("ambientocclusion"))
			|| HasDelimitedToken(Stem, TEXT("occlusion"))
			|| HasDelimitedToken(Stem, TEXT("ao"))
			|| ContainsIgnoreCase(Stem, TEXT("环境光"))
			|| ContainsIgnoreCase(Stem, TEXT("遮蔽")))
		{
			return EPBRMapKind::AO;
		}
		if (HasDelimitedToken(Stem, TEXT("basecolor"))
			|| HasDelimitedToken(Stem, TEXT("albedo"))
			|| HasDelimitedToken(Stem, TEXT("diffuse"))
			|| HasDelimitedToken(Stem, TEXT("col"))
			|| HasDelimitedToken(Stem, TEXT("color"))
			|| HasDelimitedToken(Stem, TEXT("d"))
			|| ContainsIgnoreCase(Stem, TEXT("基础色"))
			|| ContainsIgnoreCase(Stem, TEXT("基础贴图")))
		{
			return EPBRMapKind::BaseColor;
		}
		return EPBRMapKind::Unknown;
	}

	UTexture2D* ImportLocalImageFile(
		const FString& Filename,
		const FString& DestinationPath,
		const FString& DesiredName,
		EPBRMapKind Kind,
		EPBRImportConflictPolicy ConflictPolicy,
		bool bSave,
		FString* OutError,
		const bool bMakeSeamless)
	{
		if (!ImportIsGameThread(OutError))
		{
			return nullptr;
		}
		if (Filename.IsEmpty() || !IFileManager::Get().FileExists(*Filename))
		{
			ImportSetError(OutError, FString::Printf(TEXT("Local image does not exist: %s"), *Filename));
			return nullptr;
		}
		if (!IsSupportedLocalImageExtension(FPaths::GetExtension(Filename)))
		{
			ImportSetError(OutError, FString::Printf(TEXT("Unsupported image type: %s"), *Filename));
			return nullptr;
		}

		const FString NormalizedDest = ImportNormalizeContentPath(DestinationPath);
		if (!NormalizedDest.StartsWith(TEXT("/Game")))
		{
			ImportSetError(OutError, FString::Printf(TEXT("Destination path must be under /Game: %s"), *NormalizedDest));
			return nullptr;
		}

		FString AssetName = ObjectTools::SanitizeObjectName(
			DesiredName.IsEmpty() ? FPaths::GetBaseFilename(Filename) : DesiredName);
		if (AssetName.IsEmpty())
		{
			ImportSetError(OutError, TEXT("Asset name is empty after sanitizing."));
			return nullptr;
		}

		FString PackageName = NormalizedDest / AssetName;
		bool bReplaceExisting = false;
		if (ImportAssetExists(PackageName, AssetName))
		{
			switch (ConflictPolicy)
			{
			case EPBRImportConflictPolicy::Cancel:
				if (OutError)
				{
					*OutError = FString::Printf(TEXT("Name conflict: %s"), *PackageName);
				}
				UE_LOG(LogPBRTextureLab, Warning, TEXT("ImportLocalImageFile name conflict (cancel): %s"), *PackageName);
				return nullptr;
			case EPBRImportConflictPolicy::Replace:
				if (!PrepareExistingPackageForReplace(PackageName, OutError))
				{
					return nullptr;
				}
				bReplaceExisting = ImportAssetExists(PackageName, AssetName);
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

		FString ImportFilename = Filename;
		TUniquePtr<FStagingSession> Staging;
		if (bMakeSeamless)
		{
			const FString SessionDir = FPaths::Combine(GetStagingRootDirectory(), FGuid::NewGuid().ToString(EGuidFormats::Digits));
			Staging = MakeUnique<FStagingSession>(SessionDir);
			IFileManager::Get().MakeDirectory(*SessionDir, true);
			const FString StagingFile = FPaths::Combine(SessionDir, AssetName + TEXT(".png"));
			if (!MakeLocalFileSeamless(Filename, Kind, StagingFile, OutError))
			{
				return nullptr;
			}
			ImportFilename = StagingFile;
		}

		UTextureFactory* Factory = NewObject<UTextureFactory>(GetTransientPackage());
		Factory->bCreateMaterial = 0;
		Factory->CompressionSettings = CompressionForKind(Kind == EPBRMapKind::Unknown ? EPBRMapKind::BaseColor : Kind);
		Factory->ColorSpaceMode = ColorSpaceForKind(Kind == EPBRMapKind::Unknown ? EPBRMapKind::BaseColor : Kind);
		Factory->LODGroup = GroupForKind(Kind);
		Factory->bFlipNormalMapGreenChannel = 0;
		UTextureFactory::SuppressImportOverwriteDialog(true);

		UAssetImportTask* Task = NewObject<UAssetImportTask>(GetTransientPackage());
		Task->Filename = ImportFilename;
		Task->DestinationPath = NormalizedDest;
		Task->DestinationName = AssetName;
		Task->bReplaceExisting = bReplaceExisting;
		Task->bReplaceExistingSettings = bReplaceExisting;
		Task->bAutomated = true;
		Task->bAsync = false;
		Task->bSave = false;
		Task->Factory = Factory;

		{
			FScopedTransaction Transaction(NSLOCTEXT("PBRTextureLab", "ImportLocalImage", "Import Local PBR Image"));
			TArray<UAssetImportTask*> Tasks;
			Tasks.Add(Task);
			ImportAssetTasks(Tasks);
		}

		UTexture2D* Texture = FindImportedTexture(Task, AssetName);
		if (!Texture)
		{
			ImportSetError(OutError, FString::Printf(TEXT("Import failed for %s"), *Filename));
			return nullptr;
		}

		if (Kind != EPBRMapKind::Unknown)
		{
			ApplyKindSettings(Texture, Kind);
		}

		if (bSave)
		{
			TArray<UPackage*> Packages;
			Packages.Add(Texture->GetOutermost());
			if (!UEditorLoadingAndSavingUtils::SavePackages(Packages, false))
			{
				ImportSetError(OutError, FString::Printf(TEXT("Failed to save imported texture %s"), *Texture->GetPathName()));
				return nullptr;
			}
		}

		UE_LOG(LogPBRTextureLab, Log, TEXT("Imported local image %s -> %s"), *Filename, *Texture->GetPathName());
		return Texture;
	}

	EPBRImportStatus ImportPBRMapsFromLocalFolder(
		const FString& FolderPath,
		const FString& DestinationPath,
		const FString& BaseName,
		EPBRImportConflictPolicy ConflictPolicy,
		bool bSave,
		FPBRImportedTextures& OutTextures,
		FString* OutError,
		const bool bMakeSeamless)
	{
		OutTextures = FPBRImportedTextures();
		if (!ImportIsGameThread(OutError))
		{
			return EPBRImportStatus::Failed;
		}
		if (FolderPath.IsEmpty() || !IFileManager::Get().DirectoryExists(*FolderPath))
		{
			ImportSetError(OutError, FString::Printf(TEXT("Local folder does not exist: %s"), *FolderPath));
			return EPBRImportStatus::Failed;
		}

		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(FolderPath / TEXT("*.*")), true, false);
		int32 Imported = 0;
		for (const FString& File : Files)
		{
			if (!IsSupportedLocalImageExtension(FPaths::GetExtension(File)))
			{
				continue;
			}
			const FString FullPath = FolderPath / File;
			const EPBRMapKind Kind = GuessPBRMapKindFromFilename(File);
			UTexture2D** Slot = TextureSlotForKind(OutTextures, Kind);
			if (!Slot || *Slot)
			{
				continue;
			}

			FString DesiredName = ObjectTools::SanitizeObjectName(BaseName);
			if (DesiredName.IsEmpty())
			{
				DesiredName = FPaths::GetBaseFilename(File);
			}
			else
			{
				switch (Kind)
				{
				case EPBRMapKind::BaseColor:
					DesiredName += TEXT("_BaseColor");
					break;
				case EPBRMapKind::Normal:
					DesiredName += TEXT("_Normal");
					break;
				case EPBRMapKind::Roughness:
					DesiredName += TEXT("_Roughness");
					break;
				case EPBRMapKind::Metallic:
					DesiredName += TEXT("_Metallic");
					break;
				case EPBRMapKind::Height:
					DesiredName += TEXT("_Height");
					break;
				case EPBRMapKind::AO:
					DesiredName += TEXT("_AO");
					break;
				default:
					break;
				}
			}

			FString FileError;
			UTexture2D* Texture = ImportLocalImageFile(
				FullPath,
				DestinationPath,
				DesiredName,
				Kind,
				ConflictPolicy,
				bSave,
				&FileError,
				bMakeSeamless);
			if (!Texture)
			{
				UE_LOG(LogPBRTextureLab, Warning, TEXT("Skipped local map %s: %s"), *FullPath, *FileError);
				continue;
			}
			*Slot = Texture;
			++Imported;
		}

		if (Imported == 0)
		{
			ImportSetError(OutError, FString::Printf(TEXT("No matching PBR images in folder: %s"), *FolderPath));
			return EPBRImportStatus::Failed;
		}

		UE_LOG(LogPBRTextureLab, Log, TEXT("Imported %d local PBR maps from %s"), Imported, *FolderPath);
		return EPBRImportStatus::Success;
	}

	EPBRImportStatus ImportPBRMaps(
		const FPBRMaps& Maps,
		const FPBRTextureImportRequest& Request,
		FPBRImportedTextures& OutTextures,
		FString* OutError)
	{
		OutTextures = FPBRImportedTextures();

		if (!ImportIsGameThread(OutError))
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

		if (!Request.ExportFlags.WantsAnyTexture())
		{
			UE_LOG(LogPBRTextureLab, Log, TEXT("ImportPBRMaps skipped: no map export flags enabled."));
			return EPBRImportStatus::Success;
		}

		const FString DestinationPath = ImportNormalizeContentPath(Request.DestinationPath);
		if (!DestinationPath.StartsWith(TEXT("/Game")))
		{
			ImportSetError(OutError, FString::Printf(TEXT("Destination path must be under /Game: %s"), *DestinationPath));
			return EPBRImportStatus::Failed;
		}

		const FString SanitizedBase = ObjectTools::SanitizeObjectName(Request.BaseName);
		if (SanitizedBase.IsEmpty())
		{
			ImportSetError(OutError, TEXT("BaseName is empty after sanitizing."));
			return EPBRImportStatus::Failed;
		}

		FChannelJob Jobs[] = {
			{ TEXT("_BaseColor"), &Maps.BaseColor, &OutTextures.BaseColor, TC_Default, true, TEXTUREGROUP_World, ETextureSourceColorSpace::SRGB, Request.ExportFlags.bBaseColor },
			{ TEXT("_Height"), &Maps.Height, &OutTextures.Height, TC_Grayscale, false, TEXTUREGROUP_World, ETextureSourceColorSpace::Linear, Request.ExportFlags.bHeight },
			{ TEXT("_Normal"), &Maps.Normal, &OutTextures.Normal, TC_Normalmap, false, TEXTUREGROUP_WorldNormalMap, ETextureSourceColorSpace::Linear, Request.ExportFlags.bNormal },
			{ TEXT("_AO"), &Maps.AO, &OutTextures.AO, TC_Grayscale, false, TEXTUREGROUP_World, ETextureSourceColorSpace::Linear, Request.ExportFlags.bAO },
			{ TEXT("_Roughness"), &Maps.Roughness, &OutTextures.Roughness, TC_Grayscale, false, TEXTUREGROUP_World, ETextureSourceColorSpace::Linear, Request.ExportFlags.bRoughness },
			{ TEXT("_Metallic"), &Maps.Metallic, &OutTextures.Metallic, TC_Grayscale, false, TEXTUREGROUP_World, ETextureSourceColorSpace::Linear, Request.ExportFlags.bMetallic }
		};

		for (const FChannelJob& Job : Jobs)
		{
			if (Job.bEnabled && !IsValidImage(*Job.Image))
			{
				ImportSetError(OutError, TEXT("ImportPBRMaps rejected invalid pixel maps."));
				return EPBRImportStatus::Failed;
			}
		}

		struct FResolvedName
		{
			FString AssetName;
			FString PackageName;
			bool bReplaceExisting = false;
		};
		FResolvedName Resolved[UE_ARRAY_COUNT(Jobs)];

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Jobs); ++Index)
		{
			if (!Jobs[Index].bEnabled)
			{
				continue;
			}
			FString AssetName = ObjectTools::SanitizeObjectName(SanitizedBase + Jobs[Index].Suffix);
			FString PackageName = DestinationPath / AssetName;
			if (!FPackageName::IsValidLongPackageName(PackageName))
			{
				ImportSetError(OutError, FString::Printf(TEXT("Invalid package name: %s"), *PackageName));
				return EPBRImportStatus::Failed;
			}

			const bool bExists = ImportAssetExists(PackageName, AssetName);
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
					Resolved[Index].bReplaceExisting = ImportAssetExists(PackageName, AssetName);
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
			if (!Jobs[Index].bEnabled)
			{
				continue;
			}
			const FString StagingFile = FPaths::Combine(SessionDir, Resolved[Index].AssetName + TEXT(".png"));
			FPBRImageRgba8 SeamlessImage = *Jobs[Index].Image;
			if (Request.bMakeSeamless
				&& !MakeSeamlessImage(SeamlessImage, Jobs[Index].Compression == TC_Normalmap))
			{
				ImportSetError(OutError, TEXT("Failed to convert imported map to a seamless tile."));
				return EPBRImportStatus::Failed;
			}
			if (!EncodePng(SeamlessImage, Jobs[Index].bSRGB, StagingFile, OutError))
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

		int32 TaskIndex = 0;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Jobs); ++Index)
		{
			if (!Jobs[Index].bEnabled)
			{
				continue;
			}
			UTexture2D* Texture = FindImportedTexture(Tasks[TaskIndex], Resolved[Index].AssetName);
			++TaskIndex;
			if (!Texture)
			{
				RollbackCreated(NewlyCreated);
				OutTextures = FPBRImportedTextures();
				ImportSetError(OutError, FString::Printf(TEXT("Import failed for %s"), *Resolved[Index].AssetName));
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
				ImportSetError(OutError, TEXT("Failed to save imported texture packages."));
				return EPBRImportStatus::Failed;
			}
		}

		UE_LOG(LogPBRTextureLab, Log, TEXT("Imported PBR maps to %s/%s*"), *DestinationPath, *SanitizedBase);
		return EPBRImportStatus::Success;
	}
}
