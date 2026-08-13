#include "SPBRTextureLabNomad.h"

#include "PBRTextureLabEditorApi.h"
#include "PBRTextureLabPixelCore.h"

#include "AssetRegistry/AssetData.h"
#include "DesktopPlatformModule.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPBRTextureLabNomad"

namespace
{
	TSharedRef<SWidget> NomadSectionLabel(const FText& Text)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"));
	}

	PBRTextureLab::EPBRMetallicMode NomadMetallicFromLabel(const FString& Label)
	{
		if (Label.Contains(TEXT("Constant")))
		{
			return PBRTextureLab::EPBRMetallicMode::Constant;
		}
		if (Label.Contains(TEXT("Threshold")))
		{
			return PBRTextureLab::EPBRMetallicMode::ThresholdMask;
		}
		return PBRTextureLab::EPBRMetallicMode::AllBlack;
	}

	PBRTextureLab::EPBRImportConflictPolicy NomadConflictFromLabel(const FString& Label)
	{
		if (Label.Contains(TEXT("Replace")))
		{
			return PBRTextureLab::EPBRImportConflictPolicy::Replace;
		}
		if (Label.Contains(TEXT("Unique")))
		{
			return PBRTextureLab::EPBRImportConflictPolicy::UniqueName;
		}
		return PBRTextureLab::EPBRImportConflictPolicy::Cancel;
	}
}

