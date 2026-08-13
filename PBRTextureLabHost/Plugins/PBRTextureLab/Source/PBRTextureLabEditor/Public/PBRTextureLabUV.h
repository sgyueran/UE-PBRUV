#pragma once

#include "CoreMinimal.h"
#include "PBRTextureLabCompat.h"

class UStaticMesh;

/**
 * Absolute UV0 scale presets for Static Mesh source LODs.
 * Does not modify Lightmap UV, other UV channels, Skeletal Mesh, or runtime meshes.
 * Must be called on the Game Thread.
 */
namespace PBRTextureLab
{
	enum class EPBRUVStatus : uint8
	{
		Success = 0,
		Cancelled = 1,
		Unsupported = 2,
		BaselineMismatch = 3,
		Failed = 4
	};

	enum class EPBRUVPreset : int32
	{
		Scale100 = PBRTEXTURELAB_UV_PRESET_100,
		Scale500 = PBRTEXTURELAB_UV_PRESET_500
	};

	struct FPBRUVScaleRequest
	{
		bool bCancelled = false;
		bool bRebaseline = false;
		bool bSave = false;
		bool bTransact = true;
	};

	int32 GetAppliedUVScale(const UStaticMesh* Mesh);

	EPBRUVStatus EstablishUVBaseline(UStaticMesh* Mesh, FString* OutError = nullptr);

	EPBRUVStatus ApplyUVPreset(
		UStaticMesh* Mesh,
		EPBRUVPreset Preset,
		const FPBRUVScaleRequest& Request,
		FString* OutError = nullptr);
}
