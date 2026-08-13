#pragma once

#include "PBRTextureLabCompat.h"

#include "AssetRegistry/AssetData.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Framework/Commands/Commands.h"
#include "IAssetTools.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "StaticMeshAttributes.h"
#include "ToolMenus.h"

class UTexture;

/**
 * Thin wrappers around Task 1 verified Editor APIs.
 * Later tasks must call these instead of scattering version checks.
 * This header does not generate assets, rewrite UVs, or register UI.
 */
namespace PBRTextureLab
{
	inline FMeshDescription* GetStaticMeshDescription(UStaticMesh* Mesh, int32 LodIndex)
	{
		return Mesh ? Mesh->GetMeshDescription(LodIndex) : nullptr;
	}

	inline void CommitStaticMeshDescription(UStaticMesh* Mesh, int32 LodIndex)
	{
		if (Mesh)
		{
			Mesh->CommitMeshDescription(LodIndex);
		}
	}

	inline bool ModifyStaticMeshDescription(UStaticMesh* Mesh, int32 LodIndex, const bool bAlwaysMarkDirty = true)
	{
		return Mesh ? Mesh->ModifyMeshDescription(LodIndex, bAlwaysMarkDirty) : false;
	}

	inline bool IsStaticMeshDescriptionValid(const UStaticMesh* Mesh, int32 LodIndex)
	{
		return Mesh && Mesh->IsMeshDescriptionValid(LodIndex);
	}

	inline int32 GetStaticMeshSourceModelCount(const UStaticMesh* Mesh)
	{
		return Mesh ? Mesh->GetNumSourceModels() : 0;
	}

	inline int32 GetLightMapUVChannel(const UStaticMesh* Mesh)
	{
		return Mesh ? Mesh->GetLightMapCoordinateIndex() : INDEX_NONE;
	}

	inline TVertexInstanceAttributesRef<FVector2f> GetVertexInstanceUVs(FMeshDescription& MeshDescription)
	{
		FStaticMeshAttributes Attributes(MeshDescription);
		return Attributes.GetVertexInstanceUVs();
	}

	inline IAssetTools& GetAssetTools()
	{
		return FAssetToolsModule::GetModule().Get();
	}

	inline void ImportAssetTasks(const TArray<UAssetImportTask*>& ImportTasks)
	{
		GetAssetTools().ImportAssetTasks(ImportTasks);
	}

	inline void SetMaterialInstanceTexture(
		UMaterialInstanceConstant* MaterialInstance,
		FName ParameterName,
		UTexture* Texture)
	{
		if (MaterialInstance)
		{
			MaterialInstance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(ParameterName), Texture);
		}
	}

	inline void SetMaterialInstanceScalar(
		UMaterialInstanceConstant* MaterialInstance,
		FName ParameterName,
		float Value)
	{
		if (MaterialInstance)
		{
			MaterialInstance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ParameterName), Value);
		}
	}

#if PBRTEXTURELAB_HAS_MIC_PARAMETER_COLLECTION_EDITORONLY
	inline void SetMaterialInstanceParameterCollection(
		UMaterialInstanceConstant* MaterialInstance,
		FName ParameterName,
		UMaterialParameterCollection* Collection)
	{
		if (MaterialInstance)
		{
			MaterialInstance->SetParameterCollectionParameterValueEditorOnly(
				FMaterialParameterInfo(ParameterName),
				Collection);
		}
	}
#endif

	inline USelection* GetSelectedActors()
	{
		return GEditor ? GEditor->GetSelectedActors() : nullptr;
	}

	inline USelection* GetSelectedComponents()
	{
		return GEditor ? GEditor->GetSelectedComponents() : nullptr;
	}

	inline USelection* GetSelectedObjects()
	{
		return GEditor ? GEditor->GetSelectedObjects() : nullptr;
	}

	inline void GetContentBrowserSelections(TArray<FAssetData>& OutSelections)
	{
		if (GEditor)
		{
			GEditor->GetContentBrowserSelections(OutSelections);
		}
	}

	inline UToolMenus* GetToolMenus()
	{
		return UToolMenus::Get();
	}

	inline FScopedTransaction MakeTransaction(const FText& SessionName, const bool bShouldActuallyTransact = true)
	{
		return FScopedTransaction(SessionName, bShouldActuallyTransact);
	}
}
