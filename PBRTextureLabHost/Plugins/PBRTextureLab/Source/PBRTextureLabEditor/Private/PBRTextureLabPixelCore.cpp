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

		int32 WrapIndex(int32 Value, const int32 Size)
		{
			if (Size <= 0)
			{
				return 0;
			}
			Value %= Size;
			if (Value < 0)
			{
				Value += Size;
			}
			return Value;
		}

		float SampleWrap(const TArray<float>& Buffer, const int32 Width, const int32 Height, const int32 X, const int32 Y)
		{
			return Buffer[WrapIndex(Y, Height) * Width + WrapIndex(X, Width)];
		}

		void BoxBlur(TArray<float>& InOut, const int32 Width, const int32 Height, const int32 Radius)
		{
			if (Radius <= 0 || Width <= 0 || Height <= 0)
			{
				return;
			}

			TArray<float> Temp;
			Temp.SetNumUninitialized(Width * Height);
			const float Window = static_cast<float>(Radius * 2 + 1);

			for (int32 Y = 0; Y < Height; ++Y)
			{
				for (int32 X = 0; X < Width; ++X)
				{
					float Sum = 0.0f;
					for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
					{
						Sum += InOut[Y * Width + WrapIndex(X + Offset, Width)];
					}
					Temp[Y * Width + X] = Sum / Window;
				}
			}

			for (int32 X = 0; X < Width; ++X)
			{
				for (int32 Y = 0; Y < Height; ++Y)
				{
					float Sum = 0.0f;
					for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
					{
						Sum += Temp[WrapIndex(Y + Offset, Height) * Width + X];
					}
					InOut[Y * Width + X] = Sum / Window;
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
			const float Tl = SampleWrap(Height, Width, HeightPixels, X - 1, Y - 1);
			const float Tc = SampleWrap(Height, Width, HeightPixels, X, Y - 1);
			const float Tr = SampleWrap(Height, Width, HeightPixels, X + 1, Y - 1);
			const float Ml = SampleWrap(Height, Width, HeightPixels, X - 1, Y);
			const float Mr = SampleWrap(Height, Width, HeightPixels, X + 1, Y);
			const float Bl = SampleWrap(Height, Width, HeightPixels, X - 1, Y + 1);
			const float Bc = SampleWrap(Height, Width, HeightPixels, X, Y + 1);
			const float Br = SampleWrap(Height, Width, HeightPixels, X + 1, Y + 1);

			OutDx = -Tl + Tr - 2.0f * Ml + 2.0f * Mr - Bl + Br;
			OutDy = -Tl - 2.0f * Tc - Tr + Bl + 2.0f * Bc + Br;
		}

		float MultiScaleAO(const TArray<float>& Height, const int32 Width, const int32 HeightPixels, const int32 X, const int32 Y)
		{
			static constexpr int32 Scales[] = {1, 2, 4};
			static constexpr float Weights[] = {0.50f, 0.35f, 0.15f};
			const float Center = SampleWrap(Height, Width, HeightPixels, X, Y);

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
						Sum += SampleWrap(Height, Width, HeightPixels, X + OffsetX, Y + OffsetY);
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
					Sum += SampleWrap(Values, Width, Height, X + OffsetX, Y + OffsetY);
					++Count;
				}
			}
			const float Mean = Sum / static_cast<float>(Count);
			float Variance = 0.0f;
			for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
			{
				for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
				{
					const float Delta = SampleWrap(Values, Width, Height, X + OffsetX, Y + OffsetY) - Mean;
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

	bool MakeSeamlessImage(FPBRImageRgba8& Image, const bool bIsNormalMap)
	{
		if (!IsValidImage(Image))
		{
			return false;
		}
		if (Image.Width < 2 || Image.Height < 2)
		{
			return true;
		}

		const int32 Width = Image.Width;
		const int32 Height = Image.Height;
		const int32 BlendX = FMath::Clamp(Width / 8, 1, Width / 2);
		const int32 BlendY = FMath::Clamp(Height / 8, 1, Height / 2);

		auto ToLinear = [](const FColor& Color)
		{
			return FLinearColor(Color.R / 255.0f, Color.G / 255.0f, Color.B / 255.0f, Color.A / 255.0f);
		};
		auto ToColor = [bIsNormalMap](FLinearColor Value)
		{
			if (bIsNormalMap)
			{
				FVector3f Normal(Value.R * 2.0f - 1.0f, Value.G * 2.0f - 1.0f, Value.B * 2.0f - 1.0f);
				Normal = Normal.GetSafeNormal();
				Value.R = Normal.X * 0.5f + 0.5f;
				Value.G = Normal.Y * 0.5f + 0.5f;
				Value.B = Normal.Z * 0.5f + 0.5f;
			}
			return FColor(
				Quantize01(Value.R),
				Quantize01(Value.G),
				Quantize01(Value.B),
				Quantize01(Value.A));
		};
		auto BlendAmount = [](const int32 Distance, const int32 Blend)
		{
			const float T = 0.5f * (1.0f - FMath::Cos(PI * static_cast<float>(Distance) / static_cast<float>(Blend)));
			return 0.5f * (1.0f - T);
		};

		TArray<FColor> Source = Image.Pixels;
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < BlendX; ++X)
			{
				const float Amount = BlendAmount(X, BlendX);
				const int32 LeftIndex = Y * Width + X;
				const int32 RightIndex = Y * Width + (Width - 1 - X);
				const FLinearColor Left = ToLinear(Source[LeftIndex]);
				const FLinearColor Right = ToLinear(Source[RightIndex]);
				Image.Pixels[LeftIndex] = ToColor(FMath::Lerp(Left, Right, Amount));
				Image.Pixels[RightIndex] = ToColor(FMath::Lerp(Right, Left, Amount));
			}
		}

		Source = Image.Pixels;
		for (int32 Y = 0; Y < BlendY; ++Y)
		{
			const float Amount = BlendAmount(Y, BlendY);
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 TopIndex = Y * Width + X;
				const int32 BottomIndex = (Height - 1 - Y) * Width + X;
				const FLinearColor Top = ToLinear(Source[TopIndex]);
				const FLinearColor Bottom = ToLinear(Source[BottomIndex]);
				Image.Pixels[TopIndex] = ToColor(FMath::Lerp(Top, Bottom, Amount));
				Image.Pixels[BottomIndex] = ToColor(FMath::Lerp(Bottom, Top, Amount));
			}
		}

		return true;
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
