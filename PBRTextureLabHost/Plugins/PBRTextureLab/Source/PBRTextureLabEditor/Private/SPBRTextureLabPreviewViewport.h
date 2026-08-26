#pragma once

#include "EditorViewportClient.h"
#include "PreviewScene.h"
#include "SEditorViewport.h"
#include "UObject/GCObject.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

enum class EPBRPreviewPrimitive : uint8
{
	Sphere = 0,
	Cube = 1,
	Plane = 2
};

/** Engine thumbnail mesh, then EngineMeshes/BasicShapes fallbacks. Safe without a viewport. */
UStaticMesh* PBRTextureLabResolvePreviewMesh(EPBRPreviewPrimitive Primitive);

/** 3D preview needs a real RHI. -NullRHI / commandlets must not construct SEditorViewport. */
bool PBRTextureLabCanCreatePreviewViewport();

class SPBRTextureLabPreviewViewport final : public SEditorViewport, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SPBRTextureLabPreviewViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SPBRTextureLabPreviewViewport() override;

	void SetPreviewMaterial(UMaterialInterface* Material);
	void SetPreviewPrimitive(EPBRPreviewPrimitive Primitive);
	EPBRPreviewPrimitive GetPreviewPrimitive() const { return PreviewPrimitive; }

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

private:
	UStaticMesh* ResolvePreviewMesh() const;
	void ApplyPreviewMesh();

	TUniquePtr<FPreviewScene> PreviewScene;
	TObjectPtr<UStaticMeshComponent> PreviewMeshComponent;
	TObjectPtr<UMaterialInterface> PreviewMaterial;
	EPBRPreviewPrimitive PreviewPrimitive = EPBRPreviewPrimitive::Sphere;
};
