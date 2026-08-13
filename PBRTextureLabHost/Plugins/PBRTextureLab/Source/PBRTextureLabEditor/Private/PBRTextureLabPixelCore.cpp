#include "PBRTextureLabPixelCore.h"

DEFINE_LOG_CATEGORY(LogPBRTextureLab);

namespace PBRTextureLab
{
	const TCHAR* const MetallicDisclaimer =
		TEXT("Metallic is a heuristic mask (constant or luminance threshold), not a reliable physical inversion of metalness.");

	const TCHAR* const MetallicDisclaimerZh =
		TEXT("Metallic 仅为启发式蒙版（常量或亮度阈值），不是可靠的物理金属度反演。");

	namespace
	{
		constexpr float Rec709R = 0.2126f;
		constexpr float Rec709G = 0.7152f;
		constexpr float Rec709B = 0.0722f;
		constexpr int32 MaxBlurRadius = 64;

		float Rec709Luminance(const FLinearColor& Linear)
		{
			return Linear.R * Rec709R + Linear.G * Rec709G + Linear.B * Rec709B;
		}

		uint8 Quantize01(const float Value)
		{
			return static_cast<uint8>(0.5f + FMath::Clamp(Value, 0.0f, 1.0f) * 255.0f);
		}

		FColor MakeGray(const float Value)
		{
			const uint8 Byte = Quantize01(Value);
			return FColor(Byte, Byte, Byte, 255);
		}

		void InitImage(FPBRImageRgba8& Image, const int32 Width, const int32 Height)
		{
			Image.Width = Width;
			Image.Height = Height;
			Image.Pixels.SetNumUninitialized(Width * Height);
		}

		float SampleClamp(const TArray<float>& Buffer, const int32 Width, const int32 Height, int32 X, int32 Y)
		{
			X = FMath::Clamp(X, 0, Width - 1);
			Y = FMath::Clamp(Y, 0, Height - 1);
			return Buffer[Y * Width + X];
		}

		void BoxBlur(TArray<float>& InOut, const int32 Width, const int32 Height, const int32 Radius)
		{
			if (Radius <= 0 || Width <= 0 || Height <= 0)
			{
				return;
			}

			TArray<float> Temp;
			Temp.SetNumUninitialized(Width * Height);

			TArray<float> Prefix;
			Prefix.SetNumUninitialized(FMath::Max(Width, Height) + 1);

			for (int32 Y = 0; Y < Height; ++Y)
			{
				Prefix[0] = 0.0f;
				for (int32 X = 0; X < Width; ++X)
				{
					Prefix[X + 1] = Prefix[X] + InOut[Y * Width + X];
				}
				for (int32 X = 0; X < Width; ++X)
				{
					const int32 Left = FMath::Max(X - Radius, 0);
					const int32 RightExclusive = FMath::Min(X + Radius + 1, Width);
					Temp[Y * Width + X] = (Prefix[RightExclusive] - Prefix[Left]) / static_cast<float>(RightExclusive - Left);
				}
			}

			for (int32 X = 0; X < Width; ++X)
			{
				Prefix[0] = 0.0f;
				for (int32 Y = 0; Y < Height; ++Y)
				{
					Prefix[Y + 1] = Prefix[Y] + Temp[Y * Width + X];
				}
				for (int32 Y = 0; Y < Height; ++Y)
				{
					const int32 Top = FMath::Max(Y - Radius, 0);
					const int32 BottomExclusive = FMath::Min(Y + Radius + 1, Height);
					InOut[Y * Width + X] = (Prefix[BottomExclusive] - Prefix[Top]) / static_cast<float>(BottomExclusive - Top);
				}
			}
		}

