#include "PBRTextureLabMaterial.h"
#include "PBRTextureLabCompat.h"
#include "PBRTextureLabEditorApi.h"
#include "PBRTextureLabPixelCore.h"
#include "PBRTextureLabTextureImport.h"
#include "Engine/Texture2D.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"

#if WITH_AUTOMATION_WORKER

namespace
{
	constexpr EAutomationTestFlags MaterialTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FString PersistMaterialBaseName()
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
		TEXT("Default RoughnessTexture"),
		UMaterialEditingLibrary::GetMaterialDefaultTextureParameterValue(Parent, PBRTEXTURELAB_PARAM_RoughnessTexture));
	TestNotNull(
		TEXT("Default MetallicTexture"),
		UMaterialEditingLibrary::GetMaterialDefaultTextureParameterValue(Parent, PBRTEXTURELAB_PARAM_MetallicTexture));
	TestNotNull(
		TEXT("Default AOTexture"),
		UMaterialEditingLibrary::GetMaterialDefaultTextureParameterValue(Parent, PBRTEXTURELAB_PARAM_AOTexture));
	TestNotNull(
		TEXT("Default HeightTexture"),
		UMaterialEditingLibrary::GetMaterialDefaultTextureParameterValue(Parent, PBRTEXTURELAB_PARAM_HeightTexture));
	TestEqual(
		TEXT("Default NormalStrength"),
		UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Parent, PBRTEXTURELAB_PARAM_NormalStrength),
		0.1f);
	TestEqual(
		TEXT("Default HeightAmount disables bump"),
		UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Parent, PBRTEXTURELAB_PARAM_HeightAmount),
		0.0f);
	TestEqual(
		TEXT("Default UVScale"),
		UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Parent, PBRTEXTURELAB_PARAM_UVScale),
		1.0f);
	TestEqual(
		TEXT("Default 基础色"),
		UMaterialEditingLibrary::GetMaterialDefaultVectorParameterValue(Parent, PBRTEXTURELAB_PARAM_BaseColor),
		FLinearColor::White);
	TestEqual(
		TEXT("Default 粗糙度"),
		UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Parent, PBRTEXTURELAB_PARAM_Roughness),
		1.0f);
	TestEqual(
		TEXT("Default 高光度"),
		UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Parent, PBRTEXTURELAB_PARAM_Specular),
		0.5f);
	TestEqual(
		TEXT("Default 金属度"),
		UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Parent, PBRTEXTURELAB_PARAM_Metallic),
		1.0f);

	TestTrue(TEXT("Parent package exists on disk"),
		FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(GetParentMaterialObjectPath())));

	auto ExpectSampler = [this](UMaterial* Material, const TCHAR* ParamName, EMaterialSamplerType Expected)
	{
		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			UMaterialExpressionTextureSampleParameter2D* Sample =
				Cast<UMaterialExpressionTextureSampleParameter2D>(Expression);
			if (!Sample || Sample->ParameterName != FName(ParamName))
			{
				continue;
			}
			UTexture* DefaultTexture = Sample->Texture;
			TestTrue(FString::Printf(TEXT("%s has a default texture"), ParamName), DefaultTexture != nullptr);
			if (DefaultTexture)
			{
				TestEqual(
					FString::Printf(TEXT("%s sampler type"), ParamName),
					static_cast<int32>(Sample->SamplerType),
					static_cast<int32>(Expected));
				TestEqual(
					FString::Printf(TEXT("%s default texture sampler"), ParamName),
					static_cast<int32>(PBRTextureLab::GetSamplerTypeForTexture(DefaultTexture)),
					static_cast<int32>(Expected));
			}
			return;
		}
		AddError(FString::Printf(TEXT("Missing texture parameter %s"), ParamName));
	};
	ExpectSampler(Parent, PBRTEXTURELAB_PARAM_BaseColorTexture, SAMPLERTYPE_Color);
	ExpectSampler(Parent, PBRTEXTURELAB_PARAM_NormalTexture, SAMPLERTYPE_Normal);
	ExpectSampler(Parent, PBRTEXTURELAB_PARAM_RoughnessTexture, SAMPLERTYPE_LinearGrayscale);
	ExpectSampler(Parent, PBRTEXTURELAB_PARAM_MetallicTexture, SAMPLERTYPE_LinearGrayscale);
	ExpectSampler(Parent, PBRTEXTURELAB_PARAM_AOTexture, SAMPLERTYPE_LinearGrayscale);
	ExpectSampler(Parent, PBRTEXTURELAB_PARAM_HeightTexture, SAMPLERTYPE_LinearGrayscale);
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
	if (!GenerateAndImport(*this, Textures, PersistMaterialBaseName() + TEXT("Maps")))
	{
		return false;
	}

	FPBRMaterialInstanceRequest Request;
	Request.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	Request.BaseName = PersistMaterialBaseName();
	Request.ParentMaterial = GetOrCreateParentMaterial();
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
		TEXT("Instance Roughness"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_RoughnessTexture),
		static_cast<UTexture*>(Textures.Roughness));
	TestEqual(
		TEXT("Instance Metallic"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_MetallicTexture),
		static_cast<UTexture*>(Textures.Metallic));
	TestEqual(
		TEXT("Instance AO"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_AOTexture),
		static_cast<UTexture*>(Textures.AO));
	TestEqual(
		TEXT("Instance Height"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_HeightTexture),
		static_cast<UTexture*>(Textures.Height));
	TestEqual(
		TEXT("Instance NormalStrength"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_NormalStrength),
		0.75f);
	TestEqual(
		TEXT("Inherited 粗糙度 stays editable"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_Roughness),
		1.0f);
	TestEqual(
		TEXT("Inherited 高光度 stays editable"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_Specular),
		0.5f);
	UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_Roughness, 0.35f);
	UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_Specular, 0.8f);
	UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, PBRTEXTURELAB_PARAM_BaseColor, FLinearColor(0.2f, 0.4f, 0.6f));
	TestEqual(
		TEXT("Child can set 粗糙度"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_Roughness),
		0.35f);
	TestEqual(
		TEXT("Child can set 高光度"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_Specular),
		0.8f);
	TestEqual(
		TEXT("Child can set 基础色"),
		UMaterialEditingLibrary::GetMaterialInstanceVectorParameterValue(Instance, PBRTEXTURELAB_PARAM_BaseColor),
		FLinearColor(0.2f, 0.4f, 0.6f));
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
	FPBRTextureLabMaterialReuseUserParent,
	"PBRTextureLab.Material.ReuseUserParent",
	MaterialTestFlags)

