#pragma once

#include "PBRTextureLabAssetBrowser.h"
#include "AssetRegistry/ARFilter.h"

namespace PBRTextureLabAssetBrowser
{
	FARFilter MakeFilter(EPBRTextureLabAssetCategory Category);
	bool ShouldFilterAsset(EPBRTextureLabAssetCategory Category, const FAssetData& AssetData);
	FText GetCategoryLabel(EPBRTextureLabAssetCategory Category);
}
