#pragma once

#include "PBRTextureLabCommand.h"
#include "PBRTextureLabPipeline.h"
#include "SPBRTextureLabPreviewViewport.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "Widgets/SCompoundWidget.h"

class UTexture2D;
class UMaterialInterface;
class UMaterialInstanceConstant;
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
	TSharedRef<SWidget> BuildParentReuseSection();
	void TrySelectDefaultUserParent();
	void RebuildBundledParentOptions();
	void SelectBundledParent(const FString& DisplayName);
	TSharedRef<SWidget> BuildPreviewSection();
	TSharedRef<SWidget> BuildPreviewPane();
	TSharedRef<SWidget> BuildActionSection();
	TSharedRef<SWidget> BuildUvSection();
	TSharedRef<SWidget> MakeAssembleTextureRow(
		TAttribute<FText> Label,
		TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot,
		PBRTextureLab::EPBRMapKind Kind);
	TSharedRef<SWidget> MakePreviewTile(
		TAttribute<FText> Label,
		TSharedPtr<FSlateDynamicImageBrush> SPBRTextureLabNomad::* BrushMember,
		TAttribute<EVisibility> Visibility = EVisibility::Visible) const;
	TSharedRef<SWidget> MakePreviewPrimitiveButton(const FText& Label, EPBRPreviewPrimitive Primitive);
	TSharedRef<SWidget> MakeFloatRow(TAttribute<FText> Label, TAttribute<float> Value, TFunction<void(float)> Setter, float MinValue, float MaxValue);
	TSharedRef<SWidget> MakeIntRow(TAttribute<FText> Label, TAttribute<int32> Value, TFunction<void(int32)> Setter, int32 MinValue, int32 MaxValue);
	TSharedRef<SWidget> MakeCheckRow(TAttribute<FText> Label, TAttribute<ECheckBoxState> IsChecked, TFunction<void(ECheckBoxState)> Setter);
	void RefreshParentTitles();
	FText GetParentParamLabel(PBRTextureLab::EPBRParentParamSlot Slot) const;
	TAttribute<FText> ParentLabel(PBRTextureLab::EPBRParentParamSlot Slot) const;
	void ApplySourceName(const FString& SourceName);
	void AdvanceNamesAfterGenerate();

	FString GetSourceTexturePath() const;
	FString GetParentMaterialPath() const;
	FString GetModifyTargetPath() const;
	void OnSourceTextureChanged(const FAssetData& AssetData);
	void OnParentMaterialChanged(const FAssetData& AssetData);
	void OnModifyTargetChanged(const FAssetData& AssetData);
	void SetModifyMode(bool bEnable);
	void OnAssembleTextureChanged(const FAssetData& AssetData, TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot);
	FString GetAssembleTexturePath(TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot) const;
	FReply OnBrowseLocalImage();
	FReply OnBrowseAssembleLocalImage(PBRTextureLab::EPBRMapKind Kind, TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot);
	FReply OnBrowseAssembleFolder();
	FReply OnUseContentBrowserTexture();
	void AssignAssembleTexture(TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot, UTexture2D* Texture);
	FReply OnRefreshPreview();
	FReply OnGenerate();
	FReply OnCreateFromExisting();
	FReply OnUvScale100();
	FReply OnUvScale500();
	FReply DispatchUvScale(PBRTextureLab::EPBRUVPreset Preset, const TCHAR* Label);
	FReply OnAssignLastMaterial();
	FReply SwitchToGeneratePage();
	FReply SwitchToAssemblePage();
	void HandleGenerateSuccess(const PBRTextureLab::FPBRGenerateResult& Result);

	bool LoadCurrentSource(PBRTextureLab::FPBRImageRgba8& OutImage, FString* OutError) const;
	PBRTextureLab::FPBRGenerateRequest MakeRequest() const;
	void SetStatus(const FString& Message);
	void ClearPreview();
	void ApplyPreviewMaps(const PBRTextureLab::FPBRMaps& Maps, const PBRTextureLab::FPBRImageRgba8& Source);
	void MakePreviewBrush(const PBRTextureLab::FPBRImageRgba8& Image, const TCHAR* Label, TSharedPtr<FSlateDynamicImageBrush>& OutBrush);
	FText GetUvSelectionText() const;

	int32 ActivePage = 0;
	bool bCopyExistingTextures = true;
	bool bSyncBrowserAfterGenerate = true;
	bool bMakeSeamless = true;
	bool bApplyOtherLods = false;
	bool bModifyExisting = false;
	TWeakObjectPtr<UTexture2D> SourceTexture;
	TWeakObjectPtr<UMaterialInterface> ParentMaterial;
	TWeakObjectPtr<UMaterialInterface> LastGeneratedMaterial;
	TWeakObjectPtr<UMaterialInstanceConstant> ModifyTarget;
	FString LastGeneratedFolder;
	TWeakObjectPtr<UTexture2D> AssembleBaseColor;
	TWeakObjectPtr<UTexture2D> AssembleNormal;
	TWeakObjectPtr<UTexture2D> AssembleRoughness;
	TWeakObjectPtr<UTexture2D> AssembleMetallic;
	TWeakObjectPtr<UTexture2D> AssembleHeight;
	TWeakObjectPtr<UTexture2D> AssembleAO;
	FString LocalImagePath;
	FString LastLocalFolder;
	FString DestinationPath = TEXT("/Game/PBRTextureLab");
	FString BaseName = TEXT("PBR");
	FString MaterialInstanceName = TEXT("PBR_Inst");
	bool bInstanceNameCustomized = false;
	int32 OutputWidth = 0;
	int32 OutputHeight = 0;
	PBRTextureLab::FPBRPixelParams PixelParams;
	float MaterialRoughnessStrength = 1.0f;
	float MaterialMetallicStrength = 1.0f;
	float MaterialHeightAmount = 0.0f;
	float MaterialUVScale = 1.0f;
	PBRTextureLab::EPBRImportConflictPolicy ConflictPolicy = PBRTextureLab::EPBRImportConflictPolicy::UniqueName;
	PBRTextureLab::FPBRMapExportFlags ExportFlags;
	PBRTextureLab::FPBRParentParamTitles CachedParentTitles;

	TArray<TSharedPtr<FString>> MetallicOptions;
	TSharedPtr<FString> SelectedMetallic;
	TArray<TSharedPtr<FString>> ConflictOptions;
	TSharedPtr<FString> SelectedConflict;
	TArray<TSharedPtr<FString>> OutputModeOptions;
	TSharedPtr<FString> SelectedOutputMode;
	TArray<TSharedPtr<FString>> ParentOptions;
	TSharedPtr<FString> SelectedParentOption;

	TSharedPtr<FSlateDynamicImageBrush> SourceBrush;
	TSharedPtr<FSlateDynamicImageBrush> BaseColorBrush;
	TSharedPtr<FSlateDynamicImageBrush> NormalBrush;
	TSharedPtr<FSlateDynamicImageBrush> RoughnessBrush;
	TSharedPtr<FSlateDynamicImageBrush> MetallicBrush;
	TSharedPtr<FSlateDynamicImageBrush> AoBrush;
	TSharedPtr<SPBRTextureLabPreviewViewport> PreviewViewport;

	FString StatusMessage;
	FString Disclaimer;
	bool bBusy = false;
	uint32 GenerationToken = 0;
	PBRTextureLab::FPBRMaps PreviewMaps;
	bool bHasPreview = false;
};