bool FPBRTextureLabMaterialReuseUserParent::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	FPBRImportedTextures Textures;
	if (!GenerateAndImport(*this, Textures, FString::Printf(TEXT("T4Reuse%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR)))
	{
		return false;
	}

	UMaterial* UserParent = NewObject<UMaterial>(GetTransientPackage(), TEXT("T4UserParent"));
	TestNotNull(TEXT("User-style parent"), UserParent);
	if (!UserParent)
	{
		return false;
	}

	auto AddTexture = [UserParent](const TCHAR* Name, const int32 Y)
	{
		UMaterialExpressionTextureSampleParameter2D* Sample = Cast<UMaterialExpressionTextureSampleParameter2D>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				UserParent,
				UMaterialExpressionTextureSampleParameter2D::StaticClass(),
				-200,
				Y));
		if (Sample)
		{
			Sample->ParameterName = Name;
		}
	};
	auto AddScalar = [UserParent](const TCHAR* Name, const int32 Y)
	{
		UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				UserParent,
				UMaterialExpressionScalarParameter::StaticClass(),
				0,
				Y));
		if (Scalar)
		{
			Scalar->ParameterName = Name;
			Scalar->DefaultValue = 0.0f;
		}
	};
	AddTexture(TEXT("基础贴图"), 0);
	AddTexture(TEXT("法线贴图"), 80);
	AddTexture(TEXT("粗糙贴图"), 160);
	AddTexture(TEXT("金属贴图"), 240);
	AddTexture(TEXT("置换贴图"), 320);

	const FPBRParentParamTitles Titles = InspectParentParamTitles(UserParent);
	TestEqual(TEXT("Title 基础贴图"), Titles.BaseColorTexture, FString(TEXT("基础贴图")));
	TestEqual(TEXT("Title 法线贴图"), Titles.NormalTexture, FString(TEXT("法线贴图")));
	TestEqual(TEXT("Title 粗糙贴图"), Titles.RoughnessTexture, FString(TEXT("粗糙贴图")));
	TestEqual(TEXT("Title 金属贴图"), Titles.MetallicTexture, FString(TEXT("金属贴图")));
	TestEqual(TEXT("Title 置换贴图"), Titles.HeightTexture, FString(TEXT("置换贴图")));
	TestEqual(TEXT("Title 法线强度"), Titles.NormalStrength, FString(TEXT("法线强度")));
	TestEqual(TEXT("Title 粗糙强度"), Titles.RoughnessStrength, FString(TEXT("粗糙强度")));
	TestEqual(TEXT("Title UV缩放"), Titles.UVScale, FString(TEXT("UV缩放")));
	AddScalar(TEXT("基础贴图开关"), 0);
	AddScalar(TEXT("粗糙贴图开关"), 80);
	AddScalar(TEXT("金属贴图开关"), 160);
	AddScalar(TEXT("置换贴图开关"), 240);
	AddScalar(TEXT("法线强度"), 320);
	AddScalar(TEXT("粗糙强度"), 400);
	AddScalar(TEXT("金属强度"), 480);
	AddScalar(TEXT("置换强度"), 560);
	AddScalar(TEXT("UV缩放"), 640);
	AddScalar(TEXT("UV缩放3"), 720);

	FPBRMaterialInstanceRequest Request;
	Request.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	Request.BaseName = FString::Printf(TEXT("T4ReuseInst%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
	Request.ParentMaterial = UserParent;
	Request.ConflictPolicy = EPBRImportConflictPolicy::Replace;
	Request.NormalStrength = 0.8f;
	Request.RoughnessStrength = 1.5f;
	Request.MetallicStrength = 0.4f;
	Request.HeightAmount = 0.2f;
	Request.UVScale = 4.0f;
	Request.bSave = true;

	UMaterialInstanceConstant* Instance = nullptr;
	FString Error;
	TestEqual(
		TEXT("Reuse create"),
		static_cast<int32>(CreateMaterialInstance(Textures, Request, Instance, &Error)),
		static_cast<int32>(EPBRImportStatus::Success));
	TestNotNull(TEXT("Reuse instance"), Instance);
	if (!Instance)
	{
		return false;
	}

	TestEqual(
		TEXT("Bound 基础贴图"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, TEXT("基础贴图")),
		static_cast<UTexture*>(Textures.BaseColor));
	TestEqual(
		TEXT("Bound 粗糙贴图"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, TEXT("粗糙贴图")),
		static_cast<UTexture*>(Textures.Roughness));
	TestEqual(
		TEXT("基础贴图开关"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, TEXT("基础贴图开关")),
		1.0f);
	TestEqual(
		TEXT("粗糙贴图开关"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, TEXT("粗糙贴图开关")),
		1.0f);
	TestEqual(
		TEXT("法线强度"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, TEXT("法线强度")),
		0.8f);
	TestEqual(
		TEXT("粗糙强度"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, TEXT("粗糙强度")),
		1.5f);
	TestEqual(
		TEXT("金属强度"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, TEXT("金属强度")),
		0.4f);
	TestEqual(
		TEXT("置换强度"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, TEXT("置换强度")),
		0.2f);
	TestEqual(
		TEXT("UV缩放"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, TEXT("UV缩放")),
		4.0f);
	TestEqual(
		TEXT("UV缩放3"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, TEXT("UV缩放3")),
		4.0f);

	Request.EnabledMaps.bRoughness = false;
	Request.EnabledMaps.bMetallic = false;
	Request.EnabledMaps.bHeight = false;
	Request.EnabledMaps.bNormal = false;
	Request.bModifyExisting = true;
	Request.ExistingInstance = Instance;
	TestEqual(
		TEXT("Reuse modify with maps off"),
		static_cast<int32>(CreateMaterialInstance(Textures, Request, Instance, &Error)),
		static_cast<int32>(EPBRImportStatus::Success));
	TestEqual(
		TEXT("Unchecked 粗糙贴图 turns switch off"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, TEXT("粗糙贴图开关")),
		0.0f);
	TestEqual(
		TEXT("Unchecked 金属贴图 turns switch off"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, TEXT("金属贴图开关")),
		0.0f);
	TestEqual(
		TEXT("Unchecked 置换贴图 turns switch off"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, TEXT("置换贴图开关")),
		0.0f);
	TestEqual(
		TEXT("Unchecked 置换 zeros 置换强度"),
		UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, TEXT("置换强度")),
		0.0f);

	if (UMaterialInterface* BundledParent = LoadBundledParentByName(TEXT("000基础材质")))
	{
		FPBRMaterialInstanceRequest BundledRequest;
		BundledRequest.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
		BundledRequest.BaseName = FString::Printf(TEXT("T4BundledInst%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
		BundledRequest.ParentMaterial = BundledParent;
		BundledRequest.ConflictPolicy = EPBRImportConflictPolicy::Replace;
		BundledRequest.NormalStrength = 0.1f;
		BundledRequest.bSave = true;
		UMaterialInstanceConstant* BundledInst = nullptr;
		TestEqual(
			TEXT("Bundled 000基础材质 create"),
			static_cast<int32>(CreateMaterialInstance(Textures, BundledRequest, BundledInst, &Error)),
			static_cast<int32>(EPBRImportStatus::Success));
		if (BundledInst)
		{
			auto MapSwitchOn = [](UMaterialInstanceConstant* Inst, const TCHAR* Name) -> bool
			{
				if (UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Inst, Name) > 0.5f)
				{
					return true;
				}
				TArray<FMaterialParameterInfo> Infos;
				TArray<FGuid> Guids;
				Inst->GetAllStaticSwitchParameterInfo(Infos, Guids);
				for (const FMaterialParameterInfo& Info : Infos)
				{
					if (Info.Name != FName(Name))
					{
						continue;
					}
					bool bValue = false;
					FGuid Unused;
					Inst->GetStaticSwitchParameterValue(
						FHashedMaterialParameterInfo(Info.Name, Info.Association, Info.Index),
						bValue,
						Unused);
					return bValue;
				}
				return false;
			};
			TestTrue(
				TEXT("Bundled 粗糙贴图开关 on"),
				MapSwitchOn(BundledInst, TEXT("粗糙贴图开关")));
			TestTrue(
				TEXT("Bundled 金属贴图开关 on"),
				MapSwitchOn(BundledInst, TEXT("金属贴图开关")));
			BundledRequest.EnabledMaps.bRoughness = false;
			BundledRequest.EnabledMaps.bMetallic = false;
			BundledRequest.EnabledMaps.bNormal = false;
			BundledRequest.bModifyExisting = true;
			BundledRequest.ExistingInstance = BundledInst;
			TestEqual(
				TEXT("Bundled modify with maps off"),
				static_cast<int32>(CreateMaterialInstance(Textures, BundledRequest, BundledInst, &Error)),
				static_cast<int32>(EPBRImportStatus::Success));
			TestFalse(
				TEXT("Bundled unchecked 粗糙贴图开关 off"),
				MapSwitchOn(BundledInst, TEXT("粗糙贴图开关")));
			TestFalse(
				TEXT("Bundled unchecked 金属贴图开关 off"),
				MapSwitchOn(BundledInst, TEXT("金属贴图开关")));
		}
	}
	else
	{
		AddWarning(TEXT("Bundled 000基础材质 not loaded; skipped real-parent switch test."));
	}
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
	const FString AssetName = PersistMaterialBaseName() + TEXT("_Inst");
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
	TestTrue(TEXT("Reloaded Roughness still bound"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_RoughnessTexture) != nullptr);
	TestTrue(TEXT("Reloaded Metallic still bound"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_MetallicTexture) != nullptr);
	TestTrue(TEXT("Reloaded AO still bound"),
		UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_AOTexture) != nullptr);
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
