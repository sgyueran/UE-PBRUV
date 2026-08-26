#include "SPBRTextureLabPreviewViewport.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Editor/UnrealEdEngine.h"
#include "Misc/App.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UnrealEdGlobals.h"

bool PBRTextureLabCanCreatePreviewViewport()
{
	return FApp::CanEverRender();
}

UStaticMesh* PBRTextureLabResolvePreviewMesh(const EPBRPreviewPrimitive Primitive)
{
	UThumbnailManager* ThumbnailManager = (GUnrealEd) ? GUnrealEd->GetThumbnailManager() : nullptr;
	UStaticMesh* Mesh = nullptr;
	const TCHAR* FallbackPath = TEXT("/Engine/EngineMeshes/Sphere.Sphere");
	switch (Primitive)
	{
	case EPBRPreviewPrimitive::Cube:
		Mesh = ThumbnailManager ? ThumbnailManager->EditorCube.Get() : nullptr;
		FallbackPath = TEXT("/Engine/EngineMeshes/Cube.Cube");
		break;
	case EPBRPreviewPrimitive::Plane:
		Mesh = ThumbnailManager ? ThumbnailManager->EditorPlane.Get() : nullptr;
		FallbackPath = TEXT("/Engine/BasicShapes/Plane.Plane");
		break;
	case EPBRPreviewPrimitive::Sphere:
	default:
		Mesh = ThumbnailManager ? ThumbnailManager->EditorSphere.Get() : nullptr;
		FallbackPath = TEXT("/Engine/EngineMeshes/Sphere.Sphere");
		break;
	}
	if (!Mesh)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, FallbackPath);
	}
	if (!Mesh && Primitive == EPBRPreviewPrimitive::Plane)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineMeshes/Plane.Plane"));
	}
	return Mesh;
}

void SPBRTextureLabPreviewViewport::Construct(const FArguments& InArgs)
{
	PreviewScene = MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues());
	SEditorViewport::Construct(SEditorViewport::FArguments());
	ApplyPreviewMesh();
	if (Client.IsValid())
	{
		Client->SetRealtime(true);
		Client->SetViewLocation(FVector(180.0f, 180.0f, 80.0f));
		Client->SetViewRotation(FRotator(-15.0f, -135.0f, 0.0f));
	}
}

SPBRTextureLabPreviewViewport::~SPBRTextureLabPreviewViewport()
{
	if (PreviewMeshComponent && PreviewScene)
	{
		PreviewScene->RemoveComponent(PreviewMeshComponent);
	}
	PreviewMeshComponent = nullptr;
	PreviewMaterial = nullptr;
}

void SPBRTextureLabPreviewViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PreviewMeshComponent);
	Collector.AddReferencedObject(PreviewMaterial);
}

FString SPBRTextureLabPreviewViewport::GetReferencerName() const
{
	return TEXT("SPBRTextureLabPreviewViewport");
}

TSharedRef<FEditorViewportClient> SPBRTextureLabPreviewViewport::MakeEditorViewportClient()
{
	TSharedRef<FEditorViewportClient> NewClient = MakeShared<FEditorViewportClient>(
		nullptr,
		PreviewScene.Get(),
		SharedThis(this));
	NewClient->SetRealtime(true);
	NewClient->bSetListenerPosition = false;
	NewClient->SetViewMode(VMI_Lit);
	NewClient->EngineShowFlags.SetGrid(false);
	return NewClient;
}

UStaticMesh* SPBRTextureLabPreviewViewport::ResolvePreviewMesh() const
{
	return PBRTextureLabResolvePreviewMesh(PreviewPrimitive);
}

void SPBRTextureLabPreviewViewport::ApplyPreviewMesh()
{
	if (!PreviewScene)
	{
		return;
	}

	UStaticMesh* Mesh = ResolvePreviewMesh();
	if (!Mesh)
	{
		return;
	}

	if (!PreviewMeshComponent)
	{
		PreviewMeshComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		PreviewMeshComponent->SetMobility(EComponentMobility::Movable);
		PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PreviewScene->AddComponent(PreviewMeshComponent, FTransform::Identity);
	}

	PreviewMeshComponent->SetStaticMesh(Mesh);
	FTransform Transform = FTransform::Identity;
	if (PreviewPrimitive == EPBRPreviewPrimitive::Plane)
	{
		Transform.SetRotation(FQuat(FRotator(0.0f, 180.0f, 0.0f)));
	}
	PreviewMeshComponent->SetRelativeTransform(Transform);
	if (PreviewMaterial)
	{
		PreviewMeshComponent->SetMaterial(0, PreviewMaterial);
	}
	if (Client.IsValid())
	{
		Client->Invalidate();
	}
}

void SPBRTextureLabPreviewViewport::SetPreviewMaterial(UMaterialInterface* Material)
{
	PreviewMaterial = Material;
	ApplyPreviewMesh();
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->SetMaterial(0, Material);
	}
	if (Client.IsValid())
	{
		Client->Invalidate();
	}
}

void SPBRTextureLabPreviewViewport::SetPreviewPrimitive(const EPBRPreviewPrimitive Primitive)
{
	if (PreviewPrimitive == Primitive && PreviewMeshComponent)
	{
		return;
	}
	PreviewPrimitive = Primitive;
	ApplyPreviewMesh();
}
