#include "PBRTextureLabPipeline.h"
#include "PBRTextureLabEditorApi.h"
#include "PBRTextureLabPixelCore.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "UObject/Package.h"

namespace PBRTextureLab
{
	namespace
	{
		void PipelineSetError(FString* OutError, const FString& Message)
		{
			UE_LOG(LogPBRTextureLab, Error, TEXT("%s"), *Message);
			if (OutError)
			{
				*OutError = Message;
			}
		}

		EPBRImportStatus ResolveMaterialFolder(
			const FPBRGenerateRequest& Request,
			FString& OutFolder,
			FString& OutMaterialName,
			FString* OutError)
		{
			FString RootDest = Request.DestinationPath;
			RootDest.ReplaceInline(TEXT("\\"), TEXT("/"));
			while (RootDest.Len() > 1 && RootDest.EndsWith(TEXT("/")))
			{
				RootDest.LeftChopInline(1);
			}
			if (!RootDest.StartsWith(TEXT("/Game")))
			{
				PipelineSetError(OutError, FString::Printf(TEXT("Destination path must be under /Game: %s"), *RootDest));
				return EPBRImportStatus::Failed;
			}

			if (Request.bModifyExisting)
			{
				UMaterialInstanceConstant* Existing = Request.ExistingInstance;
				if (!IsValid(Existing))
				{
					PipelineSetError(OutError, TEXT("Modify requires an existing material instance."));
					return EPBRImportStatus::Failed;
				}
				OutMaterialName = Existing->GetName();
				OutFolder = FPackageName::GetLongPackagePath(Existing->GetOutermost()->GetName());
				return EPBRImportStatus::Success;
			}

			OutMaterialName = ObjectTools::SanitizeObjectName(
				Request.MaterialInstanceName.IsEmpty() ? Request.BaseName : Request.MaterialInstanceName);
			if (OutMaterialName.IsEmpty())
			{
				PipelineSetError(OutError, TEXT("Material name is empty after sanitizing."));
				return EPBRImportStatus::Failed;
			}

			auto FolderHasMaterial = [&RootDest](const FString& Name)
			{
				return FPackageName::DoesPackageExist(RootDest / Name / Name);
			};

			if (FolderHasMaterial(OutMaterialName))
			{
				switch (Request.ConflictPolicy)
				{
				case EPBRImportConflictPolicy::Cancel:
					if (OutError)
					{
						*OutError = FString::Printf(TEXT("Name conflict: %s/%s"), *RootDest, *OutMaterialName);
					}
					UE_LOG(LogPBRTextureLab, Warning, TEXT("Name conflict (cancel): %s/%s"), *RootDest, *OutMaterialName);
					return EPBRImportStatus::NameConflict;
				case EPBRImportConflictPolicy::UniqueName:
					{
						int32 Suffix = 2;
						FString Candidate = FString::Printf(TEXT("%s_%d"), *OutMaterialName, Suffix);
						while (FolderHasMaterial(Candidate))
						{
							++Suffix;
							Candidate = FString::Printf(TEXT("%s_%d"), *OutMaterialName, Suffix);
						}
						OutMaterialName = Candidate;
					}
					break;
				case EPBRImportConflictPolicy::Replace:
					break;
				}
			}

			OutFolder = RootDest / OutMaterialName;
			return EPBRImportStatus::Success;
		}

		bool RewriteTextureSeamless(UTexture2D* Texture)
		{
			if (!Texture)
			{
				return false;
			}
			FPBRImageRgba8 Image;
			if (!ReadSourceTexture2D(Texture, Image, nullptr))
			{
				return false;
			}
			if (!MakeSeamlessImage(Image, Texture->CompressionSettings == TC_Normalmap))
			{
				return false;
			}
			TArray<uint8> Bytes;
			Bytes.SetNumUninitialized(Image.Pixels.Num() * sizeof(FColor));
			FMemory::Memcpy(Bytes.GetData(), Image.Pixels.GetData(), Bytes.Num());
			Texture->PreEditChange(nullptr);
			Texture->Source.Init(Image.Width, Image.Height, 1, 1, TSF_BGRA8, Bytes.GetData());
			Texture->AddressX = TA_Wrap;
			Texture->AddressY = TA_Wrap;
			Texture->UpdateResource();
			Texture->PostEditChange();
			Texture->MarkPackageDirty();
			return true;
		}

