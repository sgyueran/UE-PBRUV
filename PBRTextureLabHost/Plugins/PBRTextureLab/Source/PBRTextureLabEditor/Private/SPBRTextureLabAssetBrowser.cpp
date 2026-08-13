#include "SPBRTextureLabAssetBrowser.h"

#include "ContentBrowserModule.h"
#include "ContentBrowserDelegates.h"
#include "IContentBrowserSingleton.h"
#include "PBRTextureLabAssetBrowserUtils.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPBRTextureLabAssetBrowser"

void SPBRTextureLabAssetBrowser::Construct(const FArguments& InArgs)
{
	(void)InArgs;

	SAssignNew(AssetPickerHost, SBox);
	SAssignNew(StatusText, STextBlock)
		.Text(LOCTEXT("NoSelection", "No asset selected"))
		.ColorAndOpacity(FSlateColor::UseSubduedForeground());

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SScrollBox)
					.Orientation(Orient_Horizontal)
					+ SScrollBox::Slot()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()[MakeCategoryButton(EPBRTextureLabAssetCategory::All)]
						+ SHorizontalBox::Slot().AutoWidth()[MakeCategoryButton(EPBRTextureLabAssetCategory::Blueprints)]
						+ SHorizontalBox::Slot().AutoWidth()[MakeCategoryButton(EPBRTextureLabAssetCategory::BlueprintInterfaces)]
						+ SHorizontalBox::Slot().AutoWidth()[MakeCategoryButton(EPBRTextureLabAssetCategory::Materials)]
						+ SHorizontalBox::Slot().AutoWidth()[MakeCategoryButton(EPBRTextureLabAssetCategory::Models)]
						+ SHorizontalBox::Slot().AutoWidth()[MakeCategoryButton(EPBRTextureLabAssetCategory::Textures)]
						+ SHorizontalBox::Slot().AutoWidth()[MakeCategoryButton(EPBRTextureLabAssetCategory::Animations)]
						+ SHorizontalBox::Slot().AutoWidth()[MakeCategoryButton(EPBRTextureLabAssetCategory::Audio)]
						+ SHorizontalBox::Slot().AutoWidth()[MakeCategoryButton(EPBRTextureLabAssetCategory::Data)]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Sync", "Sync to Content Browser"))
					.OnClicked(this, &SPBRTextureLabAssetBrowser::OnSyncClicked)
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(4.0f)
		[
			AssetPickerHost.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f, 3.0f)
		[
			StatusText.ToSharedRef()
		]
	];

	RebuildAssetPicker();
}

TSharedRef<SWidget> SPBRTextureLabAssetBrowser::MakeCategoryButton(const EPBRTextureLabAssetCategory Category)
{
	return SNew(SButton)
		.ContentPadding(FMargin(8.0f, 4.0f))
		.Text(PBRTextureLabAssetBrowser::GetCategoryLabel(Category))
		.OnClicked(this, &SPBRTextureLabAssetBrowser::OnCategoryClicked, Category);
}

FReply SPBRTextureLabAssetBrowser::OnCategoryClicked(const EPBRTextureLabAssetCategory Category)
{
	ActiveCategory = Category;
	SelectedAssets.Reset();
	RebuildAssetPicker();
	return FReply::Handled();
}

FReply SPBRTextureLabAssetBrowser::OnSyncClicked()
{
	if (SelectedAssets.Num() > 0)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		ContentBrowserModule.Get().SyncBrowserToAssets(SelectedAssets, false, true);
	}
	return FReply::Handled();
}

void SPBRTextureLabAssetBrowser::RebuildAssetPicker()
{
	FAssetPickerConfig Config;
	Config.SelectionMode = ESelectionMode::Multi;
	Config.Filter = PBRTextureLabAssetBrowser::MakeFilter(ActiveCategory);
	Config.InitialAssetViewType = EAssetViewType::Tile;
	Config.InitialThumbnailSize = EThumbnailSize::Large;
	Config.ThumbnailLabel = EThumbnailLabel::ClassName;
	Config.bShowBottomToolbar = true;
	Config.bAddFilterUI = true;
	Config.bAllowDragging = true;
	Config.bAllowRename = false;
	Config.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SPBRTextureLabAssetBrowser::HandleAssetSelected);
	Config.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateSP(this, &SPBRTextureLabAssetBrowser::HandleAssetDoubleClicked);
	Config.OnAssetsActivated = FOnAssetsActivated::CreateSP(this, &SPBRTextureLabAssetBrowser::HandleAssetsActivated);
	Config.OnShouldFilterAsset = FOnShouldFilterAsset::CreateLambda(
		[this](const FAssetData& AssetData)
		{
			return PBRTextureLabAssetBrowser::ShouldFilterAsset(ActiveCategory, AssetData);
		});

	AssetPickerHost->SetContent(
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"))
			.Get()
			.CreateAssetPicker(Config));
	UpdateStatusText();
}

void SPBRTextureLabAssetBrowser::HandleAssetSelected(const FAssetData& AssetData)
{
	SelectedAssets.Reset();
	SelectedAssets.Add(AssetData);
	UpdateStatusText();
}

void SPBRTextureLabAssetBrowser::HandleAssetDoubleClicked(const FAssetData& AssetData)
{
	UPBRTextureLabAssetBrowserLibrary::OpenAssetByObjectPath(AssetData.GetObjectPathString());
}

void SPBRTextureLabAssetBrowser::HandleAssetsActivated(
	const TArray<FAssetData>& Assets,
	const EAssetTypeActivationMethod::Type ActivationMethod)
{
	(void)ActivationMethod;
	SelectedAssets = Assets;
	UpdateStatusText();
	if (Assets.Num() > 0)
	{
		UPBRTextureLabAssetBrowserLibrary::OpenAssetByObjectPath(Assets[0].GetObjectPathString());
	}
}

void SPBRTextureLabAssetBrowser::UpdateStatusText()
{
	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::Format(
			LOCTEXT("Status", "Category: {0}    Selected: {1}"),
			PBRTextureLabAssetBrowser::GetCategoryLabel(ActiveCategory),
			FText::AsNumber(SelectedAssets.Num())));
	}
}

#undef LOCTEXT_NAMESPACE
