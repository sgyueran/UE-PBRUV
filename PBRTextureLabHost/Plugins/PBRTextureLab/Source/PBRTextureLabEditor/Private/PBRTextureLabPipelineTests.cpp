#include "PBRTextureLabCommand.h"
#include "PBRTextureLabCommands.h"
#include "PBRTextureLabCompat.h"
#include "PBRTextureLabEditorApi.h"
#include "PBRTextureLabNomad.h"
#include "PBRTextureLabPipeline.h"
#include "PBRTextureLabUV.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "StaticMeshAttributes.h"
#include "ToolMenus.h"
#include "UObject/Package.h"
#include "Widgets/Docking/SDockTab.h"

#if WITH_AUTOMATION_WORKER

namespace
{
	constexpr EAutomationTestFlags PipelineTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FString PersistPipelineBaseName()
	{
		return FString::Printf(TEXT("T7Inst%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
	}

	FString PersistPipelineMeshName()
	{
		return FString::Printf(TEXT("T7UV%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
	}

	PBRTextureLab::FPBRImageRgba8 MakePipelineSolid(const int32 Width, const int32 Height, const FColor Color)
	{
		PBRTextureLab::FPBRImageRgba8 Image;
		Image.Width = Width;
		Image.Height = Height;
		Image.Pixels.Init(Color, Width * Height);
		return Image;
	}

	UTexture2D* CreatePipelineSourceTexture(UObject* Outer, const FName Name, const int32 Width, const int32 Height, const FColor Color)
	{
		UTexture2D* Texture = NewObject<UTexture2D>(Outer, Name, RF_Public | RF_Standalone);
		const PBRTextureLab::FPBRImageRgba8 Image = MakePipelineSolid(Width, Height, Color);
		Texture->Source.Init(
			Width,
			Height,
			1,
			1,
			TSF_BGRA8,
			reinterpret_cast<const uint8*>(Image.Pixels.GetData()));
		Texture->SRGB = true;
		Texture->CompressionSettings = TC_Default;
		Texture->PostEditChange();
		Texture->UpdateResource();
		return Texture;
	}

	FMeshDescription MakePipelineQuad(const TArray<FVector2f, TInlineAllocator<4>>& Uv0)
	{
		FMeshDescription MeshDescription;
		FStaticMeshAttributes Attributes(MeshDescription);
		Attributes.Register();
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
		UVs.SetNumChannels(2);

		const FVertexID V0 = MeshDescription.CreateVertex();
		const FVertexID V1 = MeshDescription.CreateVertex();
		const FVertexID V2 = MeshDescription.CreateVertex();
		const FVertexID V3 = MeshDescription.CreateVertex();
		Positions[V0] = FVector3f(0.0f, 0.0f, 0.0f);
		Positions[V1] = FVector3f(100.0f, 0.0f, 0.0f);
		Positions[V2] = FVector3f(100.0f, 100.0f, 0.0f);
		Positions[V3] = FVector3f(0.0f, 100.0f, 0.0f);

		const FVertexInstanceID I0 = MeshDescription.CreateVertexInstance(V0);
		const FVertexInstanceID I1 = MeshDescription.CreateVertexInstance(V1);
		const FVertexInstanceID I2 = MeshDescription.CreateVertexInstance(V2);
		const FVertexInstanceID I3 = MeshDescription.CreateVertexInstance(V3);
		const FVertexInstanceID Instances[4] = { I0, I1, I2, I3 };
		const TArray<FVector2f, TInlineAllocator<4>> Uv1 = {
			FVector2f(0.0f, 0.0f), FVector2f(1.0f, 0.0f), FVector2f(1.0f, 1.0f), FVector2f(0.0f, 1.0f)
		};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			UVs.Set(Instances[Index], 0, Uv0[Index]);
			UVs.Set(Instances[Index], 1, Uv1[Index]);
			Normals.Set(Instances[Index], FVector3f(0.0f, 0.0f, 1.0f));
		}

		const FPolygonGroupID Group = MeshDescription.CreatePolygonGroup();
		const FVertexInstanceID Tri0[3] = { I0, I1, I2 };
		const FVertexInstanceID Tri1[3] = { I0, I2, I3 };
		MeshDescription.CreateTriangle(Group, Tri0);
		MeshDescription.CreateTriangle(Group, Tri1);
		return MeshDescription;
	}

	UStaticMesh* CreatePipelineMesh(UObject* Outer, const FName Name)
	{
		const TArray<FVector2f, TInlineAllocator<4>> Uv0 = {
			FVector2f(0.0f, 0.0f), FVector2f(0.25f, 0.0f), FVector2f(0.25f, 0.5f), FVector2f(0.0f, 0.5f)
		};
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, Name, RF_Public | RF_Standalone | RF_Transactional);
		Mesh->SetLightMapCoordinateIndex(1);
		Mesh->AddSourceModel();
		Mesh->CreateMeshDescription(0, MakePipelineQuad(Uv0));
		PBRTextureLab::CommitStaticMeshDescription(Mesh, 0);
		return Mesh;
	}

	float ReadPipelineUv0X(UStaticMesh* Mesh, const int32 InstanceIndex = 0)
	{
		FMeshDescription* MeshDescription = PBRTextureLab::GetStaticMeshDescription(Mesh, 0);
		if (!MeshDescription)
		{
			return -1.0f;
		}
		const TVertexInstanceAttributesRef<FVector2f> UVs = PBRTextureLab::GetVertexInstanceUVs(*MeshDescription);
		int32 Index = 0;
		for (const FVertexInstanceID InstanceId : MeshDescription->VertexInstances().GetElementIDs())
		{
			if (Index == InstanceIndex)
			{
				return UVs.Get(InstanceId, 0).X;
			}
			++Index;
		}
		return -1.0f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabPipelineFromTexture2D,
	"PBRTextureLab.Pipeline.FromTexture2D",
	PipelineTestFlags)

bool FPBRTextureLabPipelineFromTexture2D::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	AddExpectedMessagePlain(
		FString(MetallicDisclaimer),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);

	UTexture2D* Source = CreatePipelineSourceTexture(
		GetTransientPackage(),
		TEXT("T7SourceTex"),
		16,
		16,
		FColor(180, 90, 40, 255));
	TestNotNull(TEXT("Source Texture2D"), Source);
	if (!Source)
	{
		return false;
	}

	FPBRImageRgba8 Decoded;
	FString Error;
	TestTrue(TEXT("Read Texture2D source"), ReadSourceTexture2D(Source, Decoded, &Error));
	TestEqual(TEXT("Decoded width"), Decoded.Width, 16);
	TestEqual(TEXT("Decoded height"), Decoded.Height, 16);

	FPBRGenerateRequest Request;
	Request.SourceTexture = Source;
	Request.OutputWidth = 8;
	Request.OutputHeight = 8;
	Request.PixelParams.HeightBlurRadius = 0;
	Request.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	Request.BaseName = FString::Printf(TEXT("T7Tex%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
	Request.ConflictPolicy = EPBRImportConflictPolicy::Replace;
	Request.MaterialNormalStrength = 0.8f;
	Request.MaterialUVScale = 3.0f;
	Request.bSave = true;

	FPBRGenerateResult Result;
	const EPBRImportStatus Status = GenerateAndImportFromSource(Request, Result, &Error);
	TestEqual(TEXT("Pipeline texture status"), static_cast<int32>(Status), static_cast<int32>(EPBRImportStatus::Success));
	TestEqual(TEXT("Working size"), Result.WorkingImage.Width, 8);
	TestTrue(TEXT("Imported maps"), Result.Textures.HasAll());
	TestNotNull(TEXT("Material instance"), Result.MaterialInstance);
	TestTrue(TEXT("Output folder matches material name"),
		Result.OutputFolder.EndsWith(TEXT("/") + Result.CreatedMaterialName)
		|| Result.OutputFolder.EndsWith(Result.CreatedMaterialName));
	if (Result.MaterialInstance)
	{
		TestEqual(TEXT("MIC lives in material folder"), Result.MaterialInstance->GetOutermost()->GetName(), Result.OutputFolder / Result.CreatedMaterialName);
	}
	if (Result.Textures.BaseColor)
	{
		TestTrue(TEXT("BaseColor lives in material folder"),
			Result.Textures.BaseColor->GetOutermost()->GetName().StartsWith(Result.OutputFolder + TEXT("/")));
	}
	if (Result.MaterialInstance)
	{
		TestEqual(
			TEXT("MIC NormalStrength"),
			UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(
				Result.MaterialInstance,
				PBRTEXTURELAB_PARAM_NormalStrength),
			0.8f);
		TestEqual(
			TEXT("MIC UVScale"),
			UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(
				Result.MaterialInstance,
				PBRTEXTURELAB_PARAM_UVScale),
			3.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabPipelineFromLocalFile,
	"PBRTextureLab.Pipeline.FromLocalFile",
	PipelineTestFlags)

bool FPBRTextureLabPipelineFromLocalFile::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	AddExpectedMessagePlain(
		FString(MetallicDisclaimer),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);

	const FString Filename = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("PBRTextureLab"),
		TEXT("T7Local.png")));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FString WriteError;
	TestTrue(TEXT("Write local PNG"), WriteRgba8Png(MakePipelineSolid(12, 10, FColor(40, 120, 200, 255)), Filename, &WriteError));

	FPBRImageRgba8 Decoded;
	FString Error;
	TestTrue(TEXT("Read local PNG"), ReadSourceLocalFile(Filename, Decoded, &Error));
	TestEqual(TEXT("Local width"), Decoded.Width, 12);
	TestEqual(TEXT("Local height"), Decoded.Height, 10);

	FPBRGenerateRequest Request;
	Request.LocalImagePath = Filename;
	Request.PixelParams.HeightBlurRadius = 0;
	Request.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	Request.BaseName = FString::Printf(TEXT("T7File%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
	Request.ConflictPolicy = EPBRImportConflictPolicy::UniqueName;
	Request.bSave = true;

	FPBRGenerateResult Result;
	TestEqual(
		TEXT("Pipeline file status"),
		static_cast<int32>(GenerateAndImportFromSource(Request, Result, &Error)),
		static_cast<int32>(EPBRImportStatus::Success));
	TestTrue(TEXT("File imported maps"), Result.Textures.HasAll());
	TestNotNull(TEXT("File material instance"), Result.MaterialInstance);
	IFileManager::Get().Delete(*Filename);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabPipelineAssembleExisting,
	"PBRTextureLab.Pipeline.AssembleExisting",
	PipelineTestFlags)

bool FPBRTextureLabPipelineAssembleExisting::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	AddExpectedMessagePlain(
		FString(MetallicDisclaimer),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);

	UTexture2D* Source = CreatePipelineSourceTexture(
		GetTransientPackage(),
		TEXT("T7AssembleSrc"),
		8,
		8,
		FColor(90, 140, 70, 255));
	FPBRGenerateRequest Seed;
	Seed.SourceTexture = Source;
	Seed.PixelParams.HeightBlurRadius = 0;
	Seed.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	Seed.BaseName = FString::Printf(TEXT("T7Seed%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
	Seed.ConflictPolicy = EPBRImportConflictPolicy::Replace;
	Seed.bSave = true;
	FPBRGenerateResult Seeded;
	FString Error;
	TestEqual(
		TEXT("Seed generate"),
		static_cast<int32>(GenerateAndImportFromSource(Seed, Seeded, &Error)),
		static_cast<int32>(EPBRImportStatus::Success));
	TestTrue(TEXT("Seed textures"), Seeded.Textures.HasAny());

	FPBRGenerateRequest Assemble;
	Assemble.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	Assemble.MaterialInstanceName = FString::Printf(TEXT("T7Asm%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
	Assemble.ConflictPolicy = EPBRImportConflictPolicy::Replace;
	Assemble.bCopyExistingTexturesToFolder = true;
	Assemble.bSave = true;

	FPBRImportedTextures Selected;
	Selected.BaseColor = Seeded.Textures.BaseColor;
	Selected.Normal = Seeded.Textures.Normal;
	Selected.Roughness = Seeded.Textures.Roughness;

	FPBRGenerateResult Assembled;
	TestEqual(
		TEXT("Assemble status"),
		static_cast<int32>(CreateMaterialFromExistingTextures(Assemble, Selected, Assembled, &Error)),
		static_cast<int32>(EPBRImportStatus::Success));
	TestNotNull(TEXT("Assemble MIC"), Assembled.MaterialInstance);
	TestEqual(TEXT("Assemble folder name"), Assembled.CreatedMaterialName, Assemble.MaterialInstanceName);
	TestTrue(TEXT("Assemble folder path"), Assembled.OutputFolder.EndsWith(Assemble.MaterialInstanceName));
	if (Assembled.Textures.BaseColor)
	{
		TestTrue(
			TEXT("Copied BaseColor into material folder"),
			Assembled.Textures.BaseColor->GetOutermost()->GetName().StartsWith(Assembled.OutputFolder + TEXT("/")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabPipelineCancelAndInvalid,
	"PBRTextureLab.Pipeline.CancelAndInvalid",
	PipelineTestFlags)

bool FPBRTextureLabPipelineCancelAndInvalid::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	FPBRGenerateRequest Cancelled;
	Cancelled.bCancelled = true;
	Cancelled.LocalImagePath = TEXT("C:/does-not-exist.png");
	FPBRGenerateResult CancelledOut;
	FString Error;
	TestEqual(
		TEXT("Cancel status"),
		static_cast<int32>(GenerateAndImportFromSource(Cancelled, CancelledOut, &Error)),
		static_cast<int32>(EPBRImportStatus::Cancelled));
	TestTrue(TEXT("Cancel creates no MIC"), CancelledOut.MaterialInstance == nullptr);

	AddExpectedErrorPlain(TEXT("Select a Texture2D or a local image"), EAutomationExpectedErrorFlags::Contains, 1);
	FPBRGenerateRequest Empty;
	FPBRGenerateResult EmptyOut;
	TestEqual(
		TEXT("Empty source"),
		static_cast<int32>(GenerateAndImportFromSource(Empty, EmptyOut, &Error)),
		static_cast<int32>(EPBRImportStatus::Failed));

	AddExpectedErrorPlain(TEXT("Local image does not exist"), EAutomationExpectedErrorFlags::Contains, 1);
	FPBRGenerateRequest Missing;
	Missing.LocalImagePath = TEXT("C:/pbrtexturelab-missing-source.png");
	FPBRGenerateResult MissingOut;
	TestEqual(
		TEXT("Missing file"),
		static_cast<int32>(GenerateAndImportFromSource(Missing, MissingOut, &Error)),
		static_cast<int32>(EPBRImportStatus::Failed));

	AddExpectedErrorPlain(TEXT("Output size"), EAutomationExpectedErrorFlags::Contains, 1);
	FPBRImageRgba8 Source = MakePipelineSolid(8, 8, FColor::White);
	FPBRImageRgba8 Working;
	TestFalse(
		TEXT("Reject oversized output"),
		PrepareWorkingImage(Source, MaxOutputDimension + 1, MaxOutputDimension + 1, Working, &Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabNomadTabRegistered,
	"PBRTextureLab.Nomad.TabRegistered",
	PipelineTestFlags)

bool FPBRTextureLabNomadTabRegistered::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	TestTrue(TEXT("Commands registered"), FPBRTextureLabCommands::IsRegistered());
	TestTrue(TEXT("Nomad tab spawner registered"), IsNomadTabRegistered());

	const TSharedPtr<FUICommandInfo> OpenTab = FInputBindingManager::Get().FindCommandInContext(
		GetUVCommandContextName(),
		GetOpenNomadTabCommandName());
	TestTrue(TEXT("OpenNomadTab command exists"), OpenTab.IsValid());
	if (OpenTab.IsValid())
	{
		TestFalse(
			TEXT("OpenNomadTab has no default chord"),
			OpenTab->GetDefaultChord(EMultipleKeyBindingIndex::Primary).IsValidChord());
	}

	UToolMenus* Menus = UToolMenus::Get();
	TestNotNull(TEXT("ToolMenus"), Menus);
	if (Menus)
	{
		TestNotNull(TEXT("Tools menu"), Menus->ExtendMenu("LevelEditor.MainMenu.Tools"));
	}

	if (FSlateApplication::IsInitialized())
	{
		const TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(FTabId(GetNomadTabId()));
		if (Tab.IsValid())
		{
			TestEqual(TEXT("Nomad tab label"), Tab->GetTabLabel().ToString(), FString(TEXT("PBR Texture Lab")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabIntegrationGenerateUVUndoRedo,
	"PBRTextureLab.Integration.GenerateUVUndoRedo",
	PipelineTestFlags)

bool FPBRTextureLabIntegrationGenerateUVUndoRedo::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	AddExpectedMessagePlain(
		FString(MetallicDisclaimer),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);

	UTexture2D* Source = CreatePipelineSourceTexture(
		GetTransientPackage(),
		TEXT("T7IntegrationSrc"),
		16,
		16,
		FColor(200, 80, 30, 255));

	FPBRGenerateRequest Request;
	Request.SourceTexture = Source;
	Request.PixelParams.HeightBlurRadius = 0;
	Request.DestinationPath = TEXT("/Game/PBRTextureLab/Automation");
	Request.BaseName = PersistPipelineBaseName();
	Request.ConflictPolicy = EPBRImportConflictPolicy::Replace;
	Request.MaterialNormalStrength = 0.75f;
	Request.MaterialUVScale = 2.0f;
	Request.bSave = true;

	FPBRGenerateResult Result;
	FString Error;
	TestEqual(
		TEXT("Integration generate"),
		static_cast<int32>(GenerateAndImportFromSource(Request, Result, &Error)),
		static_cast<int32>(EPBRImportStatus::Success));
	TestNotNull(TEXT("Integration MIC"), Result.MaterialInstance);
	TestTrue(TEXT("Integration maps imported"), Result.Textures.HasAll());

	const FString MeshName = PersistPipelineMeshName();
	UPackage* Package = CreatePackage(*(TEXT("/Game/PBRTextureLab/Automation/") + MeshName));
	UStaticMesh* Mesh = CreatePipelineMesh(Package, FName(*MeshName));
	FAssetRegistryModule::AssetCreated(Mesh);
	const float BaselineX = ReadPipelineUv0X(Mesh);

	FPBRUVScaleRequest UvRequest;
	UvRequest.bSave = false;
	UvRequest.bTransact = true;
	TestEqual(
		TEXT("Apply 100"),
		static_cast<int32>(ApplyUVPreset(Mesh, EPBRUVPreset::Scale100, UvRequest, &Error)),
		static_cast<int32>(EPBRUVStatus::Success));
	TestEqual(TEXT("Scale 100"), GetAppliedUVScale(Mesh), 100);
	TestEqual(TEXT("UV 100x"), ReadPipelineUv0X(Mesh), BaselineX * 100.0f);

	TestEqual(
		TEXT("Apply 500"),
		static_cast<int32>(ApplyUVPreset(Mesh, EPBRUVPreset::Scale500, UvRequest, &Error)),
		static_cast<int32>(EPBRUVStatus::Success));
	TestEqual(TEXT("Scale 500"), GetAppliedUVScale(Mesh), 500);
	TestEqual(TEXT("UV 500x"), ReadPipelineUv0X(Mesh), BaselineX * 500.0f);

	TestTrue(TEXT("Editor available"), GEditor != nullptr);
	TestTrue(TEXT("Undo to 100"), GEditor->UndoTransaction());
	TestEqual(TEXT("Scale after undo"), GetAppliedUVScale(Mesh), 100);
	TestEqual(TEXT("UV after undo"), ReadPipelineUv0X(Mesh), BaselineX * 100.0f);
	TestTrue(TEXT("Redo to 500"), GEditor->RedoTransaction());
	TestEqual(TEXT("Scale after redo"), GetAppliedUVScale(Mesh), 500);
	TestEqual(TEXT("UV after redo"), ReadPipelineUv0X(Mesh), BaselineX * 500.0f);
	TestTrue(TEXT("Undo to persist 100"), GEditor->UndoTransaction());

	UvRequest.bSave = true;
	TestEqual(
		TEXT("Persist 100"),
		static_cast<int32>(ApplyUVPreset(Mesh, EPBRUVPreset::Scale100, UvRequest, &Error)),
		static_cast<int32>(EPBRUVStatus::Success));
	TestTrue(
		TEXT("Mesh package exists"),
		FPackageName::DoesPackageExist(TEXT("/Game/PBRTextureLab/Automation/") + MeshName));
	TestTrue(
		TEXT("MIC package exists"),
		Result.MaterialInstance && FPackageName::DoesPackageExist(Result.MaterialInstance->GetOutermost()->GetName()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabIntegrationReloadAfterRestart,
	"PBRTextureLab.Integration.ReloadAfterRestart",
	PipelineTestFlags)

bool FPBRTextureLabIntegrationReloadAfterRestart::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	const FString InstanceName = PersistPipelineBaseName();
	const FString InstancePath = TEXT("/Game/PBRTextureLab/Automation/") + InstanceName + TEXT("/") + InstanceName + TEXT(".") + InstanceName;
	UMaterialInstanceConstant* Instance = LoadObject<UMaterialInstanceConstant>(nullptr, *InstancePath);
	TestNotNull(TEXT("Reload integration MIC"), Instance);
	if (Instance)
	{
		TestNotNull(TEXT("Reloaded parent"), Instance->Parent.Get());
		TestTrue(
			TEXT("Reloaded BaseColor"),
			UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_BaseColorTexture) != nullptr);
		TestTrue(
			TEXT("Reloaded ORM"),
			UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(Instance, PBRTEXTURELAB_PARAM_ORMTexture) != nullptr);
		TestEqual(
			TEXT("Reloaded NormalStrength"),
			UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_NormalStrength),
			0.75f);
		TestEqual(
			TEXT("Reloaded UVScale"),
			UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(Instance, PBRTEXTURELAB_PARAM_UVScale),
			2.0f);
	}

	const FString MeshName = PersistPipelineMeshName();
	const FString MeshPath = TEXT("/Game/PBRTextureLab/Automation/") + MeshName + TEXT(".") + MeshName;
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	TestNotNull(TEXT("Reload integration mesh"), Mesh);
	if (Mesh)
	{
		TestEqual(TEXT("Reloaded UV scale"), GetAppliedUVScale(Mesh), 100);
		TestEqual(TEXT("Reloaded first U is 0"), ReadPipelineUv0X(Mesh, 0), 0.0f);
		TestEqual(TEXT("Reloaded second U is 25"), ReadPipelineUv0X(Mesh, 1), 25.0f);
	}
	return true;
}

#endif