		void SobelGradients(
			const TArray<float>& Height,
			const int32 Width,
			const int32 HeightPixels,
			const int32 X,
			const int32 Y,
			float& OutDx,
			float& OutDy)
		{
			const float Tl = SampleClamp(Height, Width, HeightPixels, X - 1, Y - 1);
			const float Tc = SampleClamp(Height, Width, HeightPixels, X, Y - 1);
			const float Tr = SampleClamp(Height, Width, HeightPixels, X + 1, Y - 1);
			const float Ml = SampleClamp(Height, Width, HeightPixels, X - 1, Y);
			const float Mr = SampleClamp(Height, Width, HeightPixels, X + 1, Y);
			const float Bl = SampleClamp(Height, Width, HeightPixels, X - 1, Y + 1);
			const float Bc = SampleClamp(Height, Width, HeightPixels, X, Y + 1);
			const float Br = SampleClamp(Height, Width, HeightPixels, X + 1, Y + 1);

			OutDx = -Tl + Tr - 2.0f * Ml + 2.0f * Mr - Bl + Br;
			OutDy = -Tl - 2.0f * Tc - Tr + Bl + 2.0f * Bc + Br;
		}

		float MultiScaleAO(const TArray<float>& Height, const int32 Width, const int32 HeightPixels, const int32 X, const int32 Y)
		{
			static constexpr int32 Scales[] = {1, 2, 4};
			static constexpr float Weights[] = {0.50f, 0.35f, 0.15f};
			const float Center = SampleClamp(Height, Width, HeightPixels, X, Y);

			float Occlusion = 0.0f;
			for (int32 ScaleIndex = 0; ScaleIndex < UE_ARRAY_COUNT(Scales); ++ScaleIndex)
			{
				const int32 Scale = Scales[ScaleIndex];
				float Sum = 0.0f;
				int32 Count = 0;
				for (int32 OffsetY = -Scale; OffsetY <= Scale; ++OffsetY)
				{
					for (int32 OffsetX = -Scale; OffsetX <= Scale; ++OffsetX)
					{
						if (OffsetX == 0 && OffsetY == 0)
						{
							continue;
						}
						Sum += SampleClamp(Height, Width, HeightPixels, X + OffsetX, Y + OffsetY);
						++Count;
					}
				}
				const float NeighborhoodMean = Sum / static_cast<float>(Count);
				Occlusion += Weights[ScaleIndex] * FMath::Clamp(NeighborhoodMean - Center, 0.0f, 1.0f);
			}
			return FMath::Clamp(1.0f - Occlusion, 0.0f, 1.0f);
		}

		float LocalStdDev(const TArray<float>& Values, const int32 Width, const int32 Height, const int32 X, const int32 Y)
		{
			constexpr int32 Radius = 1;
			float Sum = 0.0f;
			int32 Count = 0;
			for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
			{
				for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
				{
					Sum += SampleClamp(Values, Width, Height, X + OffsetX, Y + OffsetY);
					++Count;
				}
			}
			const float Mean = Sum / static_cast<float>(Count);
			float Variance = 0.0f;
			for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
			{
				for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
				{
					const float Delta = SampleClamp(Values, Width, Height, X + OffsetX, Y + OffsetY) - Mean;
					Variance += Delta * Delta;
				}
			}
			Variance /= static_cast<float>(Count);
			return FMath::Sqrt(Variance);
		}
	}

	bool IsValidImage(const FPBRImageRgba8& Image)
	{
		return Image.Width > 0
			&& Image.Height > 0
			&& Image.Pixels.Num() == Image.Width * Image.Height;
	}

