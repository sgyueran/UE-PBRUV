#include "PBRTextureLabPixelCore.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_WORKER

namespace
{
	constexpr EAutomationTestFlags PixelCoreTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	PBRTextureLab::FPBRImageRgba8 MakeSolid(const int32 Width, const int32 Height, const FColor Color)
	{
		PBRTextureLab::FPBRImageRgba8 Image;
		Image.Width = Width;
		Image.Height = Height;
		Image.Pixels.Init(Color, Width * Height);
		return Image;
	}

	PBRTextureLab::FPBRImageRgba8 MakeHorizontalGradient(const int32 Width, const int32 Height)
	{
		PBRTextureLab::FPBRImageRgba8 Image;
		Image.Width = Width;
		Image.Height = Height;
		Image.Pixels.SetNumUninitialized(Width * Height);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const uint8 Value = static_cast<uint8>(FMath::RoundToInt(255.0f * static_cast<float>(X) / static_cast<float>(Width - 1)));
				Image.Pixels[Y * Width + X] = FColor(Value, Value, Value, 255);
			}
		}
		return Image;
	}

	PBRTextureLab::FPBRImageRgba8 MakeVerticalGradient(const int32 Width, const int32 Height)
	{
		PBRTextureLab::FPBRImageRgba8 Image;
		Image.Width = Width;
		Image.Height = Height;
		Image.Pixels.SetNumUninitialized(Width * Height);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			const uint8 Value = static_cast<uint8>(FMath::RoundToInt(255.0f * static_cast<float>(Y) / static_cast<float>(Height - 1)));
			for (int32 X = 0; X < Width; ++X)
			{
				Image.Pixels[Y * Width + X] = FColor(Value, Value, Value, 255);
			}
		}
		return Image;
	}

	PBRTextureLab::FPBRImageRgba8 MakeCheckerboard(const int32 Width, const int32 Height, const int32 Tile)
	{
		PBRTextureLab::FPBRImageRgba8 Image;
		Image.Width = Width;
		Image.Height = Height;
		Image.Pixels.SetNumUninitialized(Width * Height);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const bool bWhite = ((X / Tile) + (Y / Tile)) % 2 == 0;
				Image.Pixels[Y * Width + X] = bWhite ? FColor::White : FColor::Black;
			}
		}
		return Image;
	}

	bool SameSize(const PBRTextureLab::FPBRImageRgba8& Image, const int32 Width, const int32 Height)
	{
		return Image.Width == Width && Image.Height == Height && Image.Pixels.Num() == Width * Height;
	}

	bool ImagesEqual(const PBRTextureLab::FPBRImageRgba8& A, const PBRTextureLab::FPBRImageRgba8& B)
	{
		return A.Width == B.Width && A.Height == B.Height && A.Pixels == B.Pixels;
	}

	bool GenerateChecked(
		FAutomationTestBase& Test,
		const PBRTextureLab::FPBRImageRgba8& Input,
		const PBRTextureLab::FPBRPixelParams& Params,
		PBRTextureLab::FPBRMaps& OutMaps,
		const int32 ExpectedDisclaimerCount = 1)
	{
		Test.AddExpectedMessagePlain(
			FString(PBRTextureLab::MetallicDisclaimer),
			ELogVerbosity::Warning,
			EAutomationExpectedMessageFlags::Contains,
			ExpectedDisclaimerCount);

		FString Disclaimer;
		const bool bOk = PBRTextureLab::GeneratePBRMaps(Input, Params, OutMaps, &Disclaimer);
		Test.TestTrue(TEXT("GeneratePBRMaps succeeds"), bOk);
		Test.TestEqual(TEXT("Disclaimer string"), Disclaimer, FString(PBRTextureLab::MetallicDisclaimer));
		return bOk;
	}

	void ExpectMatchingDimensions(FAutomationTestBase& Test, const PBRTextureLab::FPBRMaps& Maps, const int32 Width, const int32 Height)
	{
		Test.TestTrue(TEXT("BaseColor size"), SameSize(Maps.BaseColor, Width, Height));
		Test.TestTrue(TEXT("Height size"), SameSize(Maps.Height, Width, Height));
		Test.TestTrue(TEXT("Normal size"), SameSize(Maps.Normal, Width, Height));
		Test.TestTrue(TEXT("AO size"), SameSize(Maps.AO, Width, Height));
		Test.TestTrue(TEXT("Roughness size"), SameSize(Maps.Roughness, Width, Height));
		Test.TestTrue(TEXT("Metallic size"), SameSize(Maps.Metallic, Width, Height));
		Test.TestTrue(TEXT("ORM size"), SameSize(Maps.ORM, Width, Height));
	}

	void ExpectOrmPacking(FAutomationTestBase& Test, const PBRTextureLab::FPBRMaps& Maps)
	{
		const int32 Count = Maps.ORM.Pixels.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FColor& Orm = Maps.ORM.Pixels[Index];
			if (Orm.R != Maps.AO.Pixels[Index].R
				|| Orm.G != Maps.Roughness.Pixels[Index].R
				|| Orm.B != Maps.Metallic.Pixels[Index].R
				|| Orm.A != 255)
			{
				Test.AddError(FString::Printf(TEXT("ORM packing mismatch at %d: ORM=(%d,%d,%d,%d)"),
					Index, Orm.R, Orm.G, Orm.B, Orm.A));
				return;
			}
		}
		Test.TestTrue(TEXT("ORM is R=AO G=Roughness B=Metallic"), true);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabPixelCoreSolidColor,
	"PBRTextureLab.PixelCore.SolidColor",
	PixelCoreTestFlags)