void SPBRTextureLabNomad::Construct(const FArguments& InArgs)
{
	MetallicOptions.Add(MakeShared<FString>(TEXT("All Black")));
	MetallicOptions.Add(MakeShared<FString>(TEXT("Constant")));
	MetallicOptions.Add(MakeShared<FString>(TEXT("Threshold Mask")));
	SelectedMetallic = MetallicOptions[0];

	ConflictOptions.Add(MakeShared<FString>(TEXT("Cancel")));
	ConflictOptions.Add(MakeShared<FString>(TEXT("Replace")));
	ConflictOptions.Add(MakeShared<FString>(TEXT("Unique Name")));
	SelectedConflict = ConflictOptions[0];

	StatusMessage = TEXT("Select a Texture2D or a local image, then Preview or Generate.");
	Disclaimer = PBRTextureLab::MetallicDisclaimer;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[BuildSourceSection()]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildOutputSection()]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildParameterSection()]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildPreviewSection()]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildActionSection()]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildUvSection()]
			]
		]
	];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildSourceSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			NomadSectionLabel(LOCTEXT("SourceHeading", "Source"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(UTexture2D::StaticClass())
			.AllowClear(true)
			.DisplayThumbnail(true)
			.ObjectPath(this, &SPBRTextureLabNomad::GetSourceTexturePath)
			.OnObjectChanged(this, &SPBRTextureLabNomad::OnSourceTextureChanged)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("UseSelected", "Use Content Browser Texture2D"))
				.OnClicked(this, &SPBRTextureLabNomad::OnUseContentBrowserTexture)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("BrowseLocal", "Browse Local Image..."))
				.OnClicked(this, &SPBRTextureLabNomad::OnBrowseLocalImage)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text_Lambda([this]()
			{
				if (!LocalImagePath.IsEmpty())
				{
					return FText::Format(
						LOCTEXT("LocalPathFmt", "Local file: {0}"),
						FText::FromString(LocalImagePath));
				}
				if (SourceTexture.IsValid())
				{
					return FText::Format(
						LOCTEXT("AssetPathFmt", "Texture2D: {0}"),
						FText::FromString(SourceTexture->GetPathName()));
				}
				return LOCTEXT("NoSource", "No source selected.");
			})
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildOutputSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			NomadSectionLabel(LOCTEXT("OutputHeading", "Output"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("DestLabel", "Destination"))]
			+ SHorizontalBox::Slot().FillWidth(0.75f)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([this]() { return FText::FromString(DestinationPath); })
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
				{
					DestinationPath = Text.ToString();
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("NameLabel", "Base Name"))]
			+ SHorizontalBox::Slot().FillWidth(0.75f)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([this]() { return FText::FromString(BaseName); })
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
				{
					BaseName = Text.ToString();
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("SizeLabel", "Size (0 = source)"))]
			+ SHorizontalBox::Slot().FillWidth(0.375f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SSpinBox<int32>)
				.MinValue(0)
				.MaxValue(PBRTextureLab::MaxOutputDimension)
				.Value_Lambda([this]() { return OutputWidth; })
				.OnValueChanged_Lambda([this](int32 Value) { OutputWidth = Value; })
			]
			+ SHorizontalBox::Slot().FillWidth(0.375f)
			[
				SNew(SSpinBox<int32>)
				.MinValue(0)
				.MaxValue(PBRTextureLab::MaxOutputDimension)
				.Value_Lambda([this]() { return OutputHeight; })
				.OnValueChanged_Lambda([this](int32 Value) { OutputHeight = Value; })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("ConflictLabel", "Name Conflict"))]
			+ SHorizontalBox::Slot().FillWidth(0.75f)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&ConflictOptions)
				.InitiallySelectedItem(SelectedConflict)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewValue, ESelectInfo::Type)
				{
					SelectedConflict = NewValue;
					if (NewValue.IsValid())
					{
						ConflictPolicy = NomadConflictFromLabel(*NewValue);
					}
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
				{
					return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
				})
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return FText::FromString(SelectedConflict.IsValid() ? *SelectedConflict : FString());
					})
				]
			]
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildParameterSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			NomadSectionLabel(LOCTEXT("ParamsHeading", "Parameters"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				LOCTEXT("HeightContrast", "Height Contrast"),
				TAttribute<float>::CreateLambda([this]() { return PixelParams.HeightContrast; }),
				[this](float Value) { PixelParams.HeightContrast = Value; },
				0.0f,
				8.0f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeIntRow(
				LOCTEXT("HeightBlur", "Height Blur Radius"),
				TAttribute<int32>::CreateLambda([this]() { return PixelParams.HeightBlurRadius; }),
				[this](int32 Value) { PixelParams.HeightBlurRadius = Value; },
				0,
				16)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("InvertHeight", "Invert Height"))]
			+ SHorizontalBox::Slot().FillWidth(0.75f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]()
				{
					return PixelParams.bInvertHeight ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					PixelParams.bInvertHeight = State == ECheckBoxState::Checked;
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				LOCTEXT("NormalStrength", "Normal Strength"),
				TAttribute<float>::CreateLambda([this]() { return PixelParams.NormalStrength; }),
				[this](float Value) { PixelParams.NormalStrength = Value; },
				0.0f,
				8.0f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				LOCTEXT("RoughnessScale", "Roughness Scale"),
				TAttribute<float>::CreateLambda([this]() { return PixelParams.RoughnessScale; }),
				[this](float Value) { PixelParams.RoughnessScale = Value; },
				0.0f,
				8.0f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				LOCTEXT("RoughnessBias", "Roughness Bias"),
				TAttribute<float>::CreateLambda([this]() { return PixelParams.RoughnessBias; }),
				[this](float Value) { PixelParams.RoughnessBias = Value; },
				-1.0f,
				1.0f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("MetallicMode", "Metallic Mode"))]
			+ SHorizontalBox::Slot().FillWidth(0.75f)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&MetallicOptions)
				.InitiallySelectedItem(SelectedMetallic)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewValue, ESelectInfo::Type)
				{
					SelectedMetallic = NewValue;
					if (NewValue.IsValid())
					{
						PixelParams.MetallicMode = NomadMetallicFromLabel(*NewValue);
					}
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
				{
					return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
				})
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return FText::FromString(SelectedMetallic.IsValid() ? *SelectedMetallic : FString());
					})
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				LOCTEXT("MetallicConstant", "Metallic Constant"),
				TAttribute<float>::CreateLambda([this]() { return PixelParams.MetallicConstant; }),
				[this](float Value) { PixelParams.MetallicConstant = Value; },
				0.0f,
				1.0f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				LOCTEXT("MetallicThreshold", "Metallic Threshold"),
				TAttribute<float>::CreateLambda([this]() { return PixelParams.MetallicThreshold; }),
				[this](float Value) { PixelParams.MetallicThreshold = Value; },
				0.0f,
				1.0f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				LOCTEXT("HeightAmount", "Material Height Amount"),
				TAttribute<float>::CreateLambda([this]() { return MaterialHeightAmount; }),
				[this](float Value) { MaterialHeightAmount = Value; },
				0.0f,
				0.1f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				LOCTEXT("UvScale", "Material UV Scale"),
				TAttribute<float>::CreateLambda([this]() { return MaterialUVScale; }),
				[this](float Value) { MaterialUVScale = Value; },
				0.01f,
				64.0f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text_Lambda([this]() { return FText::FromString(Disclaimer); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(FText::FromString(PBRTextureLab::MetallicDisclaimerZh))
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildPreviewSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			NomadSectionLabel(LOCTEXT("PreviewHeading", "Preview"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SUniformGridPanel)
			.SlotPadding(4.0f)
			+ SUniformGridPanel::Slot(0, 0)[MakePreviewTile(LOCTEXT("PrevSource", "Source"), &SPBRTextureLabNomad::SourceBrush)]
			+ SUniformGridPanel::Slot(1, 0)[MakePreviewTile(LOCTEXT("PrevBase", "BaseColor"), &SPBRTextureLabNomad::BaseColorBrush)]
			+ SUniformGridPanel::Slot(2, 0)[MakePreviewTile(LOCTEXT("PrevNormal", "Normal"), &SPBRTextureLabNomad::NormalBrush)]
			+ SUniformGridPanel::Slot(0, 1)[MakePreviewTile(LOCTEXT("PrevRough", "Roughness"), &SPBRTextureLabNomad::RoughnessBrush)]
			+ SUniformGridPanel::Slot(1, 1)[MakePreviewTile(LOCTEXT("PrevMetal", "Metallic"), &SPBRTextureLabNomad::MetallicBrush)]
			+ SUniformGridPanel::Slot(2, 1)[MakePreviewTile(LOCTEXT("PrevOrm", "ORM"), &SPBRTextureLabNomad::OrmBrush)]
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildActionSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("PreviewBtn", "Preview"))
				.IsEnabled_Lambda([this]() { return !bBusy; })
				.OnClicked(this, &SPBRTextureLabNomad::OnRefreshPreview)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("GenerateBtn", "Generate Maps + Material"))
				.IsEnabled_Lambda([this]() { return !bBusy; })
				.OnClicked(this, &SPBRTextureLabNomad::OnGenerate)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text_Lambda([this]() { return FText::FromString(StatusMessage); })
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildUvSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			NomadSectionLabel(LOCTEXT("UvHeading", "UV Commands"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(this, &SPBRTextureLabNomad::GetUvSelectionText)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Uv100", "UV Scale 100x"))
				.OnClicked(this, &SPBRTextureLabNomad::OnUvScale100)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Uv500", "UV Scale 500x"))
				.OnClicked(this, &SPBRTextureLabNomad::OnUvScale500)
			]
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::MakePreviewTile(
	const FText& Label,
	TSharedPtr<FSlateDynamicImageBrush> SPBRTextureLabNomad::* BrushMember) const
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(Label)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox)
			.WidthOverride(96.0f)
			.HeightOverride(96.0f)
			[
				SNew(SImage)
				.Image_Lambda([this, BrushMember]() -> const FSlateBrush*
				{
					const TSharedPtr<FSlateDynamicImageBrush>& Brush = this->*BrushMember;
					return Brush.IsValid() ? static_cast<const FSlateBrush*>(Brush.Get()) : FAppStyle::Get().GetBrush("WhiteBrush");
				})
			]
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::MakeFloatRow(
	const FText& Label,
	TAttribute<float> Value,
	TFunction<void(float)> Setter,
	float MinValue,
	float MaxValue)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
		[SNew(STextBlock).Text(Label)]
		+ SHorizontalBox::Slot().FillWidth(0.75f)
		[
			SNew(SSpinBox<float>)
			.MinValue(MinValue)
			.MaxValue(MaxValue)
			.Value(Value)
			.OnValueChanged_Lambda([Setter](float NewValue) { Setter(NewValue); })
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::MakeIntRow(
	const FText& Label,
	TAttribute<int32> Value,
	TFunction<void(int32)> Setter,
	int32 MinValue,
	int32 MaxValue)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
		[SNew(STextBlock).Text(Label)]
		+ SHorizontalBox::Slot().FillWidth(0.75f)
		[
			SNew(SSpinBox<int32>)
			.MinValue(MinValue)
			.MaxValue(MaxValue)
			.Value(Value)
			.OnValueChanged_Lambda([Setter](int32 NewValue) { Setter(NewValue); })
		];
}

FString SPBRTextureLabNomad::GetSourceTexturePath() const
{
	return SourceTexture.IsValid() ? SourceTexture->GetPathName() : FString();
}

void SPBRTextureLabNomad::OnSourceTextureChanged(const FAssetData& AssetData)
{
	SourceTexture = Cast<UTexture2D>(AssetData.GetAsset());
	if (SourceTexture.IsValid())
	{
		LocalImagePath.Empty();
	}
	ClearPreview();
	SetStatus(TEXT("Texture2D source updated."));
}

FReply SPBRTextureLabNomad::OnBrowseLocalImage()
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop)
	{
		SetStatus(TEXT("Desktop platform module is unavailable."));
		return FReply::Handled();
	}

	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	TArray<FString> Files;
	const bool bOpened = Desktop->OpenFileDialog(
		ParentWindow,
		TEXT("Select a source image"),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("Image files (*.png;*.jpg;*.jpeg;*.bmp;*.tga)|*.png;*.jpg;*.jpeg;*.bmp;*.tga"),
		EFileDialogFlags::None,
		Files);
	if (bOpened && Files.Num() > 0)
	{
		LocalImagePath = Files[0];
		SourceTexture.Reset();
		ClearPreview();
		SetStatus(FString::Printf(TEXT("Local image: %s"), *LocalImagePath));
	}
	return FReply::Handled();
}

