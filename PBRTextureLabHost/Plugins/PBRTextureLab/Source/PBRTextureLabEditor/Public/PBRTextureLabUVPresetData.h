#pragma once

#include "Engine/AssetUserData.h"
#include "PBRTextureLabUVPresetData.generated.h"

USTRUCT()
struct FPBRTextureLabUVLodRecord
{
	GENERATED_BODY()

	UPROPERTY()
	int32 LodIndex = 0;

	UPROPERTY()
	uint64 TopologyFingerprint = 0;

	UPROPERTY()
	uint64 BaselineUvFingerprint = 0;

	UPROPERTY()
	TArray<FVector2f> BaselineUVs;
};

/**
 * Per-asset UV0 absolute-preset state stored on the Static Mesh.
 * Baseline UVs are scale 1 so 100x and 500x do not accumulate.
 */
UCLASS()
class UPBRTextureLabUVPresetData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 AppliedScale = 0;

	UPROPERTY()
	TArray<FPBRTextureLabUVLodRecord> Lods;
};