	bool GeneratePBRMaps(
		const FPBRImageRgba8& Input,
		const FPBRPixelParams& Params,
		FPBRMaps& OutMaps,
		FString* OutDisclaimer)
	{
		if (!IsValidImage(Input))
		{
			UE_LOG(LogPBRTextureLab, Error, TEXT("GeneratePBRMaps rejected invalid input (%dx%d, %d pixels)."),
				Input.Width, Input.Height, Input.Pixels.Num());
			return false;
		}

		UE_LOG(LogPBRTextureLab, Warning, TEXT("%s"), MetallicDisclaimer);
		if (OutDisclaimer)
		{
			*OutDisclaimer = MetallicDisclaimer;
		}

		const int32 Width = Input.Width;
		const int32 Height = Input.Height;
		const int32 PixelCount = Width * Height;
		const float Contrast = FMath::Max(Params.HeightContrast, 0.0f);
		const int32 BlurRadius = FMath::Clamp(Params.HeightBlurRadius, 0, MaxBlurRadius);
		const float NormalStrength = FMath::Max(Params.NormalStrength, 0.0f);
		const float RoughnessScale = Params.RoughnessScale;
		const float RoughnessBias = Params.RoughnessBias;
		const float MetallicConstant = FMath::Clamp(Params.MetallicConstant, 0.0f, 1.0f);
		const float MetallicThreshold = FMath::Clamp(Params.MetallicThreshold, 0.0f, 1.0f);

		TArray<float> Luminance;
		TArray<float> HeightValues;
		Luminance.SetNumUninitialized(PixelCount);
		HeightValues.SetNumUninitialized(PixelCount);

		for (int32 Index = 0; Index < PixelCount; ++Index)
		{
			const FLinearColor Linear(Input.Pixels[Index]);
			const float Luma = FMath::Clamp(Rec709Luminance(Linear), 0.0f, 1.0f);
			Luminance[Index] = Luma;
			float HeightValue = (Luma - 0.5f) * Contrast + 0.5f;
			HeightValue = FMath::Clamp(HeightValue, 0.0f, 1.0f);
			if (Params.bInvertHeight)
			{
				HeightValue = 1.0f - HeightValue;
			}
			HeightValues[Index] = HeightValue;
		}

		BoxBlur(HeightValues, Width, Height, BlurRadius);

		InitImage(OutMaps.BaseColor, Width, Height);
		InitImage(OutMaps.Height, Width, Height);
		InitImage(OutMaps.Normal, Width, Height);
		InitImage(OutMaps.AO, Width, Height);
		InitImage(OutMaps.Roughness, Width, Height);
		InitImage(OutMaps.Metallic, Width, Height);
		InitImage(OutMaps.ORM, Width, Height);

		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 Index = Y * Width + X;
				const float HeightValue = HeightValues[Index];
				const float Luma = Luminance[Index];

				float Dx = 0.0f;
				float Dy = 0.0f;
				SobelGradients(HeightValues, Width, Height, X, Y, Dx, Dy);

				// UE tangent-space / DirectX: +X = +U (right), +Y = +V, no green flip.
				// Image origin is top-left, so image +Y is +V. Increasing height along +X/+V
				// tilts the normal toward -X/-Y (R<128 / G<128 after 0.5+0.5 encode).
				const FVector3f NormalVec(-Dx * NormalStrength, -Dy * NormalStrength, 1.0f);
				const FVector3f Normal = NormalVec.GetSafeNormal();

				const float AO = MultiScaleAO(HeightValues, Width, Height, X, Y);
				const float Roughness = FMath::Clamp(
					LocalStdDev(Luminance, Width, Height, X, Y) * RoughnessScale + RoughnessBias,
					0.0f,
					1.0f);

				float Metallic = 0.0f;
				switch (Params.MetallicMode)
				{
				case EPBRMetallicMode::Constant:
					Metallic = MetallicConstant;
					break;
				case EPBRMetallicMode::ThresholdMask:
					Metallic = Luma >= MetallicThreshold ? 1.0f : 0.0f;
					break;
				default:
					Metallic = 0.0f;
					break;
				}

				const FColor Source = Input.Pixels[Index];
				OutMaps.BaseColor.Pixels[Index] = FColor(Source.R, Source.G, Source.B, Source.A);
				OutMaps.Height.Pixels[Index] = MakeGray(HeightValue);
				OutMaps.Normal.Pixels[Index] = FColor(
					Quantize01(Normal.X * 0.5f + 0.5f),
					Quantize01(Normal.Y * 0.5f + 0.5f),
					Quantize01(Normal.Z * 0.5f + 0.5f),
					255);
				OutMaps.AO.Pixels[Index] = MakeGray(AO);
				OutMaps.Roughness.Pixels[Index] = MakeGray(Roughness);
				OutMaps.Metallic.Pixels[Index] = MakeGray(Metallic);
				OutMaps.ORM.Pixels[Index] = FColor(
					OutMaps.AO.Pixels[Index].R,
					OutMaps.Roughness.Pixels[Index].R,
					OutMaps.Metallic.Pixels[Index].R,
					255);
			}
		}

		return true;
	}
}
