#include "PBRTextureLabCommand.h"
#include "PBRTextureLabCommands.h"
#include "PBRTextureLabCompat.h"
#include "PBRTextureLabEditorApi.h"
#include "PBRTextureLabUV.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TargetPoint.h"
#include "Framework/Commands/InputBindingManager.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "StaticMeshAttributes.h"
#include "ToolMenus.h"
#include "UObject/Package.h"

#if WITH_AUTOMATION_WORKER

namespace
{
	constexpr EAutomationTestFlags CommandTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FString CommandPersistName(const TCHAR* Suffix)
	{
		return FString::Printf(
			TEXT("T6%s%d%d"),
			Suffix,
			PBRTEXTURELAB_ENGINE_MAJOR,
			PBRTEXTURELAB_ENGINE_MINOR);
	}

	FMeshDescription MakeCommandTestQuad(const TArray<FVector2f, TInlineAllocator<4>>& Uv0)
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

	UStaticMesh* CreateCommandTestMesh(UObject* Outer, const FName Name)
	{
		const TArray<FVector2f, TInlineAllocator<4>> Uv0 = {
			FVector2f(0.0f, 0.0f), FVector2f(0.25f, 0.0f), FVector2f(0.25f, 0.5f), FVector2f(0.0f, 0.5f)
		};
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, Name, RF_Public | RF_Standalone | RF_Transactional);
		Mesh->SetLightMapCoordinateIndex(1);
		Mesh->AddSourceModel();
		Mesh->CreateMeshDescription(0, MakeCommandTestQuad(Uv0));
		PBRTextureLab::CommitStaticMeshDescription(Mesh, 0);
		return Mesh;
	}

	UStaticMesh* CreateSavedCommandMesh(const FString& AssetName)
	{
		const FString PackageName = TEXT("/Game/PBRTextureLab/Automation/") + AssetName;
		UPackage* Package = CreatePackage(*PackageName);
		UStaticMesh* Mesh = CreateCommandTestMesh(Package, FName(*AssetName));
		FAssetRegistryModule::AssetCreated(Mesh);
		return Mesh;
	}

	float ReadFirstUv0X(UStaticMesh* Mesh)
	{
		FMeshDescription* MeshDescription = PBRTextureLab::GetStaticMeshDescription(Mesh, 0);
		if (!MeshDescription)
		{
			return -1.0f;
		}
		const TVertexInstanceAttributesRef<FVector2f> UVs = PBRTextureLab::GetVertexInstanceUVs(*MeshDescription);
		const FVertexInstanceID First = *MeshDescription->VertexInstances().GetElementIDs().begin();
		return UVs.Get(First, 0).X;
	}

	UWorld* CommandEditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	AStaticMeshActor* SpawnCommandMeshActor(UWorld* World, UStaticMesh* Mesh, const FVector& Location)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags = RF_Transactional;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator, Params);
		if (Actor && Actor->GetStaticMeshComponent())
		{
			Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
		}
		return Actor;
	}

	void SelectOnly(AActor* Actor)
	{
		USelection* Selection = PBRTextureLab::GetSelectedActors();
		if (!Selection)
		{
			return;
		}
		Selection->BeginBatchSelectOperation();
		Selection->DeselectAll();
		if (Actor)
		{
			Selection->Select(Actor);
		}
		Selection->EndBatchSelectOperation();
	}

	void SelectTwo(AActor* First, AActor* Second)
	{
		USelection* Selection = PBRTextureLab::GetSelectedActors();
		if (!Selection)
		{
			return;
		}
		Selection->BeginBatchSelectOperation();
		Selection->DeselectAll();
		if (First)
		{
			Selection->Select(First);
		}
		if (Second)
		{
			Selection->Select(Second);
		}
		Selection->EndBatchSelectOperation();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabCommandShortcuts,
	"PBRTextureLab.Command.Shortcuts",
	CommandTestFlags)

