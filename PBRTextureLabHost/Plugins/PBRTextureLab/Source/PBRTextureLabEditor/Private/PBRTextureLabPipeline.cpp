#include "PBRTextureLabPipeline.h"
#include "PBRTextureLabPixelCore.h"

#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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

		if (!GeneratePBRMaps(OutResult.WorkingImage, Request.PixelParams, OutResult.Maps, &OutResult.Disclaimer))
		{
			PipelineSetError(OutError, TEXT("GeneratePBRMaps failed."));
			return EPBRImportStatus::Failed;
		}

		FPBRTextureImportRequest ImportRequest;
		ImportRequest.DestinationPath = Request.DestinationPath;
		ImportRequest.BaseName = Request.BaseName;
		ImportRequest.ConflictPolicy = Request.ConflictPolicy;
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
		MaterialRequest.DestinationPath = Request.DestinationPath;
		MaterialRequest.BaseName = Request.BaseName;
		MaterialRequest.ConflictPolicy = Request.ConflictPolicy;
		MaterialRequest.NormalStrength = Request.MaterialNormalStrength;
		MaterialRequest.HeightAmount = Request.MaterialHeightAmount;
		MaterialRequest.UVScale = Request.MaterialUVScale;
		MaterialRequest.bSave = Request.bSave;
		MaterialRequest.bCancelled = false;

		return CreateMaterialInstance(
			OutResult.Textures,
			MaterialRequest,
			OutResult.MaterialInstance,
			OutError);
	}
}
