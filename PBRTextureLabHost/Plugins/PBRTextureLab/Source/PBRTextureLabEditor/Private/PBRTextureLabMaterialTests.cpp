#include "PBRTextureLabMaterial.h"
#include "PBRTextureLabCompat.h"
#include "PBRTextureLabPixelCore.h"
#include "PBRTextureLabTextureImport.h"
#include "Engine/Texture2D.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"

#if WITH_AUTOMATION_WORKER

namespace
{
	constexpr EAutomationTestFlags MaterialTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FString PersistBaseName()
	{
		return FString::Printf(TEXT("T4Inst%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
	}

	PBRTextureLab::FPBRImageRgba8 MakeMaterialSolid(const int32 Width, const int32 Height, const FColor Color)
	{
		PBRTextureLab::FPBRImageRgba8 Image;
		Image.Width = Width;
		Image.Height = Height;
		Image.Pixels.Init(Color, Width * Height);
		return Image;
	}

	bool GenerateAndImport(
		FAutomationTestBase& Test,
		PBRTextureLab::FPBRImportedTextures& OutTextures,
		const FString& BaseName)
	{
		Test.AddExpectedMessagePlain(
			FString(PBRTextureLab::MetallicDisclaimer),
			ELogVerbosity::Warning,
			EAutomationExpectedMessageFlags::Contains,
			1);

		PBRTextureLab::FPBRMaps Maps;
		const PBRTextureLab::FPBRImageRgba8 Input = MakeMaterialSolid(16, 16, FColor(180, 90, 40, 255));
		PBRTextureLab::FPBRPixelParams Params;
		Params.HeightBlurRadius = 0;
		if (!PBRTextureLab::GeneratePBRMaps(Input, Params, Maps))
		{
			Test.AddError(TEXT("GeneratePBRMaps failed"));
			return false;
		}

		PBRTextureLab::FPBRTextureImportRequest Request;
		Request.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
		Request.BaseName = BaseName;
		Request.ConflictPolicy = PBRTextureLab::EPBRImportConflictPolicy::Replace;
		Request.bSave = true;
		FString Error;
		const PBRTextureLab::EPBRImportStatus Status = PBRTextureLab::ImportPBRMaps(Maps, Request, OutTextures, &Error);
		Test.TestEqual(TEXT("ImportPBRMaps status"), static_cast<int32>(Status), static_cast<int32>(PBRTextureLab::EPBRImportStatus::Success));
		Test.TestTrue(TEXT("Imported all textures"), OutTextures.HasAll());
		return Status == PBRTextureLab::EPBRImportStatus::Success && OutTextures.HasAll();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabMaterialParentCompiles,
	"PBRTextureLab.Material.ParentCompiles",
	MaterialTestFlags)

bool FPBRTextureLabMaterialParentCompiles::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	FString Error;
	UMaterial* Parent = GetOrCreateParentMaterial(&Error);
	TestNotNull(TEXT("Parent material"), Parent);
	TestEqual(TEXT("Parent create error"), Error, FString());
	if (!Parent)
	{
		return false;
	}

	TestTrue(TEXT("Parent is Surface"), Parent->MaterialDomain == MD_Surface);
	TestTrue(TEXT("Parent is Opaque"), Parent->BlendMode == BLEND_Opaque);
	TestTrue(TEXT("Parent is DefaultLit Metallic/Roughness"), Parent->GetShadingModels().HasShadingModel(MSM_DefaultLit));

	TestNotNull(
		TEXT("Default BaseColorTexture"),
		UMaterialEditingLibrary::GetMaterialDefaultTextureParameterValue(Parent, PBRTEXTURELAB_PARAM_BaseColorTexture));
	TestNotNull(
		TEXT("Default NormalTexture"),
		UMaterialEditingLibrary::GetMaterialDefaultTextureParameterValue(Parent, PBRTEXTURELAB_PARAM_NormalTexture));
	TestNotNull(
		TEXT("Default ORMTexture"),
		UMaterialEditingLibrary::GetMaterialDefaultTextureParameterValue(Parent, PBRTEXTURELAB_PARAM_ORMTexture));
	TestNotNull(
		TEXT("Default HeightTexture"),
		UMaterialEditingLibrary::GetMaterialDefaultTextureParameterValue(Parent, PBRTEXTURELAB_PARAM_HeightTexture));
	TestEqual(
		TEXT("Default NormalStrength"),
		UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Parent, PBRTEXTURELAB_PARAM_NormalStrength),
		1.0f);
	TestEqual(
		TEXT("Default HeightAmount disables bump"),
		UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Parent, PBRTEXTURELAB_PARAM_HeightAmount),
		0.0f);
	TestEqual(
		TEXT("Default UVScale"),
		UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Parent, PBRTEXTURELAB_PARAM_UVScale),
		1.0f);

	TestTrue(TEXT("Parent package exists on disk"),
		FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(GetParentMaterialObjectPath())));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabMaterialCreateInstance,
	"PBRTextureLab.Material.CreateInstance",
	MaterialTestFlags)

