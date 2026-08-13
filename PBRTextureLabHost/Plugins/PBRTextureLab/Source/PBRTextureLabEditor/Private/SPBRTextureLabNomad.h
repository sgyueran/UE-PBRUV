#pragma once

#include "PBRTextureLabCommand.h"
#include "PBRTextureLabPipeline.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "Widgets/SCompoundWidget.h"

class UTexture2D;
struct FAssetData;

class SPBRTextureLabNomad final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPBRTextureLabNomad) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildSourceSection();
	TSharedRef<SWidget> BuildOutputSection();
	TSharedRef<SWidget> BuildParameterSection();
	TSharedRef<SWidget> BuildPreviewSection();
	TSharedRef<SWidget> BuildActionSection();
	TSharedRef<SWidget> BuildUvSection();
	TSharedRef<SWidget> MakePreviewTile(
		const FText& Label,
		TSharedPtr<FSlateDynamicImageBrush> SPBRTextureLabNomad::* BrushMember) const;
	TSharedRef<SWidget> MakeFloatRow(const FText& Label, TAttribute<float> Value, TFunction<void(float)> Setter, float MinValue, float MaxValue);
	TSharedRef<SWidget> MakeIntRow(const FText& Label, TAttribute<int32> Value, TFunction<void(int32)> Setter, int32 MinValue, int32 MaxValue);

	FString GetSourceTexturePath() const;
	void OnSourceTextureChanged(const FAssetData& AssetData);
	FReply OnBrowseLocalImage();
	FReply OnUseContentBrowserTexture();
	FReply OnRefreshPreview();
	FReply OnGenerate();
	FReply OnUvScale100();
	FReply OnUvScale500();

	bool LoadCurrentSource(PBRTextureLab::FPBRImageRgba8& OutImage, FString* OutError) const;
	PBRTextureLab::FPBRGenerateRequest MakeRequest() const;
	void SetStatus(const FString& Message);
	void ClearPreview();
	void ApplyPreviewMaps(const PBRTextureLab::FPBRMaps& Maps, const PBRTextureLab::FPBRImageRgba8& Source);
	void MakePreviewBrush(const PBRTextureLab::FPBRImageRgba8& Image, const TCHAR* Label, TSharedPtr<FSlateDynamicImageBrush>& OutBrush);
	FText GetUvSelectionText() const;

	TWeakObjectPtr<UTexture2D> SourceTexture;
	FString LocalImagePath;
	FString DestinationPath = TEXT("/Game/PBRTextureLab");
	FString BaseName = TEXT("PBR");
	int32 OutputWidth = 0;
	int32 OutputHeight = 0;
	PBRTextureLab::FPBRPixelParams PixelParams;
	float MaterialHeightAmount = 0.0f;
	float MaterialUVScale = 1.0f;
	PBRTextureLab::EPBRImportConflictPolicy ConflictPolicy = PBRTextureLab::EPBRImportConflictPolicy::Cancel;

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
