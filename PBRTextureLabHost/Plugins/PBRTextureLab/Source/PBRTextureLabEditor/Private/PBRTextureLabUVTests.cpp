#include "PBRTextureLabUV.h"
#include "PBRTextureLabCompat.h"
#include "PBRTextureLabEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"

#if WITH_AUTOMATION_WORKER

namespace
{
	constexpr EAutomationTestFlags UVTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FString PersistMeshName()
	{
		return FString::Printf(TEXT("T5UV%d%d"), PBRTEXTURELAB_ENGINE_MAJOR, PBRTEXTURELAB_ENGINE_MINOR);
	}

	FMeshDescription MakeUvTestQuad(
		const TArray<FVector2f, TInlineAllocator<4>>& Uv0,
		const TArray<FVector2f, TInlineAllocator<4>>& Uv1)
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

	UStaticMesh* CreateUvTestMesh(
		UObject* Outer,
		const FName Name,
		const TArray<FMeshDescription>& Lods)
	{
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, Name, RF_Public | RF_Standalone | RF_Transactional);
		Mesh->SetLightMapCoordinateIndex(1);
		for (int32 LodIndex = 0; LodIndex < Lods.Num(); ++LodIndex)
		{
			Mesh->AddSourceModel();
			Mesh->CreateMeshDescription(LodIndex, Lods[LodIndex]);
			PBRTextureLab::CommitStaticMeshDescription(Mesh, LodIndex);
		}
		return Mesh;
	}

	TArray<FVector2f> ReadUvChannel(UStaticMesh* Mesh, const int32 LodIndex, const int32 Channel)
	{
		TArray<FVector2f> Values;
		FMeshDescription* MeshDescription = PBRTextureLab::GetStaticMeshDescription(Mesh, LodIndex);
		if (!MeshDescription)
		{
			return Values;
		}
		const TVertexInstanceAttributesRef<FVector2f> UVs = PBRTextureLab::GetVertexInstanceUVs(*MeshDescription);
		if (!UVs.IsValid() || Channel >= UVs.GetNumChannels())
		{
			return Values;
		}
		for (const FVertexInstanceID VertexInstanceID : MeshDescription->VertexInstances().GetElementIDs())
		{
			Values.Add(UVs.Get(VertexInstanceID, Channel));
		}
		return Values;
	}

	bool UVsNearlyEqual(
		FAutomationTestBase& Test,
		const TCHAR* What,
		const TArray<FVector2f>& Actual,
		const TArray<FVector2f>& Expected)
	{
		bool bOk = Test.TestEqual(FString::Printf(TEXT("%s count"), What), Actual.Num(), Expected.Num());
		const int32 Count = FMath::Min(Actual.Num(), Expected.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			bOk = Test.TestEqual(
				FString::Printf(TEXT("%s[%d].X"), What, Index),
				Actual[Index].X,
				Expected[Index].X) && bOk;
			bOk = Test.TestEqual(
				FString::Printf(TEXT("%s[%d].Y"), What, Index),
				Actual[Index].Y,
				Expected[Index].Y) && bOk;
		}
		return bOk;
	}

	TArray<FVector2f> ScaleUVs(const TArray<FVector2f>& Source, const float Scale)
	{
		TArray<FVector2f> Out;
		Out.Reserve(Source.Num());
		for (const FVector2f& Value : Source)
		{
			Out.Add(FVector2f(Value.X * Scale, Value.Y * Scale));
		}
		return Out;
	}

	void PokeUv0(UStaticMesh* Mesh, const int32 LodIndex, const FVector2f NewValue)
	{
		FMeshDescription* MeshDescription = PBRTextureLab::GetStaticMeshDescription(Mesh, LodIndex);
		TVertexInstanceAttributesRef<FVector2f> UVs = PBRTextureLab::GetVertexInstanceUVs(*MeshDescription);
		const FVertexInstanceID First = *MeshDescription->VertexInstances().GetElementIDs().begin();
		UVs.Set(First, PBRTEXTURELAB_UV_CHANNEL_INDEX, NewValue);
		PBRTextureLab::CommitStaticMeshDescription(Mesh, LodIndex);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabUVAbsolutePresets,
	"PBRTextureLab.UV.AbsolutePresets",
	UVTestFlags)

