#pragma once

#include "PBRTextureLabCommand.h"
#include "PBRTextureLabPipeline.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "Widgets/SCompoundWidget.h"

class UTexture2D;
class UMaterialInterface;
struct FAssetData;

class SPBRTextureLabNomad final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPBRTextureLabNomad) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildModeTabs();
	TSharedRef<SWidget> BuildGeneratePage();
	TSharedRef<SWidget> BuildAssemblePage();
	TSharedRef<SWidget> BuildSourceSection();
	TSharedRef<SWidget> BuildOutputSection();
	TSharedRef<SWidget> BuildParameterSection();
	TSharedRef<SWidget> BuildPreviewSection();
	TSharedRef<SWidget> BuildActionSection();
	TSharedRef<SWidget> BuildUvSection();
	TSharedRef<SWidget> MakeAssembleTextureRow(
		const FText& Label,
		TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot);
	TSharedRef<SWidget> MakePreviewTile(
		const FText& Label,
		TSharedPtr<FSlateDynamicImageBrush> SPBRTextureLabNomad::* BrushMember) const;
	TSharedRef<SWidget> MakeFloatRow(const FText& Label, TAttribute<float> Value, TFunction<void(float)> Setter, float MinValue, float MaxValue);
	TSharedRef<SWidget> MakeIntRow(const FText& Label, TAttribute<int32> Value, TFunction<void(int32)> Setter, int32 MinValue, int32 MaxValue);
	TSharedRef<SWidget> MakeCheckRow(const FText& Label, TAttribute<ECheckBoxState> IsChecked, TFunction<void(ECheckBoxState)> Setter);
	void ApplySourceName(const FString& SourceName);
	void AdvanceNamesAfterGenerate();

	FString GetSourceTexturePath() const;
	FString GetParentMaterialPath() const;
	void OnSourceTextureChanged(const FAssetData& AssetData);
	void OnParentMaterialChanged(const FAssetData& AssetData);
	void OnAssembleTextureChanged(const FAssetData& AssetData, TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot);
	FString GetAssembleTexturePath(TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot) const;
	FReply OnBrowseLocalImage();
	FReply OnUseContentBrowserTexture();
	FReply OnRefreshPreview();
	FReply OnGenerate();
	FReply OnCreateFromExisting();
	FReply OnUvScale100();
	FReply OnUvScale500();
	FReply SwitchToGeneratePage();
	FReply SwitchToAssemblePage();

	bool LoadCurrentSource(PBRTextureLab::FPBRImageRgba8& OutImage, FString* OutError) const;
	PBRTextureLab::FPBRGenerateRequest MakeRequest() const;
	void SetStatus(const FString& Message);
	void ClearPreview();
	void ApplyPreviewMaps(const PBRTextureLab::FPBRMaps& Maps, const PBRTextureLab::FPBRImageRgba8& Source);
	void MakePreviewBrush(const PBRTextureLab::FPBRImageRgba8& Image, const TCHAR* Label, TSharedPtr<FSlateDynamicImageBrush>& OutBrush);
	FText GetUvSelectionText() const;

	int32 ActivePage = 0;
	bool bCopyExistingTextures = true;
	TWeakObjectPtr<UTexture2D> SourceTexture;
	TWeakObjectPtr<UMaterialInterface> ParentMaterial;
	TWeakObjectPtr<UTexture2D> AssembleBaseColor;
	TWeakObjectPtr<UTexture2D> AssembleNormal;
	TWeakObjectPtr<UTexture2D> AssembleRoughness;
	TWeakObjectPtr<UTexture2D> AssembleMetallic;
	TWeakObjectPtr<UTexture2D> AssembleHeight;
	TWeakObjectPtr<UTexture2D> AssembleAO;
	TWeakObjectPtr<UTexture2D> AssembleORM;
	FString LocalImagePath;
	FString DestinationPath = TEXT("/Game/PBRTextureLab");
	FString BaseName = TEXT("PBR");
	FString MaterialInstanceName = TEXT("PBR_Inst");
	bool bInstanceNameCustomized = false;
	int32 OutputWidth = 0;
	int32 OutputHeight = 0;
	PBRTextureLab::FPBRPixelParams PixelParams;
	float MaterialHeightAmount = 0.0f;
	float MaterialUVScale = 1.0f;
	PBRTextureLab::EPBRImportConflictPolicy ConflictPolicy = PBRTextureLab::EPBRImportConflictPolicy::UniqueName;
	PBRTextureLab::FPBRMapExportFlags ExportFlags;

	TArray<TSharedPtr<FString>> MetallicOptions;
	TSharedPtr<FString> SelectedMetallic;
	TArray<TSharedPtr<FString>> ConflictOptions;
	TSharedPtr<FString> SelectedConflict;

	TSharedPtr<FSlateDynamicImageBrush> SourceBrush;
	TSharedPtr<FSlateDynamicImageBrush> BaseColorBrush;
	TSharedPtr<FSlateDynamicImageBrush> NormalBrush;
	TSharedPtr<FSlateDynamicImageBrush> RoughnessBrush;
	TSharedPtr<FSlateDynamicImageBrush> MetallicBrush;
	TSharedPtr<FSlateDynamicImageBrush> OrmBrush;

	FString StatusMessage;
	FString Disclaimer;
	bool bBusy = false;
	uint32 GenerationToken = 0;
	PBRTextureLab::FPBRMaps PreviewMaps;
	bool bHasPreview = false;
};