bool FPBRTextureLabMaterialCreateInstance::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	FPBRImportedTextures Textures;
	if (!GenerateAndImport(*this, Textures, PersistBaseName() + TEXT("Maps")))
	{
		return false;
	}

	FPBRMaterialInstanceRequest Request;
	Request.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	Request.BaseName = PersistBaseName();
	Request.ConflictPolicy = EPBRImportConflictPolicy::Replace;
	Request.NormalStrength = 0.75f;
	Request.HeightAmount = 0.0f;
	Request.UVScale = 2.0f;
	Request.bSave = true;

	UMaterialInstanceConstant* Instance = nullptr;
	FString Error;
	const EPBRImportStatus Status = CreateMaterialInstance(Textures, Request, Instance, &Error);
	TestEqual(TEXT("Create instance status"), static_cast<int32>(Status), static_cast<int32>(EPBRImportStatus::Success));
	TestNotNull(TEXT("Material instance"), Instance);
	TestEqual(TEXT("Create instance error"), Error, FString());
	if (!Instance)
	{
		return false;
	}

	TestTrue(TEXT("Instance parent"), Instance->Parent == GetOrCreateParentMaterial());
	TestEqual(
		TEXT("Instance BaseColor"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_BaseColorTexture),
		static_cast<UTexture*>(Textures.BaseColor));
	TestEqual(
		TEXT("Instance Normal"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_NormalTexture),
		static_cast<UTexture*>(Textures.Normal));
	TestEqual(
		TEXT("Instance ORM"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_ORMTexture),
		static_cast<UTexture*>(Textures.ORM));
	TestEqual(
		TEXT("Instance Height"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_HeightTexture),
		static_cast<UTexture*>(Textures.Height));
	TestEqual(
		TEXT("Instance NormalStrength"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_NormalStrength),
		0.75f);
	TestEqual(
		TEXT("Instance HeightAmount"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_HeightAmount),
		0.0f);
	TestEqual(
		TEXT("Instance UVScale"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_UVScale),
		2.0f);

	TestTrue(TEXT("Instance package exists on disk"),
		FPackageName::DoesPackageExist(Instance->GetOutermost()->GetName()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabMaterialCancelAndConflict,
	"PBRTextureLab.Material.CancelAndConflict",
	MaterialTestFlags)

bool FPBRTextureLabMaterialCancelAndConflict::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	FPBRImportedTextures Textures;
	if (!GenerateAndImport(*this, Textures, FString::Printf(TEXT("T4Cancel%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR)))
	{
		return false;
	}

	FPBRMaterialInstanceRequest Cancelled;
	Cancelled.bCancelled = true;
	Cancelled.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	Cancelled.BaseName = TEXT("T4ShouldNotExist");
	UMaterialInstanceConstant* CancelledOut = nullptr;
	FString Error;
	TestEqual(
		TEXT("Explicit cancel"),
		static_cast<int32>(CreateMaterialInstance(Textures, Cancelled, CancelledOut, &Error)),
		static_cast<int32>(EPBRImportStatus::Cancelled));
	TestTrue(TEXT("Cancel creates no instance"), CancelledOut == nullptr);

	FPBRMaterialInstanceRequest Seed;
	Seed.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	Seed.BaseName = FString::Printf(TEXT("T4Conflict%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
	Seed.ConflictPolicy = EPBRImportConflictPolicy::Replace;
	UMaterialInstanceConstant* Seeded = nullptr;
	TestEqual(
		TEXT("Seed instance"),
		static_cast<int32>(CreateMaterialInstance(Textures, Seed, Seeded, &Error)),
		static_cast<int32>(EPBRImportStatus::Success));

	Seed.ConflictPolicy = EPBRImportConflictPolicy::Cancel;
	UMaterialInstanceConstant* ConflictOut = nullptr;
	AddExpectedMessagePlain(
		TEXT("CreateMaterialInstance name conflict (cancel)"),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);
	TestEqual(
		TEXT("Conflict cancel"),
		static_cast<int32>(CreateMaterialInstance(Textures, Seed, ConflictOut, &Error)),
		static_cast<int32>(EPBRImportStatus::NameConflict));
	TestTrue(TEXT("Conflict creates no new instance"), ConflictOut == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabMaterialReloadAfterRestart,
	"PBRTextureLab.Material.ReloadAfterRestart",
	MaterialTestFlags)

bool FPBRTextureLabMaterialReloadAfterRestart::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	const FString AssetName = PersistBaseName() + TEXT("_Inst");
	const FString ObjectPath = TEXT("/Game/PBRTextureLab/Automation/") + AssetName + TEXT(".") + AssetName;
	UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *ObjectPath);
	TestNotNull(TEXT("Reload material instance"), Instance);
	if (!Instance)
	{
		return false;
	}

	TestNotNull(TEXT("Reloaded parent"), Instance->Parent.Get());
	TestTrue(TEXT("Reloaded BaseColor still bound"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_BaseColorTexture) != nullptr);
	TestTrue(TEXT("Reloaded Normal still bound"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_NormalTexture) != nullptr);
	TestTrue(TEXT("Reloaded ORM still bound"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_ORMTexture) != nullptr);
	TestTrue(TEXT("Reloaded Height still bound"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_HeightTexture) != nullptr);
	TestEqual(
		TEXT("Reloaded NormalStrength"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_NormalStrength),
		0.75f);
	TestEqual(
		TEXT("Reloaded UVScale"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_UVScale),
		2.0f);
	return true;
}

#endif
