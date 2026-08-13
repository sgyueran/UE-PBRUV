#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserDelegates.h"
#include "PBRTextureLabAssetBrowser.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class STextBlock;

class SPBRTextureLabAssetBrowser final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPBRTextureLabAssetBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeCategoryButton(EPBRTextureLabAssetCategory Category);
	FReply OnCategoryClicked(EPBRTextureLabAssetCategory Category);
	FReply OnSyncClicked();
	void RebuildAssetPicker();
	void HandleAssetSelected(const FAssetData& AssetData);
	void HandleAssetDoubleClicked(const FAssetData& AssetData);
	void HandleAssetsActivated(const TArray<FAssetData>& Assets, EAssetTypeActivationMethod::Type ActivationMethod);
	void UpdateStatusText();

	EPBRTextureLabAssetCategory ActiveCategory = EPBRTextureLabAssetCategory::All;
	TArray<FAssetData> SelectedAssets;
	TSharedPtr<SBox> AssetPickerHost;
	TSharedPtr<STextBlock> StatusText;
};