bool FPBRTextureLabUVAbsolutePresets::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	const TArray<FVector2f, TInlineAllocator<4>> Lod0Uv0 = {
		FVector2f(0.0f, 0.0f), FVector2f(0.25f, 0.0f), FVector2f(0.25f, 0.5f), FVector2f(0.0f, 0.5f)
	};
	const TArray<FVector2f, TInlineAllocator<4>> Lod1Uv0 = {
		FVector2f(0.5f, 0.25f), FVector2f(1.0f, 0.25f), FVector2f(1.0f, 1.0f), FVector2f(0.5f, 1.0f)
	};
	const TArray<FVector2f, TInlineAllocator<4>> LightmapUv = {
		FVector2f(0.1f, 0.2f), FVector2f(0.3f, 0.2f), FVector2f(0.3f, 0.4f), FVector2f(0.1f, 0.4f)
	};

	TArray<FMeshDescription> Lods;
	Lods.Add(MakeUvTestQuad(Lod0Uv0, LightmapUv));
	Lods.Add(MakeUvTestQuad(Lod1Uv0, LightmapUv));
	UStaticMesh* Mesh = CreateUvTestMesh(GetTransientPackage(), TEXT("T5Absolute"), Lods);

	const TArray<FVector2f> Baseline0 = ReadUvChannel(Mesh, 0, 0);
	const TArray<FVector2f> Baseline1 = ReadUvChannel(Mesh, 1, 0);
	const TArray<FVector2f> Lightmap0 = ReadUvChannel(Mesh, 0, 1);

	FPBRUVScaleRequest Request;
	FString Error;
	TestEqual(TEXT("Apply 100 LOD0"), static_cast<int32>(ApplyUVPreset(Mesh, EPBRUVPreset::Scale100, Request, &Error)), static_cast<int32>(EPBRUVStatus::Success));
	TestEqual(TEXT("Applied scale 100"), GetAppliedUVScale(Mesh), 100);
	UVsNearlyEqual(*this, TEXT("LOD0 after 100"), ReadUvChannel(Mesh, 0, 0), ScaleUVs(Baseline0, 100.0f));
	UVsNearlyEqual(*this, TEXT("LOD1 unchanged by default"), ReadUvChannel(Mesh, 1, 0), Baseline1);

	Request.bApplyOtherLods = true;
	TestEqual(TEXT("Apply 100 other LODs"), static_cast<int32>(ApplyUVPreset(Mesh, EPBRUVPreset::Scale100, Request, &Error)), static_cast<int32>(EPBRUVStatus::Success));
	UVsNearlyEqual(*this, TEXT("LOD1 after sync 100"), ReadUvChannel(Mesh, 1, 0), ScaleUVs(Baseline1, 100.0f));

	TestEqual(TEXT("Apply 500"), static_cast<int32>(ApplyUVPreset(Mesh, EPBRUVPreset::Scale500, Request, &Error)), static_cast<int32>(EPBRUVStatus::Success));
	TestEqual(TEXT("Applied scale 500"), GetAppliedUVScale(Mesh), 500);
	UVsNearlyEqual(*this, TEXT("LOD0 after 500"), ReadUvChannel(Mesh, 0, 0), ScaleUVs(Baseline0, 500.0f));
	UVsNearlyEqual(*this, TEXT("LOD1 after 500"), ReadUvChannel(Mesh, 1, 0), ScaleUVs(Baseline1, 500.0f));

	TestEqual(TEXT("Apply 100 again"), static_cast<int32>(ApplyUVPreset(Mesh, EPBRUVPreset::Scale100, Request, &Error)), static_cast<int32>(EPBRUVStatus::Success));
	UVsNearlyEqual(*this, TEXT("LOD0 back to 100"), ReadUvChannel(Mesh, 0, 0), ScaleUVs(Baseline0, 100.0f));
	UVsNearlyEqual(*this, TEXT("Lightmap UV1 untouched"), ReadUvChannel(Mesh, 0, 1), Lightmap0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabUVUndoRedo,
	"PBRTextureLab.UV.UndoRedo",
	UVTestFlags)