		UTexture2D* DuplicateTextureIntoFolder(
			UTexture2D* Source,
			const FString& Folder,
			const FString& AssetName,
			const bool bMakeSeamless)
		{
			if (!Source)
			{
				return nullptr;
			}
			const FString DestPackage = Folder / AssetName;
			if (Source->GetOutermost()->GetName() == DestPackage)
			{
				if (bMakeSeamless)
				{
					RewriteTextureSeamless(Source);
				}
				return Source;
			}
			if (FPackageName::DoesPackageExist(DestPackage))
			{
				if (UPackage* ExistingPackage = FindPackage(nullptr, *DestPackage))
				{
					ResetLoaders(ExistingPackage);
					TArray<UPackage*> ToUnload;
					ToUnload.Add(ExistingPackage);
					FText UnloadError;
					UPackageTools::UnloadPackages(ToUnload, UnloadError, true);
				}
				FString Filename;
				if (FPackageName::DoesPackageExist(DestPackage, &Filename))
				{
					IFileManager::Get().Delete(*Filename, false, true, true);
				}
			}
			UTexture2D* Duplicated = nullptr;
			if (UObject* Object = GetAssetTools().DuplicateAsset(AssetName, Folder, Source))
			{
				Duplicated = Cast<UTexture2D>(Object);
			}
			if (!Duplicated)
			{
				return Source;
			}
			if (bMakeSeamless)
			{
				RewriteTextureSeamless(Duplicated);
			}
			return Duplicated;
		}

		bool PipelineCopyBgra(const FImage& Bgra, FPBRImageRgba8& OutImage, FString* OutError)
		{
			if (Bgra.Format != ERawImageFormat::BGRA8 || Bgra.SizeX <= 0 || Bgra.SizeY <= 0)
			{
				PipelineSetError(OutError, TEXT("Converted image is not a valid BGRA8 buffer."));
				return false;
			}

			const int64 PixelCount64 = static_cast<int64>(Bgra.SizeX) * static_cast<int64>(Bgra.SizeY);
			if (PixelCount64 > MAX_int32)
			{
				PipelineSetError(OutError, TEXT("Source image is too large for the pixel core."));
				return false;
			}

			const TArrayView64<const FColor> Colors = Bgra.AsBGRA8();
			const int32 PixelCount = static_cast<int32>(PixelCount64);
			if (Colors.Num() < PixelCount)
			{
				PipelineSetError(OutError, TEXT("Converted BGRA buffer is smaller than Width*Height."));
				return false;
			}

			OutImage.Width = Bgra.SizeX;
			OutImage.Height = Bgra.SizeY;
			OutImage.Pixels.SetNumUninitialized(PixelCount);
			FMemory::Memcpy(OutImage.Pixels.GetData(), Colors.GetData(), sizeof(FColor) * PixelCount);
			return true;
		}
	}

