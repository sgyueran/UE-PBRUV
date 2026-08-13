#include "PBRTextureLabEditorApi.h"

/**
 * Compile-time probes against the Task 1 API inventory.
 * These functions are never invoked at runtime.
 */
namespace PBRTextureLab::ApiProbe
{
	using FGetMeshDescription = FMeshDescription* (UStaticMesh::*)(int32) const;
	using FCommitMeshDescription = void (UStaticMesh::*)(int32, const UStaticMesh::FCommitMeshDescriptionParams&);
	using FGetNumSourceModels = int32 (UStaticMesh::*)() const;
	using FGetLightMapCoordinateIndex = int32 (UStaticMesh::*)() const;
	using FImportAssetTasks = void (IAssetTools::*)(const TArray<UAssetImportTask*>&);
	using FSetTextureParameter = void (UMaterialInstanceConstant::*)(const FMaterialParameterInfo&, UTexture*);
	using FSetScalarParameter = void (UMaterialInstanceConstant::*)(const FMaterialParameterInfo&, float);
	using FGetSelectedActors = USelection* (UEditorEngine::*)() const;
	using FGetContentBrowserSelections = void (UEditorEngine::*)(TArray<FAssetData>&) const;
	using FGetSelectedObject = UObject* (USelection::*)(const int32) const;
	using FToolMenusGet = UToolMenus* (*)();
	using FToolMenusExtend = UToolMenu* (UToolMenus::*)(const FName);

	void BindVerifiedApis()
	{
		const FGetMeshDescription GetMeshDescription = &UStaticMesh::GetMeshDescription;
		const FCommitMeshDescription CommitMeshDescription = &UStaticMesh::CommitMeshDescription;
		const FGetNumSourceModels GetNumSourceModels = &UStaticMesh::GetNumSourceModels;
		const FGetLightMapCoordinateIndex GetLightMapCoordinateIndex = &UStaticMesh::GetLightMapCoordinateIndex;
		const FImportAssetTasks ImportAssetTasksFn = &IAssetTools::ImportAssetTasks;
		const FSetTextureParameter SetTextureParameter = &UMaterialInstanceConstant::SetTextureParameterValueEditorOnly;
		const FSetScalarParameter SetScalarParameter = &UMaterialInstanceConstant::SetScalarParameterValueEditorOnly;
		const FGetSelectedActors GetSelectedActorsFn = &UEditorEngine::GetSelectedActors;
		const FGetContentBrowserSelections GetContentBrowserSelectionsFn = &UEditorEngine::GetContentBrowserSelections;
		const FGetSelectedObject GetSelectedObject = &USelection::GetSelectedObject;
		const FToolMenusGet ToolMenusGet = &UToolMenus::Get;
		const FToolMenusExtend ToolMenusExtend = &UToolMenus::ExtendMenu;

		(void)GetMeshDescription;
		(void)CommitMeshDescription;
		(void)GetNumSourceModels;
		(void)GetLightMapCoordinateIndex;
		(void)ImportAssetTasksFn;
		(void)SetTextureParameter;
		(void)SetScalarParameter;
		(void)GetSelectedActorsFn;
		(void)GetContentBrowserSelectionsFn;
		(void)GetSelectedObject;
		(void)ToolMenusGet;
		(void)ToolMenusExtend;
		(void)&UAssetImportTask::GetObjects;
		(void)&UAssetImportTask::IsAsyncImportComplete;
		using FGetVertexInstanceUVs = TVertexInstanceAttributesRef<FVector2f> (FStaticMeshAttributes::*)();
		const FGetVertexInstanceUVs GetVertexInstanceUVs = &FStaticMeshAttributes::GetVertexInstanceUVs;
		(void)GetVertexInstanceUVs;
		(void)&FAssetToolsModule::GetModule;

#if PBRTEXTURELAB_HAS_MIC_DOUBLE_VECTOR_EDITORONLY
		using FSetDoubleVector = void (UMaterialInstanceConstant::*)(const FMaterialParameterInfo&, FVector4d);
		const FSetDoubleVector SetDoubleVector = &UMaterialInstanceConstant::SetDoubleVectorParameterValueEditorOnly;
		(void)SetDoubleVector;
#endif
#if PBRTEXTURELAB_HAS_MIC_PARAMETER_COLLECTION_EDITORONLY
		using FSetParameterCollection = void (UMaterialInstanceConstant::*)(const FMaterialParameterInfo&, UMaterialParameterCollection*);
		const FSetParameterCollection SetParameterCollection = &UMaterialInstanceConstant::SetParameterCollectionParameterValueEditorOnly;
		(void)SetParameterCollection;
#endif
	}

	class FProbeCommands final : public TCommands<FProbeCommands>
	{
	public:
		FProbeCommands()
			: TCommands<FProbeCommands>(
				TEXT("PBRTextureLabProbe"),
				NSLOCTEXT("PBRTextureLab", "ProbeCommands", "PBR Texture Lab API Probe"),
				NAME_None,
				TEXT("PBRTextureLabStyle"))
		{
		}

		virtual void RegisterCommands() override
		{
		}
	};
}