FReply SPBRTextureLabNomad::OnUseContentBrowserTexture()
{
	TArray<FAssetData> Selected;
	PBRTextureLab::GetContentBrowserSelections(Selected);
	for (const FAssetData& Asset : Selected)
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(Asset.GetAsset()))
		{
			SourceTexture = Texture;
			LocalImagePath.Empty();
			ClearPreview();
			SetStatus(FString::Printf(TEXT("Using Texture2D %s"), *Texture->GetPathName()));
			return FReply::Handled();
		}
	}
	SetStatus(TEXT("Content Browser has no Texture2D selected."));
	return FReply::Handled();
}

bool SPBRTextureLabNomad::LoadCurrentSource(PBRTextureLab::FPBRImageRgba8& OutImage, FString* OutError) const
{
	if (SourceTexture.IsValid())
	{
		return PBRTextureLab::ReadSourceTexture2D(SourceTexture.Get(), OutImage, OutError);
	}
	if (!LocalImagePath.IsEmpty())
	{
		return PBRTextureLab::ReadSourceLocalFile(LocalImagePath, OutImage, OutError);
	}
	if (OutError)
	{
		*OutError = TEXT("Select a Texture2D or a local image.");
	}
	return false;
}

PBRTextureLab::FPBRGenerateRequest SPBRTextureLabNomad::MakeRequest() const
{
	PBRTextureLab::FPBRGenerateRequest Request;
	Request.SourceTexture = SourceTexture.Get();
	Request.LocalImagePath = LocalImagePath;
	Request.OutputWidth = OutputWidth;
	Request.OutputHeight = OutputHeight;
	Request.PixelParams = PixelParams;
	Request.DestinationPath = DestinationPath;
	Request.BaseName = BaseName;
	Request.ConflictPolicy = ConflictPolicy;
	Request.MaterialNormalStrength = PixelParams.NormalStrength;
	Request.MaterialHeightAmount = MaterialHeightAmount;
	Request.MaterialUVScale = MaterialUVScale;
	Request.bCreateMaterial = true;
	Request.bSave = true;
	return Request;
}