bool FPBRTextureLabUVUndoRedo::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	const TArray<FVector2f, TInlineAllocator<4>> Uv0 = {
		FVector2f(0.0f, 0.0f), FVector2f(1.0f, 0.0f), FVector2f(1.0f, 1.0f), FVector2f(0.0f, 1.0f)
	};
	const TArray<FVector2f, TInlineAllocator<4>> Uv1 = {
		FVector2f(0.0f, 0.0f), FVector2f(0.5f, 0.0f), FVector2f(0.5f, 0.5f), FVector2f(0.0f, 0.5f)
	};

	TArray<FMeshDescription> Lods;
	Lods.Add(MakeUvTestQuad(Uv0, Uv1));
	UStaticMesh* Mesh = CreateUvTestMesh(GetTransientPackage(), TEXT("T5Undo"), Lods);
	const TArray<FVector2f> Baseline = ReadUvChannel(Mesh, 0, 0);

	FPBRUVScaleRequest Request;
	FString Error;
	TestEqual(TEXT("Apply 100"), static_cast<int32>(ApplyUVPreset(Mesh, EPBRUVPreset::Scale100, Request, &Error)), static_cast<int32>(EPBRUVStatus::Success));
	TestEqual(TEXT("Apply 500"), static_cast<int32>(ApplyUVPreset(Mesh, EPBRUVPreset::Scale500, Request, &Error)), static_cast<int32>(EPBRUVStatus::Success));
	TestTrue(TEXT("Editor available"), GEditor != nullptr);
	TestTrue(TEXT("Undo to 100"), GEditor->UndoTransaction());
	TestEqual(TEXT("Scale after undo"), GetAppliedUVScale(Mesh), 100);
	UVsNearlyEqual(*this, TEXT("UV after undo"), ReadUvChannel(Mesh, 0, 0), ScaleUVs(Baseline, 100.0f));
	TestTrue(TEXT("Redo to 500"), GEditor->RedoTransaction());
	TestEqual(TEXT("Scale after redo"), GetAppliedUVScale(Mesh), 500);
	UVsNearlyEqual(*this, TEXT("UV after redo"), ReadUvChannel(Mesh, 0, 0), ScaleUVs(Baseline, 500.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabUVCancelAndUnsupported,
	"PBRTextureLab.UV.CancelAndUnsupported",
	UVTestFlags)

bool FPBRTextureLabUVCancelAndUnsupported::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	const TArray<FVector2f, TInlineAllocator<4>> Uv0 = {
		FVector2f(0.0f, 0.0f), FVector2f(1.0f, 0.0f), FVector2f(1.0f, 1.0f), FVector2f(0.0f, 1.0f)
	};
	const TArray<FVector2f, TInlineAllocator<4>> Uv1 = {
		FVector2f(0.2f, 0.2f), FVector2f(0.8f, 0.2f), FVector2f(0.8f, 0.8f), FVector2f(0.2f, 0.8f)
	};

	TArray<FMeshDescription> Lods;
	Lods.Add(MakeUvTestQuad(Uv0, Uv1));
	UStaticMesh* Mesh = CreateUvTestMesh(GetTransientPackage(), TEXT("T5Cancel"), Lods);
	const TArray<FVector2f> Before = ReadUvChannel(Mesh, 0, 0);

	FPBRUVScaleRequest Cancelled;
	Cancelled.bCancelled = true;
	FString Error;
	UStaticMesh* CancelledMesh = Mesh;
	TestEqual(
		TEXT("Explicit cancel"),
		static_cast<int32>(ApplyUVPreset(CancelledMesh, EPBRUVPreset::Scale100, Cancelled, &Error)),
		static_cast<int32>(EPBRUVStatus::Cancelled));
	UVsNearlyEqual(*this, TEXT("Cancel leaves UV"), ReadUvChannel(Mesh, 0, 0), Before);

	AddExpectedMessagePlain(
		TEXT("ApplyUVPreset requires a Static Mesh"),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);
	TestEqual(
		TEXT("Null mesh"),
		static_cast<int32>(ApplyUVPreset(nullptr, EPBRUVPreset::Scale100, FPBRUVScaleRequest(), &Error)),
		static_cast<int32>(EPBRUVStatus::Unsupported));

	UStaticMesh* Empty = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("T5Empty"), RF_Transactional);
	AddExpectedMessagePlain(
		TEXT("Static Mesh has no source LODs"),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);
	TestEqual(
		TEXT("Empty mesh"),
		static_cast<int32>(ApplyUVPreset(Empty, EPBRUVPreset::Scale100, FPBRUVScaleRequest(), &Error)),
		static_cast<int32>(EPBRUVStatus::Unsupported));

	Mesh->SetLightMapCoordinateIndex(0);
	AddExpectedMessagePlain(
		TEXT("Lightmap coordinate index is UV0"),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);
	TestEqual(
		TEXT("Lightmap is UV0 still scales"),
		static_cast<int32>(ApplyUVPreset(Mesh, EPBRUVPreset::Scale100, FPBRUVScaleRequest(), &Error)),
		static_cast<int32>(EPBRUVStatus::Success));
	UVsNearlyEqual(*this, TEXT("Lightmap UV0 scaled"), ReadUvChannel(Mesh, 0, 0), ScaleUVs(Before, 100.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabUVBaselineMismatch,
	"PBRTextureLab.UV.BaselineMismatch",
	UVTestFlags)

bool FPBRTextureLabUVBaselineMismatch::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	const TArray<FVector2f, TInlineAllocator<4>> Uv0 = {
		FVector2f(0.0f, 0.0f), FVector2f(0.5f, 0.0f), FVector2f(0.5f, 0.5f), FVector2f(0.0f, 0.5f)
	};
	const TArray<FVector2f, TInlineAllocator<4>> Uv1 = {
		FVector2f(0.0f, 0.0f), FVector2f(1.0f, 0.0f), FVector2f(1.0f, 1.0f), FVector2f(0.0f, 1.0f)
	};

	TArray<FMeshDescription> Lods;
	Lods.Add(MakeUvTestQuad(Uv0, Uv1));
	UStaticMesh* Mesh = CreateUvTestMesh(GetTransientPackage(), TEXT("T5Mismatch"), Lods);

	FPBRUVScaleRequest Request;
	FString Error;
	TestEqual(TEXT("Seed 100"), static_cast<int32>(ApplyUVPreset(Mesh, EPBRUVPreset::Scale100, Request, &Error)), static_cast<int32>(EPBRUVStatus::Success));
	PokeUv0(Mesh, 0, FVector2f(9.0f, 9.0f));

	AddExpectedMessagePlain(
		TEXT("UV0 changed; re-establish the UV baseline"),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);
	TestEqual(
		TEXT("External UV change"),
		static_cast<int32>(ApplyUVPreset(Mesh, EPBRUVPreset::Scale500, Request, &Error)),
		static_cast<int32>(EPBRUVStatus::BaselineMismatch));
	TestEqual(TEXT("Scale stays 100 after mismatch"), GetAppliedUVScale(Mesh), 100);

	Request.bRebaseline = true;
	TestEqual(
		TEXT("Rebaseline then 500"),
		static_cast<int32>(ApplyUVPreset(Mesh, EPBRUVPreset::Scale500, Request, &Error)),
		static_cast<int32>(EPBRUVStatus::Success));
	TestEqual(TEXT("Scale after rebaseline"), GetAppliedUVScale(Mesh), 500);
	const TArray<FVector2f> After = ReadUvChannel(Mesh, 0, 0);
	TestEqual(TEXT("Rebaselined first U"), After[0].X, 9.0f * 500.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPBRTextureLabUVReloadAfterRestart,
	"PBRTextureLab.UV.ReloadAfterRestart",
	UVTestFlags)

bool FPBRTextureLabUVReloadAfterRestart::RunTest(const FString& Parameters)
{
	using namespace PBRTextureLab;
	const FString AssetName = PersistMeshName();
	const FString ObjectPath = TEXT("/Game/PBRTextureLab/Automation/") + AssetName + TEXT(".") + AssetName;

	if (GetAppliedUVScale(LoadObject<UStaticMesh>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet)) != 100)
	{
		const TArray<FVector2f, TInlineAllocator<4>> Uv0 = {
			FVector2f(0.0f, 0.0f), FVector2f(0.25f, 0.0f), FVector2f(0.25f, 0.5f), FVector2f(0.0f, 0.5f)
		};
		const TArray<FVector2f, TInlineAllocator<4>> Uv1 = {
			FVector2f(0.0f, 0.0f), FVector2f(1.0f, 0.0f), FVector2f(1.0f, 1.0f), FVector2f(0.0f, 1.0f)
		};
		TArray<FMeshDescription> Lods;
		Lods.Add(MakeUvTestQuad(Uv0, Uv1));

		UPackage* Package = CreatePackage(*(TEXT("/Game/PBRTextureLab/Automation/") + AssetName));
		UStaticMesh* Created = CreateUvTestMesh(Package, FName(*AssetName), Lods);
		FAssetRegistryModule::AssetCreated(Created);
		FPBRUVScaleRequest Request;
		Request.bSave = true;
		FString Error;
		if (ApplyUVPreset(Created, EPBRUVPreset::Scale100, Request, &Error) != EPBRUVStatus::Success)
		{
			AddError(FString::Printf(TEXT("Failed to persist UV mesh: %s"), *Error));
			return false;
		}
	}

	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath);
	TestNotNull(TEXT("Reload UV mesh"), Mesh);
	if (!Mesh)
	{
		return false;
	}
	TestEqual(TEXT("Reloaded scale"), GetAppliedUVScale(Mesh), 100);
	const TArray<FVector2f> Reloaded = ReadUvChannel(Mesh, 0, 0);
	TestTrue(TEXT("Reloaded UV0 still scaled"), Reloaded.Num() == 4 && FMath::IsNearlyEqual(Reloaded[1].X, 25.0f));
	return true;
}

#endif
