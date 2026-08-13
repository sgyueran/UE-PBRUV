#include "PBRTextureLabTextureImport.h"
#include "PBRTextureLabCompat.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if WITH_AUTOMATION_WORKER

namespace
{
	constexpr EAutomationTestFlags ImportTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FString PersistImportBaseName()
	{
		return FString::Printf(TEXT("T3Reload%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
	}

	FString ConflictBaseName()
	{
		return FString::Printf(TEXT("T3Conflict%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
	}

	PBRTextureLab::FPBRImageRgba8 MakeImportSolid(const int32 Width, const int32 Height, const FColor Color)
	{
		PBRTextureLab::FPBRImageRgba8 Image;
		Image.Width = Width;
		Image.Height = Height;
		Image.Pixels.Init(Color, Width * Height);
		return Image;
	}

	bool GenerateMaps(FAutomationTestBase& Test, PBRTextureLab::FPBRMaps& OutMaps)
	{
		Test.AddExpectedMessagePlain(
			FString(PBRTextureLab::MetallicDisclaimer),
			ELogVerbosity::Warning,
			EAutomationExpectedMessageFlags::Contains,
			1);

		const PBRTextureLab::FPBRImageRgba8 Input = MakeImportSolid(16, 16, FColor(180, 90, 40, 255));
		PBRTextureLab::FPBRPixelParams Params;
		Params.HeightBlurRadius = 0;
		FString Disclaimer;
		const bool bOk = PBRTextureLab::GeneratePBRMaps(Input, Params, OutMaps, &Disclaimer);
		Test.TestTrue(TEXT("GeneratePBRMaps succeeds"), bOk);
		return bOk;
	}

	int32 CountStagingPngs()
	{
		TArray<FString> Found;
		IFileManager::Get().FindFilesRecursive(
			Found,
			*PBRTextureLab::GetStagingRootDirectory(),
			TEXT("*.png"),
			true,
			false);
		return Found.Num();
	}

	void ExpectTextureSettings(
		FAutomationTestBase& Test,
		UTexture2D* Texture,
		const TCHAR* Label,
		const TextureCompressionSettings Compression,
		const bool bSRGB)
	{
		Test.TestNotNull(Label, Texture);
		if (!Texture)
		{
			return;
		}
		Test.TestEqual(FString::Printf(TEXT("%s compression"), Label), Texture->CompressionSettings, Compression);
		Test.TestEqual(FString::Printf(TEXT("%s sRGB"), Label), static_cast<int32>(Texture->SRGB), bSRGB ? 1 : 0);
	}

	void ExpectAllSettings(FAutomationTestBase& Test, const PBRTextureLab::FPBRImportedTextures& Textures)
	{
		ExpectTextureSettings(Test, Textures.BaseColor, TEXT("BaseColor"), TC_Default, true);
		ExpectTextureSettings(Test, Textures.Height, TEXT("Height"), TC_Grayscale, false);
		ExpectTextureSettings(Test, Textures.Normal, TEXT("Normal"), TC_Normalmap, false);
		ExpectTextureSettings(Test, Textures.AO, TEXT("AO"), TC_Grayscale, false);
		ExpectTextureSettings(Test, Textures.Roughness, TEXT("Roughness"), TC_Grayscale, false);
		ExpectTextureSettings(Test, Textures.Metallic, TEXT("Metallic"), TC_Grayscale, false);
		ExpectTextureSettings(Test, Textures.ORM, TEXT("ORM"), TC_Masks, false);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabImportHappyPath,
	"PBRTextureLab.Import.HappyPath",
	ImportTestFlags)

bool FPBRTextureLabImportHappyPath::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	FPBRMaps Maps;
	if (!GenerateMaps(*this, Maps))
	{
		return false;
	}

	FPBRTextureImportRequest Request;
	Request.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	Request.BaseName = FString::Printf(TEXT("T3Happy%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
	Request.ConflictPolicy = EPBRImportConflictPolicy::UniqueName;
	Request.bSave = true;

	FPBRImportedTextures Imported;
	FString Error;
	const EPBRImportStatus Status = ImportPBRMaps(Maps, Request, Imported, &Error);
	TestEqual(TEXT("Happy path status"), static_cast<int32>(Status), static_cast<int32>(EPBRImportStatus::Success));
	TestTrue(TEXT("Happy path imported all maps"), Imported.HasAll());
	TestEqual(TEXT("Happy path error empty"), Error, FString());
	ExpectAllSettings(*this, Imported);
	TestEqual(TEXT("Staging PNGs cleaned after success"), CountStagingPngs(), 0);

	if (Imported.BaseColor)
	{
		FString Filename;
		TestTrue(TEXT("BaseColor package exists on disk"),
			FPackageName::DoesPackageExist(Imported.BaseColor->GetOutermost()->GetName(), &Filename));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabImportCancelAndConflict,
	"PBRTextureLab.Import.CancelAndConflict",
	ImportTestFlags)

bool FPBRTextureLabImportCancelAndConflict::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	FPBRMaps Maps;
	if (!GenerateMaps(*this, Maps))
	{
		return false;
	}

	FPBRTextureImportRequest CancelledRequest;
	CancelledRequest.bCancelled = true;
	CancelledRequest.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	CancelledRequest.BaseName = TEXT("T3ShouldNotExist");
	FPBRImportedTextures CancelledOut;
	FString CancelError;
	const EPBRImportStatus CancelStatus = ImportPBRMaps(Maps, CancelledRequest, CancelledOut, &CancelError);
	TestEqual(TEXT("Explicit cancel status"), static_cast<int32>(CancelStatus), static_cast<int32>(EPBRImportStatus::Cancelled));
	TestTrue(TEXT("Explicit cancel creates no textures"), !CancelledOut.HasAll() && CancelledOut.BaseColor == nullptr);
	TestTrue(TEXT("Cancelled name is not on disk"),
		!FPackageName::DoesPackageExist(TEXT("/Game/PBRTextureLab/Automation/T3ShouldNotExist_BaseColor")));

	FPBRTextureImportRequest SeedRequest;
	SeedRequest.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	SeedRequest.BaseName = ConflictBaseName();
	SeedRequest.ConflictPolicy = EPBRImportConflictPolicy::Replace;
	SeedRequest.bSave = true;
	FPBRImportedTextures Seeded;
	TestEqual(TEXT("Seed import"), static_cast<int32>(ImportPBRMaps(Maps, SeedRequest, Seeded)), static_cast<int32>(EPBRImportStatus::Success));

	FPBRTextureImportRequest ConflictRequest = SeedRequest;
	ConflictRequest.ConflictPolicy = EPBRImportConflictPolicy::Cancel;
	FPBRImportedTextures ConflictOut;
	FString ConflictError;
	AddExpectedMessagePlain(
		TEXT("ImportPBRMaps name conflict (cancel)"),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);
	const EPBRImportStatus ConflictStatus = ImportPBRMaps(Maps, ConflictRequest, ConflictOut, &ConflictError);
	TestEqual(TEXT("Conflict cancel status"), static_cast<int32>(ConflictStatus), static_cast<int32>(EPBRImportStatus::NameConflict));
	TestTrue(TEXT("Conflict cancel creates no new pointers"), ConflictOut.BaseColor == nullptr);
	TestEqual(TEXT("Staging cleaned after conflict"), CountStagingPngs(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabImportFailureCleanup,
	"PBRTextureLab.Import.FailureCleanup",
	ImportTestFlags)

bool FPBRTextureLabImportFailureCleanup::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	FPBRMaps InvalidMaps;
	FPBRTextureImportRequest Request;
	Request.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	Request.BaseName = TEXT("T3Invalid");
	FPBRImportedTextures Out;
	FString Error;
	AddExpectedErrorPlain(
		TEXT("ImportPBRMaps rejected invalid pixel maps"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	const EPBRImportStatus Status = ImportPBRMaps(InvalidMaps, Request, Out, &Error);
	TestEqual(TEXT("Invalid maps status"), static_cast<int32>(Status), static_cast<int32>(EPBRImportStatus::Failed));
	TestTrue(TEXT("Invalid maps create no assets"), Out.BaseColor == nullptr);
	TestTrue(TEXT("Invalid maps leave no package"),
		!FPackageName::DoesPackageExist(TEXT("/Game/PBRTextureLab/Automation/T3Invalid_BaseColor")));
	TestEqual(TEXT("Staging cleaned after invalid maps"), CountStagingPngs(), 0);

	FPBRMaps Maps;
	if (!GenerateMaps(*this, Maps))
	{
		return false;
	}
	Request.DestinationPath = TEXT("/NotGame/PBRTextureLab");
	Request.BaseName = TEXT("T3BadPath");
	AddExpectedErrorPlain(
		TEXT("Destination path must be under /Game"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	const EPBRImportStatus PathStatus = ImportPBRMaps(Maps, Request, Out, &Error);
	TestEqual(TEXT("Bad path status"), static_cast<int32>(PathStatus), static_cast<int32>(EPBRImportStatus::Failed));
	TestEqual(TEXT("Staging cleaned after bad path"), CountStagingPngs(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabImportCreateAndSave,
	"PBRTextureLab.Import.CreateAndSave",
	ImportTestFlags)

bool FPBRTextureLabImportCreateAndSave::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	FPBRMaps Maps;
	if (!GenerateMaps(*this, Maps))
	{
		return false;
	}

	FPBRTextureImportRequest Request;
	Request.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	Request.BaseName = PersistImportBaseName();
	Request.ConflictPolicy = EPBRImportConflictPolicy::Replace;
	Request.bSave = true;

	FPBRImportedTextures Imported;
	FString Error;
	const EPBRImportStatus Status = ImportPBRMaps(Maps, Request, Imported, &Error);
	TestEqual(TEXT("CreateAndSave status"), static_cast<int32>(Status), static_cast<int32>(EPBRImportStatus::Success));
	TestTrue(TEXT("CreateAndSave imported all maps"), Imported.HasAll());
	ExpectAllSettings(*this, Imported);
	TestEqual(TEXT("Staging cleaned after CreateAndSave"), CountStagingPngs(), 0);

	const FString Base = PersistImportBaseName();
	const TCHAR* Suffixes[] = {
		TEXT("_BaseColor"), TEXT("_Height"), TEXT("_Normal"), TEXT("_AO"),
		TEXT("_Roughness"), TEXT("_Metallic"), TEXT("_ORM")
	};
	for (const TCHAR* Suffix : Suffixes)
	{
		const FString PackageName = TEXT("/Game/PBRTextureLab/Automation/") + Base + Suffix;
		TestTrue(FString::Printf(TEXT("%s exists on disk"), *PackageName), FPackageName::DoesPackageExist(PackageName));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabImportReloadAfterRestart,
	"PBRTextureLab.Import.ReloadAfterRestart",
	ImportTestFlags)

bool FPBRTextureLabImportReloadAfterRestart::RunTest(const FString& Parameters)
{
	struct FExpected
	{
		const TCHAR* Suffix;
		TextureCompressionSettings Compression;
		bool bSRGB;
	};

	const FString Base = PersistImportBaseName();
	const FExpected Expected[] = {
		{ TEXT("_BaseColor"), TC_Default, true },
		{ TEXT("_Height"), TC_Grayscale, false },
		{ TEXT("_Normal"), TC_Normalmap, false },
		{ TEXT("_AO"), TC_Grayscale, false },
		{ TEXT("_Roughness"), TC_Grayscale, false },
		{ TEXT("_Metallic"), TC_Grayscale, false },
		{ TEXT("_ORM"), TC_Masks, false }
	};

	for (const FExpected& Item : Expected)
	{
		const FString AssetName = Base + Item.Suffix;
		const FString ObjectPath = FString::Printf(TEXT("/Game/PBRTextureLab/Automation/%s.%s"), *AssetName, *AssetName);
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
		TestNotNull(FString::Printf(TEXT("Reload %s"), *ObjectPath), Texture);
		if (Texture)
		{
			TestEqual(FString::Printf(TEXT("%s compression after reload"), *ObjectPath), Texture->CompressionSettings, Item.Compression);
			TestEqual(FString::Printf(TEXT("%s sRGB after reload"), *ObjectPath), static_cast<int32>(Texture->SRGB), Item.bSRGB ? 1 : 0);
		}
	}
	return true;
}

#endif