	bool ConvertImageToRgba8(const FImage& Image, FPBRImageRgba8& OutImage, FString* OutError)
	{
		FImage Bgra;
		Image.CopyTo(Bgra, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		return PipelineCopyBgra(Bgra, OutImage, OutError);
	}

	bool ReadSourceTexture2D(UTexture2D* Texture, FPBRImageRgba8& OutImage, FString* OutError)
	{
		OutImage = FPBRImageRgba8();
		if (!Texture)
		{
			PipelineSetError(OutError, TEXT("ReadSourceTexture2D requires a Texture2D."));
			return false;
		}
		if (!Texture->Source.IsValid())
		{
			PipelineSetError(
				OutError,
				FString::Printf(TEXT("Texture2D has no source art: %s"), *Texture->GetPathName()));
			return false;
		}

		FImage SourceImage;
		if (!FImageUtils::GetTexture2DSourceImage(Texture, SourceImage))
		{
			PipelineSetError(
				OutError,
				FString::Printf(TEXT("GetTexture2DSourceImage failed: %s"), *Texture->GetPathName()));
			return false;
		}
		return ConvertImageToRgba8(SourceImage, OutImage, OutError);
	}

	bool ReadSourceLocalFile(const FString& Filename, FPBRImageRgba8& OutImage, FString* OutError)
	{
		OutImage = FPBRImageRgba8();
		if (Filename.IsEmpty() || !IFileManager::Get().FileExists(*Filename))
		{
			PipelineSetError(
				OutError,
				FString::Printf(TEXT("Local image does not exist: %s"), *Filename));
			return false;
		}

		TArray64<uint8> FileData;
		if (!FFileHelper::LoadFileToArray(FileData, *Filename))
		{
			PipelineSetError(OutError, FString::Printf(TEXT("Failed to read image file: %s"), *Filename));
			return false;
		}

		FImage Decoded;
		if (!FImageUtils::DecompressImage(FileData.GetData(), FileData.Num(), Decoded))
		{
			PipelineSetError(OutError, FString::Printf(TEXT("Failed to decode image file: %s"), *Filename));
			return false;
		}
		return ConvertImageToRgba8(Decoded, OutImage, OutError);
	}

	bool WriteRgba8Png(const FPBRImageRgba8& Image, const FString& Filename, FString* OutError)
	{
		if (!IsValidImage(Image))
		{
			PipelineSetError(OutError, TEXT("WriteRgba8Png rejected invalid image."));
			return false;
		}

		FImage Encoded(Image.Width, Image.Height, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		TArrayView64<FColor> Pixels = Encoded.AsBGRA8();
		for (int32 Index = 0; Index < Image.Pixels.Num(); ++Index)
		{
			Pixels[Index] = Image.Pixels[Index];
		}

		TArray64<uint8> Compressed;
		if (!FImageUtils::CompressImage(Compressed, TEXT("png"), Encoded))
		{
			PipelineSetError(OutError, FString::Printf(TEXT("CompressImage failed: %s"), *Filename));
			return false;
		}
		if (!FFileHelper::SaveArrayToFile(Compressed, *Filename))
		{
			PipelineSetError(OutError, FString::Printf(TEXT("Failed to write PNG: %s"), *Filename));
			return false;
		}
		return true;
	}

	bool PrepareWorkingImage(
		const FPBRImageRgba8& Source,
		int32 OutputWidth,
		int32 OutputHeight,
		FPBRImageRgba8& OutImage,
		FString* OutError)
	{
		OutImage = FPBRImageRgba8();
		if (!IsValidImage(Source))
		{
			PipelineSetError(OutError, TEXT("PrepareWorkingImage rejected invalid source."));
			return false;
		}
		if (OutputWidth < 0 || OutputHeight < 0)
		{
			PipelineSetError(OutError, TEXT("Output size cannot be negative."));
			return false;
		}

		const int32 Width = OutputWidth > 0 ? OutputWidth : Source.Width;
		const int32 Height = OutputHeight > 0 ? OutputHeight : Source.Height;
		if (Width > MaxOutputDimension || Height > MaxOutputDimension)
		{
			PipelineSetError(
				OutError,
				FString::Printf(
					TEXT("Output size %dx%d exceeds the %d limit."),
					Width,
					Height,
					MaxOutputDimension));
			return false;
		}

		if (Width == Source.Width && Height == Source.Height)
		{
			OutImage = Source;
			return true;
		}

		FImage Src(Source.Width, Source.Height, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		TArrayView64<FColor> SrcPixels = Src.AsBGRA8();
		for (int32 Index = 0; Index < Source.Pixels.Num(); ++Index)
		{
			SrcPixels[Index] = Source.Pixels[Index];
		}

		FImage Resized;
		Src.ResizeTo(Resized, Width, Height, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		return PipelineCopyBgra(Resized, OutImage, OutError);
	}

	bool LoadGenerateSource(
		const FPBRGenerateRequest& Request,
		FPBRImageRgba8& OutImage,
		FString* OutError)
	{
		if (Request.SourceTexture)
		{
			return ReadSourceTexture2D(Request.SourceTexture, OutImage, OutError);
		}
		if (!Request.LocalImagePath.IsEmpty())
		{
			return ReadSourceLocalFile(Request.LocalImagePath, OutImage, OutError);
		}
		PipelineSetError(OutError, TEXT("Select a Texture2D or a local image."));
		return false;
	}

	EPBRImportStatus GenerateAndImportFromSource(
		const FPBRGenerateRequest& Request,
		FPBRGenerateResult& OutResult,
		FString* OutError)
	{
		OutResult = FPBRGenerateResult();

		if (Request.bCancelled)
		{
			if (OutError)
			{
				*OutError = TEXT("Generate cancelled.");
			}
			UE_LOG(LogPBRTextureLab, Log, TEXT("GenerateAndImportFromSource cancelled before work started."));
			return EPBRImportStatus::Cancelled;
		}

		if (!IsInGameThread())
		{
			PipelineSetError(OutError, TEXT("GenerateAndImportFromSource must run on the Game Thread."));
			return EPBRImportStatus::Failed;
		}

		FPBRImageRgba8 SourceImage;
		if (!LoadGenerateSource(Request, SourceImage, OutError))
		{
			return EPBRImportStatus::Failed;
		}
		if (!PrepareWorkingImage(SourceImage, Request.OutputWidth, Request.OutputHeight, OutResult.WorkingImage, OutError))
		{
			return EPBRImportStatus::Failed;
		}

		if (Request.bMakeSeamless && !MakeSeamlessImage(OutResult.WorkingImage, false))
		{
			PipelineSetError(OutError, TEXT("Failed to convert the source image to a seamless tile."));
			return EPBRImportStatus::Failed;
		}

		if (!Request.ExportFlags.WantsAnyTexture() && !Request.bCreateMaterial)
		{
			PipelineSetError(OutError, TEXT("Select at least one map or enable material creation."));
			return EPBRImportStatus::Failed;
		}

		if (!GeneratePBRMaps(OutResult.WorkingImage, Request.PixelParams, OutResult.Maps, &OutResult.Disclaimer))
		{
			PipelineSetError(OutError, TEXT("GeneratePBRMaps failed."));
			return EPBRImportStatus::Failed;
		}

		FString OutputFolder;
		FString MaterialName;
		const EPBRImportStatus FolderStatus = ResolveMaterialFolder(Request, OutputFolder, MaterialName, OutError);
		if (FolderStatus != EPBRImportStatus::Success)
		{
			return FolderStatus;
		}
		OutResult.OutputFolder = OutputFolder;
		OutResult.CreatedMaterialName = MaterialName;

		FPBRTextureImportRequest ImportRequest;
		ImportRequest.DestinationPath = OutputFolder;
		ImportRequest.BaseName = MaterialName;
		ImportRequest.ConflictPolicy = Request.ConflictPolicy;
		ImportRequest.ExportFlags = Request.ExportFlags;
		ImportRequest.bMakeSeamless = false;
		ImportRequest.bSave = Request.bSave;
		ImportRequest.bCancelled = false;

		const EPBRImportStatus ImportStatus = ImportPBRMaps(
			OutResult.Maps,
			ImportRequest,
			OutResult.Textures,
			OutError);
		if (ImportStatus != EPBRImportStatus::Success)
		{
			return ImportStatus;
		}

		if (!Request.bCreateMaterial)
		{
			return EPBRImportStatus::Success;
		}

		FPBRMaterialInstanceRequest MaterialRequest;
		MaterialRequest.DestinationPath = OutputFolder;
		MaterialRequest.BaseName = MaterialName;
		MaterialRequest.InstanceName = MaterialName;
		MaterialRequest.ParentMaterial = Request.ParentMaterial;
		MaterialRequest.ConflictPolicy = Request.ConflictPolicy;
		MaterialRequest.NormalStrength = Request.MaterialNormalStrength;
		MaterialRequest.RoughnessStrength = Request.MaterialRoughnessStrength;
		MaterialRequest.MetallicStrength = Request.MaterialMetallicStrength;
		MaterialRequest.HeightAmount = Request.MaterialHeightAmount;
		MaterialRequest.UVScale = Request.MaterialUVScale;
		MaterialRequest.RoughnessBrightness = Request.MaterialRoughnessBrightness;
		MaterialRequest.EnabledMaps = Request.ExportFlags;
		MaterialRequest.ExistingInstance = Request.ExistingInstance;
		MaterialRequest.bModifyExisting = Request.bModifyExisting;
		MaterialRequest.bSave = Request.bSave;
		MaterialRequest.bCancelled = false;

		return CreateMaterialInstance(
			OutResult.Textures,
			MaterialRequest,
			OutResult.MaterialInstance,
			OutError);
	}

	EPBRImportStatus CreateMaterialFromExistingTextures(
		const FPBRGenerateRequest& Request,
		const FPBRImportedTextures& ExistingTextures,
		FPBRGenerateResult& OutResult,
		FString* OutError)
	{
		OutResult = FPBRGenerateResult();
		OutResult.Textures = ExistingTextures;

		if (Request.bCancelled)
		{
			if (OutError)
			{
				*OutError = TEXT("Generate cancelled.");
			}
			return EPBRImportStatus::Cancelled;
		}
		if (!IsInGameThread())
		{
			PipelineSetError(OutError, TEXT("CreateMaterialFromExistingTextures must run on the Game Thread."));
			return EPBRImportStatus::Failed;
		}
		if (!ExistingTextures.HasAny())
		{
			PipelineSetError(OutError, TEXT("Select at least one existing PBR texture."));
			return EPBRImportStatus::Failed;
		}

		FString OutputFolder;
		FString MaterialName;
		const EPBRImportStatus FolderStatus = ResolveMaterialFolder(Request, OutputFolder, MaterialName, OutError);
		if (FolderStatus != EPBRImportStatus::Success)
		{
			return FolderStatus;
		}
		OutResult.OutputFolder = OutputFolder;
		OutResult.CreatedMaterialName = MaterialName;

		if (Request.bCopyExistingTexturesToFolder)
		{
			OutResult.Textures.BaseColor = DuplicateTextureIntoFolder(ExistingTextures.BaseColor, OutputFolder, MaterialName + TEXT("_BaseColor"), Request.bMakeSeamless);
			OutResult.Textures.Normal = DuplicateTextureIntoFolder(ExistingTextures.Normal, OutputFolder, MaterialName + TEXT("_Normal"), Request.bMakeSeamless);
			OutResult.Textures.Height = DuplicateTextureIntoFolder(ExistingTextures.Height, OutputFolder, MaterialName + TEXT("_Height"), Request.bMakeSeamless);
			OutResult.Textures.AO = DuplicateTextureIntoFolder(ExistingTextures.AO, OutputFolder, MaterialName + TEXT("_AO"), Request.bMakeSeamless);
			OutResult.Textures.Roughness = DuplicateTextureIntoFolder(ExistingTextures.Roughness, OutputFolder, MaterialName + TEXT("_Roughness"), Request.bMakeSeamless);
			OutResult.Textures.Metallic = DuplicateTextureIntoFolder(ExistingTextures.Metallic, OutputFolder, MaterialName + TEXT("_Metallic"), Request.bMakeSeamless);
		}

		FPBRMaterialInstanceRequest MaterialRequest;
		MaterialRequest.DestinationPath = OutputFolder;
		MaterialRequest.BaseName = MaterialName;
		MaterialRequest.InstanceName = MaterialName;
		MaterialRequest.ParentMaterial = Request.ParentMaterial;
		MaterialRequest.ConflictPolicy = Request.ConflictPolicy;
		MaterialRequest.NormalStrength = Request.MaterialNormalStrength;
		MaterialRequest.RoughnessStrength = Request.MaterialRoughnessStrength;
		MaterialRequest.MetallicStrength = Request.MaterialMetallicStrength;
		MaterialRequest.HeightAmount = Request.MaterialHeightAmount;
		MaterialRequest.UVScale = Request.MaterialUVScale;
		MaterialRequest.RoughnessBrightness = Request.MaterialRoughnessBrightness;
		MaterialRequest.EnabledMaps.bBaseColor = OutResult.Textures.BaseColor != nullptr;
		MaterialRequest.EnabledMaps.bNormal = OutResult.Textures.Normal != nullptr;
		MaterialRequest.EnabledMaps.bRoughness = OutResult.Textures.Roughness != nullptr;
		MaterialRequest.EnabledMaps.bMetallic = OutResult.Textures.Metallic != nullptr;
		MaterialRequest.EnabledMaps.bHeight = OutResult.Textures.Height != nullptr;
		MaterialRequest.EnabledMaps.bAO = OutResult.Textures.AO != nullptr;
		MaterialRequest.ExistingInstance = Request.ExistingInstance;
		MaterialRequest.bModifyExisting = Request.bModifyExisting;
		MaterialRequest.bSave = Request.bSave;
		return CreateMaterialInstance(
			OutResult.Textures,
			MaterialRequest,
			OutResult.MaterialInstance,
			OutError);
	}
}
