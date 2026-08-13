#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPBRTextureLab, Log, All);

/**
 * Offline Metallic/Roughness pixel core.
 * No UObject, no asset I/O, no network. Safe to call from a worker thread.
 */
namespace PBRTextureLab
{
	/** English disclaimer required by Task 2. Metallic is not a physical inversion. */
	extern const TCHAR* const MetallicDisclaimer;

	/** Chinese disclaimer for later UI (Task 6/7). */
	extern const TCHAR* const MetallicDisclaimerZh;

	enum class EPBRMetallicMode : uint8
	{
		AllBlack = 0,
		Constant = 1,
		ThresholdMask = 2
	};

	struct FPBRImageRgba8
	{
		int32 Width = 0;
		int32 Height = 0;
		TArray<FColor> Pixels;
	};

	struct FPBRPixelParams
	{
		float HeightContrast = 1.0f;
		int32 HeightBlurRadius = 1;
		bool bInvertHeight = false;
		float NormalStrength = 1.0f;
		float RoughnessScale = 2.0f;
		float RoughnessBias = 0.0f;
		EPBRMetallicMode MetallicMode = EPBRMetallicMode::AllBlack;
		float MetallicConstant = 0.0f;
		float MetallicThreshold = 0.5f;
	};

	struct FPBRMaps
	{
		FPBRImageRgba8 BaseColor;
		FPBRImageRgba8 Height;
		FPBRImageRgba8 Normal;
		FPBRImageRgba8 AO;
		FPBRImageRgba8 Roughness;
		FPBRImageRgba8 Metallic;
		FPBRImageRgba8 ORM;
	};

	bool IsValidImage(const FPBRImageRgba8& Image);

	/**
	 * Generate BaseColor, Height, Normal, AO, Roughness, Metallic, ORM.
	 * Fixed inputs + params produce bit-identical outputs.
	 * On success, OutDisclaimer receives MetallicDisclaimer.
	 */
	bool GeneratePBRMaps(
		const FPBRImageRgba8& Input,
		const FPBRPixelParams& Params,
		FPBRMaps& OutMaps,
		FString* OutDisclaimer = nullptr);
}