bool FPBRTextureLabCommandShortcuts::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	TestTrue(TEXT("Commands registered"), FPBRTextureLabCommands::IsRegistered());
	const TSharedPtr<FUICommandInfo> Scale100 = FInputBindingManager::Get().FindCommandInContext(
		GetUVCommandContextName(),
		GetUVScale100CommandName());
	const TSharedPtr<FUICommandInfo> Scale500 = FInputBindingManager::Get().FindCommandInContext(
		GetUVCommandContextName(),
		GetUVScale500CommandName());
	TestTrue(TEXT("UVScale100 exists"), Scale100.IsValid());
	TestTrue(TEXT("UVScale500 exists"), Scale500.IsValid());
	if (Scale100.IsValid())
	{
		TestFalse(
			TEXT("UVScale100 has no default chord"),
			Scale100->GetDefaultChord(EMultipleKeyBindingIndex::Primary).IsValidChord());
	}
	if (Scale500.IsValid())
	{
		TestFalse(
			TEXT("UVScale500 has no default chord"),
			Scale500->GetDefaultChord(EMultipleKeyBindingIndex::Primary).IsValidChord());
	}

	UToolMenus* Menus = UToolMenus::Get();
	TestNotNull(TEXT("ToolMenus"), Menus);
	if (Menus)
	{
		TestNotNull(TEXT("Tools menu"), Menus->ExtendMenu("LevelEditor.MainMenu.Tools"));
		TestNotNull(TEXT("User toolbar"), Menus->ExtendMenu("LevelEditor.LevelEditorToolBar.User"));
		TestNotNull(TEXT("Asset context menu"), Menus->ExtendMenu("ContentBrowser.AssetContextMenu"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabCommandSelectionGuards,
	"PBRTextureLab.Command.SelectionGuards",
	CommandTestFlags)

bool FPBRTextureLabCommandSelectionGuards::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	UWorld* World = CommandEditorWorld();
	TestNotNull(TEXT("Editor world"), World);
	if (!World || !GEditor)
	{
		return false;
	}

	UStaticMesh* Mesh = CreateSavedCommandMesh(CommandPersistName(TEXT("Guard")));
	AStaticMeshActor* MeshActor = SpawnCommandMeshActor(World, Mesh, FVector(0.0f, 0.0f, 0.0f));
	ATargetPoint* Marker = World->SpawnActor<ATargetPoint>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Mesh actor"), MeshActor);
	TestNotNull(TEXT("Non-SM actor"), Marker);
	if (!MeshActor || !Marker)
	{
		return false;
	}

	const float Before = ReadFirstUv0X(Mesh);
	FPBRUVCommandRequest Request;
	Request.Preset = EPBRUVPreset::Scale100;
	Request.EditChoice = EPBRUVEditChoice::ModifySource;
	FString Error;

	SelectOnly(nullptr);
	AddExpectedMessagePlain(
		TEXT("empty selection"),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);
	TestEqual(
		TEXT("Empty selection"),
		static_cast<int32>(ExecuteUVCommand(Request, &Error)),
		static_cast<int32>(EPBRUVCommandStatus::EmptySelection));
	TestEqual(TEXT("Empty leaves UV"), ReadFirstUv0X(Mesh), Before);

	SelectOnly(Marker);
	AddExpectedMessagePlain(
		TEXT("selection is not a Static Mesh"),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);
	TestEqual(
		TEXT("Non-SM selection"),
		static_cast<int32>(ExecuteUVCommand(Request, &Error)),
		static_cast<int32>(EPBRUVCommandStatus::Unsupported));
	TestEqual(TEXT("Non-SM leaves UV"), ReadFirstUv0X(Mesh), Before);

	SelectTwo(MeshActor, Marker);
	AddExpectedMessagePlain(
		TEXT("mixed selection"),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);
	TestEqual(
		TEXT("Mixed selection"),
		static_cast<int32>(ExecuteUVCommand(Request, &Error)),
		static_cast<int32>(EPBRUVCommandStatus::MixedSelection));
	TestEqual(TEXT("Mixed leaves UV"), ReadFirstUv0X(Mesh), Before);

	World->DestroyActor(MeshActor);
	World->DestroyActor(Marker);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabCommandDuplicateAndModify,
	"PBRTextureLab.Command.DuplicateAndModify",
	CommandTestFlags)

bool FPBRTextureLabCommandDuplicateAndModify::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	UWorld* World = CommandEditorWorld();
	TestNotNull(TEXT("Editor world"), World);
	if (!World || !GEditor)
	{
		return false;
	}

	UStaticMesh* Mesh = CreateSavedCommandMesh(CommandPersistName(TEXT("Dup")));
	AStaticMeshActor* SelectedActor = SpawnCommandMeshActor(World, Mesh, FVector(0.0f, 0.0f, 100.0f));
	AStaticMeshActor* OtherActor = SpawnCommandMeshActor(World, Mesh, FVector(200.0f, 0.0f, 100.0f));
	TestNotNull(TEXT("Selected actor"), SelectedActor);
	TestNotNull(TEXT("Unselected actor"), OtherActor);
	if (!SelectedActor || !OtherActor)
	{
		return false;
	}

	const float BaselineX = ReadFirstUv0X(Mesh);
	TestTrue(TEXT("Shared users before"), CountStaticMeshComponentUsers(Mesh) >= 2);

	SelectOnly(SelectedActor);
	FPBRUVCommandRequest CancelRequest;
	CancelRequest.EditChoice = EPBRUVEditChoice::Cancel;
	FString Error;
	TestEqual(
		TEXT("Cancel"),
		static_cast<int32>(ExecuteUVCommand(CancelRequest, &Error)),
		static_cast<int32>(EPBRUVCommandStatus::Cancelled));
	TestEqual(TEXT("Cancel leaves source UV"), ReadFirstUv0X(Mesh), BaselineX);
	TestTrue(TEXT("Cancel leaves other mesh"), OtherActor->GetStaticMeshComponent()->GetStaticMesh() == Mesh);

	FPBRUVCommandRequest DuplicateRequest;
	DuplicateRequest.Preset = EPBRUVPreset::Scale100;
	DuplicateRequest.EditChoice = EPBRUVEditChoice::DuplicateAndRebind;
	TestEqual(
		TEXT("Duplicate"),
		static_cast<int32>(ExecuteUVCommand(DuplicateRequest, &Error)),
		static_cast<int32>(EPBRUVCommandStatus::Success));

	UStaticMesh* SelectedMesh = SelectedActor->GetStaticMeshComponent()->GetStaticMesh();
	UStaticMesh* OtherMesh = OtherActor->GetStaticMeshComponent()->GetStaticMesh();
	TestNotNull(TEXT("Selected still has a mesh"), SelectedMesh);
	TestTrue(TEXT("Selected rebound to copy"), SelectedMesh != Mesh);
	TestTrue(TEXT("Unselected keeps source"), OtherMesh == Mesh);
	TestEqual(TEXT("Source UV unchanged"), ReadFirstUv0X(Mesh), BaselineX);
	TestEqual(TEXT("Copy UV is 100x"), ReadFirstUv0X(SelectedMesh), BaselineX * 100.0f);

	SelectOnly(OtherActor);
	FPBRUVCommandRequest ModifyRequest;
	ModifyRequest.Preset = EPBRUVPreset::Scale500;
	ModifyRequest.EditChoice = EPBRUVEditChoice::ModifySource;
	TestEqual(
		TEXT("Modify source"),
		static_cast<int32>(ExecuteUVCommand(ModifyRequest, &Error)),
		static_cast<int32>(EPBRUVCommandStatus::Success));
	TestEqual(TEXT("Modified source UV is 500x"), ReadFirstUv0X(Mesh), BaselineX * 500.0f);
	TestEqual(TEXT("Copy stayed at 100x"), ReadFirstUv0X(SelectedMesh), BaselineX * 100.0f);

	World->DestroyActor(SelectedActor);
	World->DestroyActor(OtherActor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabCommandAssetSelection,
	"PBRTextureLab.Command.AssetSelection",
	CommandTestFlags)

bool FPBRTextureLabCommandAssetSelection::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	UStaticMesh* Mesh = CreateSavedCommandMesh(CommandPersistName(TEXT("Asset")));
	const float BaselineX = ReadFirstUv0X(Mesh);

	FPBRUVSelection Selection;
	FPBRUVSelectionItem Item;
	Item.Mesh = Mesh;
	Selection.Items.Add(Item);
	Selection.ConsideredCount = 1;

	FPBRUVCommandRequest Request;
	Request.Preset = EPBRUVPreset::Scale100;
	Request.EditChoice = EPBRUVEditChoice::ModifySource;
	FString Error;
	TestEqual(
		TEXT("Asset modify"),
		static_cast<int32>(ExecuteUVCommand(Request, Selection, &Error)),
		static_cast<int32>(EPBRUVCommandStatus::Success));
	TestEqual(TEXT("Asset UV is 100x"), ReadFirstUv0X(Mesh), BaselineX * 100.0f);
	return true;
}

#endif