void SPBRTextureLabNomad::SetStatus(const FString& Message)
{
	StatusMessage = Message;
	UE_LOG(LogPBRTextureLab, Log, TEXT("%s"), *Message);
}

void SPBRTextureLabNomad::ClearPreview()
{
	bHasPreview = false;
	PreviewMaps = PBRTextureLab::FPBRMaps();
	SourceBrush.Reset();
	BaseColorBrush.Reset();
	NormalBrush.Reset();
	RoughnessBrush.Reset();
	MetallicBrush.Reset();
	OrmBrush.Reset();
}

void SPBRTextureLabNomad::MakePreviewBrush(
	const PBRTextureLab::FPBRImageRgba8& Image,
	const TCHAR* Label,
	TSharedPtr<FSlateDynamicImageBrush>& OutBrush)
{
	OutBrush.Reset();
	if (!PBRTextureLab::IsValidImage(Image))
	{
		return;
	}

	PBRTextureLab::FPBRImageRgba8 PreviewImage;
	const int32 MaxPreview = 128;
	const int32 Width = FMath::Min(Image.Width, MaxPreview);
	const int32 Height = FMath::Min(Image.Height, MaxPreview);
	FString ResizeError;
	if (!PBRTextureLab::PrepareWorkingImage(Image, Width, Height, PreviewImage, &ResizeError)
		|| !PBRTextureLab::IsValidImage(PreviewImage))
	{
		return;
	}

	TArray<uint8> Bytes;
	Bytes.SetNumUninitialized(PreviewImage.Pixels.Num() * sizeof(FColor));
	FMemory::Memcpy(Bytes.GetData(), PreviewImage.Pixels.GetData(), Bytes.Num());
	++GenerationToken;
	const FName BrushName(*FString::Printf(TEXT("PBRTextureLabPreview_%s_%u"), Label, GenerationToken));
	OutBrush = FSlateDynamicImageBrush::CreateWithImageData(
		BrushName,
		FVector2D(static_cast<float>(PreviewImage.Width), static_cast<float>(PreviewImage.Height)),
		Bytes);
}