bool FPBRTextureLabPixelCoreSolidColor::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	const FPBRImageRgba8 Input = MakeSolid(16, 16, FColor(128, 64, 32, 200));
	FPBRPixelParams Params;
	Params.HeightBlurRadius = 1;
	FPBRMaps Maps;
	if (!GenerateChecked(*this, Input, Params, Maps))
	{
		return false;
	}

	ExpectMatchingDimensions(*this, Maps, 16, 16);
	ExpectOrmPacking(*this, Maps);

	const FColor MidHeight = Maps.Height.Pixels[8 * 16 + 8];
	const FColor MidNormal = Maps.Normal.Pixels[8 * 16 + 8];
	const FColor MidAO = Maps.AO.Pixels[8 * 16 + 8];
	const FColor MidRough = Maps.Roughness.Pixels[8 * 16 + 8];
	const FColor MidMetal = Maps.Metallic.Pixels[8 * 16 + 8];

	TestEqual(TEXT("BaseColor preserves sRGB bytes"), Maps.BaseColor.Pixels[0], Input.Pixels[0]);
	TestTrue(TEXT("Solid height is uniform"), Maps.Height.Pixels[0] == MidHeight);
	TestTrue(TEXT("Flat normal R near 128"), FMath::Abs(int32(MidNormal.R) - 128) <= 1);
	TestTrue(TEXT("Flat normal G near 128"), FMath::Abs(int32(MidNormal.G) - 128) <= 1);
	TestTrue(TEXT("Flat normal B is outward hemisphere"), MidNormal.B >= 250);
	TestTrue(TEXT("Solid AO is unoccluded"), MidAO.R >= 250);
	TestEqual(TEXT("Solid roughness is bias 0"), int32(MidRough.R), 0);
	TestEqual(TEXT("Default metallic is black"), int32(MidMetal.R), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabPixelCoreHorizontalGradient,
	"PBRTextureLab.PixelCore.HorizontalGradient",
	PixelCoreTestFlags)

bool FPBRTextureLabPixelCoreHorizontalGradient::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	const FPBRImageRgba8 Input = MakeHorizontalGradient(32, 32);
	FPBRPixelParams Params;
	Params.HeightBlurRadius = 0;
	FPBRMaps Maps;
	if (!GenerateChecked(*this, Input, Params, Maps))
	{
		return false;
	}

	ExpectMatchingDimensions(*this, Maps, 32, 32);
	ExpectOrmPacking(*this, Maps);

	const FColor Left = Maps.Height.Pixels[16 * 32 + 2];
	const FColor Right = Maps.Height.Pixels[16 * 32 + 29];
	const FColor MidNormal = Maps.Normal.Pixels[16 * 32 + 16];

	TestTrue(TEXT("Horizontal height increases to the right"), Right.R > Left.R);
	TestTrue(TEXT("UE normal: Nx < 0 so encoded R < 128"), MidNormal.R < 128);
	TestTrue(TEXT("Horizontal gradient: Ny ~ 0 so G near 128"), FMath::Abs(int32(MidNormal.G) - 128) <= 2);
	TestTrue(TEXT("Normal B is outward hemisphere"), MidNormal.B >= 128);

	Params.bInvertHeight = true;
	FPBRMaps Inverted;
	if (!GenerateChecked(*this, Input, Params, Inverted))
	{
		return false;
	}
	const FColor InvertedNormal = Inverted.Normal.Pixels[16 * 32 + 16];
	TestTrue(TEXT("Inverted height flips normal X across 128"), InvertedNormal.R > 128);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabPixelCoreVerticalGradient,
	"PBRTextureLab.PixelCore.VerticalGradient",
	PixelCoreTestFlags)

bool FPBRTextureLabPixelCoreVerticalGradient::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	const FPBRImageRgba8 Input = MakeVerticalGradient(32, 32);
	FPBRPixelParams Params;
	Params.HeightBlurRadius = 0;
	FPBRMaps Maps;
	if (!GenerateChecked(*this, Input, Params, Maps))
	{
		return false;
	}

	ExpectMatchingDimensions(*this, Maps, 32, 32);

	const FColor Top = Maps.Height.Pixels[2 * 32 + 16];
	const FColor Bottom = Maps.Height.Pixels[29 * 32 + 16];
	const FColor MidNormal = Maps.Normal.Pixels[16 * 32 + 16];

	TestTrue(TEXT("Vertical height increases downward"), Bottom.R > Top.R);
	TestTrue(TEXT("UE DirectX, no green flip: Ny < 0 so encoded G < 128"), MidNormal.G < 128);
	TestTrue(TEXT("Vertical gradient: Nx ~ 0 so R near 128"), FMath::Abs(int32(MidNormal.R) - 128) <= 2);
	TestTrue(TEXT("Normal B is outward hemisphere"), MidNormal.B >= 128);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabPixelCoreCheckerboard,
	"PBRTextureLab.PixelCore.Checkerboard",
	PixelCoreTestFlags)