void SPBRTextureLabNomad::ApplyPreviewMaps(
	const PBRTextureLab::FPBRMaps& Maps,
	const PBRTextureLab::FPBRImageRgba8& Source)
{
	PreviewMaps = Maps;
	bHasPreview = true;
	MakePreviewBrush(Source, TEXT("Source"), SourceBrush);
	MakePreviewBrush(Maps.BaseColor, TEXT("BaseColor"), BaseColorBrush);
	MakePreviewBrush(Maps.Normal, TEXT("Normal"), NormalBrush);
	MakePreviewBrush(Maps.Roughness, TEXT("Roughness"), RoughnessBrush);
	MakePreviewBrush(Maps.Metallic, TEXT("Metallic"), MetallicBrush);
	MakePreviewBrush(Maps.ORM, TEXT("ORM"), OrmBrush);
}

FReply SPBRTextureLabNomad::OnRefreshPreview()
{
	FString Error;
	PBRTextureLab::FPBRImageRgba8 Source;
	if (!LoadCurrentSource(Source, &Error))
	{
		SetStatus(Error);
		return FReply::Handled();
	}

	PBRTextureLab::FPBRImageRgba8 Working;
	if (!PBRTextureLab::PrepareWorkingImage(Source, OutputWidth, OutputHeight, Working, &Error))
	{
		SetStatus(Error);
		return FReply::Handled();
	}

	PBRTextureLab::FPBRMaps Maps;
	FString NewDisclaimer;
	if (!PBRTextureLab::GeneratePBRMaps(Working, PixelParams, Maps, &NewDisclaimer))
	{
		SetStatus(TEXT("Preview generation failed."));
		return FReply::Handled();
	}

	Disclaimer = NewDisclaimer;
	ApplyPreviewMaps(Maps, Working);
	SetStatus(FString::Printf(TEXT("Preview ready (%dx%d)."), Working.Width, Working.Height));
	return FReply::Handled();
}

FReply SPBRTextureLabNomad::OnGenerate()
{
	FString Error;
	PBRTextureLab::FPBRGenerateResult Result;
	const PBRTextureLab::EPBRImportStatus Status = PBRTextureLab::GenerateAndImportFromSource(
		MakeRequest(),
		Result,
		&Error);
	if (Status != PBRTextureLab::EPBRImportStatus::Success)
	{
		SetStatus(Error.IsEmpty()
			? FString::Printf(TEXT("Generate failed (%d)."), static_cast<int32>(Status))
			: Error);
		return FReply::Handled();
	}

	Disclaimer = Result.Disclaimer;
	ApplyPreviewMaps(Result.Maps, Result.WorkingImage);
	const FString InstanceName = Result.MaterialInstance
		? Result.MaterialInstance->GetPathName()
		: TEXT("(no material)");
	SetStatus(FString::Printf(TEXT("Generated %s"), *InstanceName));
	return FReply::Handled();
}

FText SPBRTextureLabNomad::GetUvSelectionText() const
{
	const PBRTextureLab::FPBRUVSelection Selection = PBRTextureLab::GatherUVSelection();
	if (Selection.ConsideredCount == 0)
	{
		return LOCTEXT("UvEmpty", "No Static Mesh selected. Select a Static Mesh Actor, Component, or asset.");
	}
	if (Selection.Items.Num() == 0)
	{
		return LOCTEXT("UvUnsupported", "Selection is not a Static Mesh.");
	}
	if (Selection.NonStaticMeshCount > 0)
	{
		return LOCTEXT("UvMixed", "Mixed selection: Static Mesh and non-Static Mesh.");
	}
	return FText::Format(
		LOCTEXT("UvReady", "{0} Static Mesh(es) selected."),
		FText::AsNumber(Selection.Items.Num()));
}

FReply SPBRTextureLabNomad::OnUvScale100()
{
	PBRTextureLab::ExecuteRegisteredUVCommand(PBRTextureLab::EPBRUVPreset::Scale100);
	SetStatus(TEXT("UV Scale 100x command dispatched."));
	return FReply::Handled();
}

FReply SPBRTextureLabNomad::OnUvScale500()
{
	PBRTextureLab::ExecuteRegisteredUVCommand(PBRTextureLab::EPBRUVPreset::Scale500);
	SetStatus(TEXT("UV Scale 500x command dispatched."));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