bool FPBRTextureLabPixelCoreCheckerboard::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	const FPBRImageRgba8 Input = MakeCheckerboard(32, 32, 4);
	FPBRPixelParams Params;
	Params.HeightBlurRadius = 0;
	FPBRMaps Maps;
	if (!GenerateChecked(*this, Input, Params, Maps))
	{
		return false;
	}

	ExpectMatchingDimensions(*this, Maps, 32, 32);
	ExpectOrmPacking(*this, Maps);

	uint8 MaxRough = 0;
	uint8 MinAO = 255;
	for (int32 Index = 0; Index < Maps.Roughness.Pixels.Num(); ++Index)
	{
		MaxRough = FMath::Max(MaxRough, Maps.Roughness.Pixels[Index].R);
		MinAO = FMath::Min(MinAO, Maps.AO.Pixels[Index].R);
		TestTrue(TEXT("Normal B stays in outward hemisphere"), Maps.Normal.Pixels[Index].B >= 128);
	}

	TestTrue(TEXT("Checkerboard produces high roughness somewhere"), MaxRough > 80);
	TestTrue(TEXT("Checkerboard produces some AO darkening"), MinAO < 250);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabPixelCoreDeterminismAndMetallic,
	"PBRTextureLab.PixelCore.DeterminismAndMetallic",
	PixelCoreTestFlags)

bool FPBRTextureLabPixelCoreDeterminismAndMetallic::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	const FPBRImageRgba8 Input = MakeHorizontalGradient(24, 16);
	FPBRPixelParams Params;
	Params.HeightContrast = 1.25f;
	Params.HeightBlurRadius = 2;
	Params.NormalStrength = 1.5f;
	Params.RoughnessBias = 0.1f;
	Params.MetallicMode = EPBRMetallicMode::ThresholdMask;
	Params.MetallicThreshold = 0.5f;

	FPBRMaps First;
	FPBRMaps Second;
	if (!GenerateChecked(*this, Input, Params, First) || !GenerateChecked(*this, Input, Params, Second))
	{
		return false;
	}

	TestTrue(TEXT("BaseColor deterministic"), ImagesEqual(First.BaseColor, Second.BaseColor));
	TestTrue(TEXT("Height deterministic"), ImagesEqual(First.Height, Second.Height));
	TestTrue(TEXT("Normal deterministic"), ImagesEqual(First.Normal, Second.Normal));
	TestTrue(TEXT("AO deterministic"), ImagesEqual(First.AO, Second.AO));
	TestTrue(TEXT("Roughness deterministic"), ImagesEqual(First.Roughness, Second.Roughness));
	TestTrue(TEXT("Metallic deterministic"), ImagesEqual(First.Metallic, Second.Metallic));
	TestTrue(TEXT("ORM deterministic"), ImagesEqual(First.ORM, Second.ORM));
	ExpectOrmPacking(*this, First);

	const FColor LeftMetal = First.Metallic.Pixels[8 * 24 + 2];
	const FColor RightMetal = First.Metallic.Pixels[8 * 24 + 21];
	TestEqual(TEXT("Threshold metallic is 0 on dark side"), int32(LeftMetal.R), 0);
	TestEqual(TEXT("Threshold metallic is 255 on bright side"), int32(RightMetal.R), 255);

	Params.MetallicMode = EPBRMetallicMode::Constant;
	Params.MetallicConstant = 0.25f;
	FPBRMaps ConstantMaps;
	if (!GenerateChecked(*this, Input, Params, ConstantMaps))
	{
		return false;
	}
	TestEqual(TEXT("Constant metallic is quantized 0.25"), int32(ConstantMaps.Metallic.Pixels[0].R), 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabPixelCoreInvalidInput,
	"PBRTextureLab.PixelCore.InvalidInput",
	PixelCoreTestFlags)

bool FPBRTextureLabPixelCoreInvalidInput::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	FPBRImageRgba8 Input;
	Input.Width = 4;
	Input.Height = 4;
	FPBRMaps Maps;
	AddExpectedErrorPlain(
		TEXT("GeneratePBRMaps rejected invalid input"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestTrue(TEXT("Mismatched pixel count is invalid"), !IsValidImage(Input));
	TestTrue(TEXT("Generate rejects invalid input"), !GeneratePBRMaps(Input, FPBRPixelParams(), Maps));
	return true;
}

#endif
