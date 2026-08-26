#include "SPBRTextureLabNomad.h"
#include "SPBRTextureLabPreviewViewport.h"

#include "PBRTextureLabEditorApi.h"
#include "PBRTextureLabPixelCore.h"

#include "AssetRegistry/AssetData.h"
#include "DesktopPlatformModule.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
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
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
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
		if (Label.Contains(TEXT("常量")) || Label.Contains(TEXT("Constant")))
		{
			return PBRTextureLab::EPBRMetallicMode::Constant;
		}
		if (Label.Contains(TEXT("阈值")) || Label.Contains(TEXT("Threshold")))
		{
			return PBRTextureLab::EPBRMetallicMode::ThresholdMask;
		}
		return PBRTextureLab::EPBRMetallicMode::AllBlack;
	}

	PBRTextureLab::EPBRImportConflictPolicy NomadConflictFromLabel(const FString& Label)
	{
		if (Label.Contains(TEXT("覆盖")) || Label.Contains(TEXT("Replace")))
		{
			return PBRTextureLab::EPBRImportConflictPolicy::Replace;
		}
		if (Label.Contains(TEXT("改名")) || Label.Contains(TEXT("Unique")))
		{
			return PBRTextureLab::EPBRImportConflictPolicy::UniqueName;
		}
		return PBRTextureLab::EPBRImportConflictPolicy::Cancel;
	}

	FString NomadNextNumberedName(const FString& Name)
	{
		int32 DigitStart = Name.Len();
		while (DigitStart > 0 && FChar::IsDigit(Name[DigitStart - 1]))
		{
			--DigitStart;
		}
		if (DigitStart == Name.Len())
		{
			return Name + TEXT("_2");
		}
		const int32 Value = FCString::Atoi(*Name.Mid(DigitStart));
		return Name.Left(DigitStart) + FString::FromInt(FMath::Max(Value, 1) + 1);
	}
}

void SPBRTextureLabNomad::Construct(const FArguments& InArgs)
{
	MetallicOptions.Add(MakeShared<FString>(TEXT("全黑")));
	MetallicOptions.Add(MakeShared<FString>(TEXT("常量")));
	MetallicOptions.Add(MakeShared<FString>(TEXT("亮度阈值")));
	SelectedMetallic = MetallicOptions[0];

	ConflictOptions.Add(MakeShared<FString>(TEXT("取消")));
	ConflictOptions.Add(MakeShared<FString>(TEXT("覆盖")));
	ConflictOptions.Add(MakeShared<FString>(TEXT("自动改名")));
	SelectedConflict = ConflictOptions[2];

	OutputModeOptions.Add(MakeShared<FString>(TEXT("新建")));
	OutputModeOptions.Add(MakeShared<FString>(TEXT("修改")));
	SelectedOutputMode = OutputModeOptions[0];

	StatusMessage = TEXT("选择源图和母材质，然后预览或生成。贴图会放进与材质同名的文件夹。");
	Disclaimer = PBRTextureLab::MetallicDisclaimer;
	RebuildBundledParentOptions();
	TrySelectDefaultUserParent();
	if (PBRTextureLabCanCreatePreviewViewport())
	{
		PreviewViewport = SNew(SPBRTextureLabPreviewViewport);
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[BuildModeTabs()]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SSplitter)
				.PhysicalSplitterHandleSize(6.0f)
				+ SSplitter::Slot()
				.Value(0.42f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SWidgetSwitcher)
							.WidgetIndex_Lambda([this]() { return ActivePage; })
							+ SWidgetSwitcher::Slot()[BuildGeneratePage()]
							+ SWidgetSwitcher::Slot()[BuildAssemblePage()]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)[SNew(SSeparator)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)[BuildUvSection()]
					]
				]
				+ SSplitter::Slot()
				.Value(0.58f)
				[
					BuildPreviewPane()
				]
			]
		]
	];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildModeTabs()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("PageGenerate", "单图生成"))
			.OnClicked(this, &SPBRTextureLabNomad::SwitchToGeneratePage)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("PageAssemble", "现成贴图组材质"))
			.OnClicked(this, &SPBRTextureLabNomad::SwitchToAssemblePage)
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildGeneratePage()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[BuildSourceSection()]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildOutputSection()]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildParentReuseSection()]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildParameterSection()]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildActionSection()];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildAssemblePage()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			NomadSectionLabel(LOCTEXT("AssembleHeading", "现成贴图组材质"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(LOCTEXT("AssembleHint", "可选内容浏览器里的贴图，或从本地文件夹导入图片。文件名带 albedo/normal/roughness 等后缀时，选文件夹会自动对槽。"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildOutputSection()]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
		[
			MakeAssembleTextureRow(ParentLabel(PBRTextureLab::EPBRParentParamSlot::BaseColorTexture), &SPBRTextureLabNomad::AssembleBaseColor, PBRTextureLab::EPBRMapKind::BaseColor)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			MakeAssembleTextureRow(ParentLabel(PBRTextureLab::EPBRParentParamSlot::NormalTexture), &SPBRTextureLabNomad::AssembleNormal, PBRTextureLab::EPBRMapKind::Normal)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			MakeAssembleTextureRow(ParentLabel(PBRTextureLab::EPBRParentParamSlot::RoughnessTexture), &SPBRTextureLabNomad::AssembleRoughness, PBRTextureLab::EPBRMapKind::Roughness)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			MakeAssembleTextureRow(ParentLabel(PBRTextureLab::EPBRParentParamSlot::MetallicTexture), &SPBRTextureLabNomad::AssembleMetallic, PBRTextureLab::EPBRMapKind::Metallic)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			MakeAssembleTextureRow(ParentLabel(PBRTextureLab::EPBRParentParamSlot::HeightTexture), &SPBRTextureLabNomad::AssembleHeight, PBRTextureLab::EPBRMapKind::Height)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			MakeAssembleTextureRow(ParentLabel(PBRTextureLab::EPBRParentParamSlot::AOTexture), &SPBRTextureLabNomad::AssembleAO, PBRTextureLab::EPBRMapKind::AO)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AssembleFolder", "从本地文件夹匹配贴图"))
				.OnClicked(this, &SPBRTextureLabNomad::OnBrowseAssembleFolder)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildParentReuseSection()]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
		[
			MakeCheckRow(
				LOCTEXT("CopyToFolder", "把选中贴图复制到材质同名文件夹"),
				TAttribute<ECheckBoxState>::CreateLambda([this]()
				{
					return bCopyExistingTextures ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				}),
				[this](ECheckBoxState State) { bCopyExistingTextures = State == ECheckBoxState::Checked; })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
		[
			SNew(SButton)
			.Text_Lambda([this]()
			{
				return bModifyExisting
					? LOCTEXT("ModifyAssembleBtn", "用选中贴图修改材质")
					: LOCTEXT("AssembleBtn", "用选中贴图生成材质");
			})
			.OnClicked(this, &SPBRTextureLabNomad::OnCreateFromExisting)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text_Lambda([this]() { return FText::FromString(StatusMessage); })
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::MakeAssembleTextureRow(
	TAttribute<FText> Label,
	TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot,
	PBRTextureLab::EPBRMapKind Kind)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.22f).VAlign(VAlign_Center)
		[SNew(STextBlock).Text(Label)]
		+ SHorizontalBox::Slot().FillWidth(0.58f)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(UTexture2D::StaticClass())
			.AllowClear(true)
			.DisplayThumbnail(true)
			.ObjectPath_Lambda([this, Slot]() { return GetAssembleTexturePath(Slot); })
			.OnObjectChanged_Lambda([this, Slot](const FAssetData& AssetData)
			{
				OnAssembleTextureChanged(AssetData, Slot);
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("AssembleBrowseLocal", "本地图片"))
			.OnClicked_Lambda([this, Kind, Slot]()
			{
				return OnBrowseAssembleLocalImage(Kind, Slot);
			})
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildSourceSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			NomadSectionLabel(LOCTEXT("SourceHeading", "源"))
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
				.Text(LOCTEXT("UseSelected", "使用内容浏览器中的 Texture2D"))
				.OnClicked(this, &SPBRTextureLabNomad::OnUseContentBrowserTexture)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("BrowseLocal", "浏览本地图片..."))
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
						LOCTEXT("LocalPathFmt", "本地文件：{0}"),
						FText::FromString(LocalImagePath));
				}
				if (SourceTexture.IsValid())
				{
					return FText::Format(
						LOCTEXT("AssetPathFmt", "Texture2D：{0}"),
						FText::FromString(SourceTexture->GetPathName()));
				}
				return LOCTEXT("NoSource", "尚未选择源。");
			})
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildOutputSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			NomadSectionLabel(LOCTEXT("OutputHeading", "输出与命名"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("OutputModeLabel", "输出方式"))]
			+ SHorizontalBox::Slot().FillWidth(0.75f)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&OutputModeOptions)
				.InitiallySelectedItem(SelectedOutputMode)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewValue, ESelectInfo::Type)
				{
					SelectedOutputMode = NewValue;
					SetModifyMode(NewValue.IsValid() && NewValue->Equals(TEXT("修改")));
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
				{
					return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
				})
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return FText::FromString(SelectedOutputMode.IsValid() ? *SelectedOutputMode : FString());
					})
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]()
			{
				return bModifyExisting ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
				[SNew(STextBlock).Text(LOCTEXT("ModifyTargetLabel", "要修改的材质"))]
				+ SHorizontalBox::Slot().FillWidth(0.75f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UMaterialInstanceConstant::StaticClass())
					.AllowClear(true)
					.DisplayThumbnail(true)
					.ObjectPath(this, &SPBRTextureLabNomad::GetModifyTargetPath)
					.OnObjectChanged(this, &SPBRTextureLabNomad::OnModifyTargetChanged)
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("DestLabel", "输出目录"))]
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
			[SNew(STextBlock).Text(LOCTEXT("ParentLabel", "母球"))]
			+ SHorizontalBox::Slot().FillWidth(0.75f)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&ParentOptions)
				.InitiallySelectedItem(SelectedParentOption)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewValue, ESelectInfo::Type)
				{
					SelectedParentOption = NewValue;
					if (NewValue.IsValid())
					{
						SelectBundledParent(*NewValue);
					}
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
				{
					return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
				})
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return FText::FromString(SelectedParentOption.IsValid() ? *SelectedParentOption : FString());
					})
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("ParentCustomLabel", "其他母材质"))]
			+ SHorizontalBox::Slot().FillWidth(0.75f)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UMaterialInterface::StaticClass())
				.AllowClear(true)
				.DisplayThumbnail(true)
				.ObjectPath(this, &SPBRTextureLabNomad::GetParentMaterialPath)
				.OnObjectChanged(this, &SPBRTextureLabNomad::OnParentMaterialChanged)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text_Lambda([this]()
			{
				if (bModifyExisting && ModifyTarget.IsValid())
				{
					return FText::Format(
						LOCTEXT("ModifyHint", "将覆盖已有材质：{0}"),
						FText::FromString(ModifyTarget->GetPathName()));
				}
				return FText::Format(
					LOCTEXT("FolderHint", "贴图将写入：{0}/{1}/"),
					FText::FromString(DestinationPath),
					FText::FromString(MaterialInstanceName));
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("NameLabel", "贴图前缀"))]
			+ SHorizontalBox::Slot().FillWidth(0.75f)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([this]() { return FText::FromString(BaseName); })
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
				{
					BaseName = Text.ToString();
					if (!bInstanceNameCustomized)
					{
						MaterialInstanceName = BaseName + TEXT("_Inst");
					}
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("InstNameLabel", "材质实例名"))]
			+ SHorizontalBox::Slot().FillWidth(0.75f)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([this]() { return FText::FromString(MaterialInstanceName); })
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
				{
					MaterialInstanceName = Text.ToString();
					bInstanceNameCustomized = true;
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("SizeLabel", "尺寸（0=源尺寸）"))]
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
			[SNew(STextBlock).Text(LOCTEXT("ConflictLabel", "重名策略"))]
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
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			MakeCheckRow(
				LOCTEXT("SyncFolder", "生成后跳转到材质文件夹"),
				TAttribute<ECheckBoxState>::CreateLambda([this]()
				{
					return bSyncBrowserAfterGenerate ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				}),
				[this](ECheckBoxState State) { bSyncBrowserAfterGenerate = State == ECheckBoxState::Checked; })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			MakeCheckRow(
				LOCTEXT("MakeSeamless", "导入后转为无缝贴图"),
				TAttribute<ECheckBoxState>::CreateLambda([this]()
				{
					return bMakeSeamless ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				}),
				[this](ECheckBoxState State) { bMakeSeamless = State == ECheckBoxState::Checked; })
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildParameterSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			NomadSectionLabel(LOCTEXT("ParamsHeading", "参数与贴图开关"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				MakeCheckRow(ParentLabel(PBRTextureLab::EPBRParentParamSlot::BaseColorTexture),
					TAttribute<ECheckBoxState>::CreateLambda([this]()
					{
						return ExportFlags.bBaseColor ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					[this](ECheckBoxState State) { ExportFlags.bBaseColor = State == ECheckBoxState::Checked; })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				MakeCheckRow(ParentLabel(PBRTextureLab::EPBRParentParamSlot::NormalTexture),
					TAttribute<ECheckBoxState>::CreateLambda([this]()
					{
						return ExportFlags.bNormal ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					[this](ECheckBoxState State) { ExportFlags.bNormal = State == ECheckBoxState::Checked; })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				MakeCheckRow(ParentLabel(PBRTextureLab::EPBRParentParamSlot::RoughnessTexture),
					TAttribute<ECheckBoxState>::CreateLambda([this]()
					{
						return ExportFlags.bRoughness ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					[this](ECheckBoxState State) { ExportFlags.bRoughness = State == ECheckBoxState::Checked; })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				MakeCheckRow(ParentLabel(PBRTextureLab::EPBRParentParamSlot::MetallicTexture),
					TAttribute<ECheckBoxState>::CreateLambda([this]()
					{
						return ExportFlags.bMetallic ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					[this](ECheckBoxState State) { ExportFlags.bMetallic = State == ECheckBoxState::Checked; })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				MakeCheckRow(ParentLabel(PBRTextureLab::EPBRParentParamSlot::HeightTexture),
					TAttribute<ECheckBoxState>::CreateLambda([this]()
					{
						return ExportFlags.bHeight ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					[this](ECheckBoxState State) { ExportFlags.bHeight = State == ECheckBoxState::Checked; })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				MakeCheckRow(ParentLabel(PBRTextureLab::EPBRParentParamSlot::AOTexture),
					TAttribute<ECheckBoxState>::CreateLambda([this]()
					{
						return ExportFlags.bAO ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					[this](ECheckBoxState State) { ExportFlags.bAO = State == ECheckBoxState::Checked; })
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				LOCTEXT("HeightContrast", "高度对比度"),
				TAttribute<float>::CreateLambda([this]() { return PixelParams.HeightContrast; }),
				[this](float Value) { PixelParams.HeightContrast = Value; },
				0.0f,
				8.0f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeIntRow(
				LOCTEXT("HeightBlur", "高度模糊半径"),
				TAttribute<int32>::CreateLambda([this]() { return PixelParams.HeightBlurRadius; }),
				[this](int32 Value) { PixelParams.HeightBlurRadius = Value; },
				0,
				16)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("InvertHeight", "反转高度"))]
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
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::NormalStrength),
				TAttribute<float>::CreateLambda([this]() { return PixelParams.NormalStrength; }),
				[this](float Value) { PixelParams.NormalStrength = Value; },
				0.0f,
				8.0f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::RoughnessStrength),
				TAttribute<float>::CreateLambda([this]() { return PixelParams.RoughnessScale; }),
				[this](float Value) { PixelParams.RoughnessScale = Value; },
				0.0f,
				8.0f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::RoughnessBrightness),
				TAttribute<float>::CreateLambda([this]() { return PixelParams.RoughnessBias; }),
				[this](float Value) { PixelParams.RoughnessBias = Value; },
				-1.0f,
				1.0f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("MetallicMode", "金属模式"))]
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
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::MetallicStrength),
				TAttribute<float>::CreateLambda([this]() { return PixelParams.MetallicConstant; }),
				[this](float Value) { PixelParams.MetallicConstant = Value; },
				0.0f,
				1.0f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				LOCTEXT("MetallicThreshold", "金属阈值"),
				TAttribute<float>::CreateLambda([this]() { return PixelParams.MetallicThreshold; }),
				[this](float Value) { PixelParams.MetallicThreshold = Value; },
				0.0f,
				1.0f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(FText::FromString(PBRTextureLab::MetallicDisclaimerZh))
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildParentReuseSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			NomadSectionLabel(LOCTEXT("ParentReuseHeading", "母球参数"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(LOCTEXT("ParentReuseHint", "完全复用所选母球。生成时按母球上已有的贴图槽、开关、强度和 UV 参数写入，不另造一套名字。"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::NormalStrength),
				TAttribute<float>::CreateLambda([this]() { return PixelParams.NormalStrength; }),
				[this](float Value) { PixelParams.NormalStrength = Value; },
				0.0f,
				8.0f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::RoughnessStrength),
				TAttribute<float>::CreateLambda([this]() { return MaterialRoughnessStrength; }),
				[this](float Value) { MaterialRoughnessStrength = Value; },
				0.0f,
				8.0f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::MetallicStrength),
				TAttribute<float>::CreateLambda([this]() { return MaterialMetallicStrength; }),
				[this](float Value) { MaterialMetallicStrength = Value; },
				0.0f,
				8.0f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::HeightAmount),
				TAttribute<float>::CreateLambda([this]() { return MaterialHeightAmount; }),
				[this](float Value) { MaterialHeightAmount = Value; },
				0.0f,
				1.0f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeFloatRow(
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::UVScale),
				TAttribute<float>::CreateLambda([this]() { return MaterialUVScale; }),
				[this](float Value) { MaterialUVScale = Value; },
				0.01f,
				64.0f)
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildPreviewSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			NomadSectionLabel(LOCTEXT("Preview2DHeading", "2D 预览"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SUniformGridPanel)
			.SlotPadding(4.0f)
			+ SUniformGridPanel::Slot(0, 0)[MakePreviewTile(LOCTEXT("PrevSource", "源图"), &SPBRTextureLabNomad::SourceBrush)]
			+ SUniformGridPanel::Slot(1, 0)[MakePreviewTile(
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::BaseColorTexture),
				&SPBRTextureLabNomad::BaseColorBrush,
				TAttribute<EVisibility>::CreateLambda([this]()
				{
					return ExportFlags.bBaseColor ? EVisibility::Visible : EVisibility::Collapsed;
				}))]
			+ SUniformGridPanel::Slot(2, 0)[MakePreviewTile(
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::NormalTexture),
				&SPBRTextureLabNomad::NormalBrush,
				TAttribute<EVisibility>::CreateLambda([this]()
				{
					return ExportFlags.bNormal ? EVisibility::Visible : EVisibility::Collapsed;
				}))]
			+ SUniformGridPanel::Slot(0, 1)[MakePreviewTile(
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::RoughnessTexture),
				&SPBRTextureLabNomad::RoughnessBrush,
				TAttribute<EVisibility>::CreateLambda([this]()
				{
					return ExportFlags.bRoughness ? EVisibility::Visible : EVisibility::Collapsed;
				}))]
			+ SUniformGridPanel::Slot(1, 1)[MakePreviewTile(
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::MetallicTexture),
				&SPBRTextureLabNomad::MetallicBrush,
				TAttribute<EVisibility>::CreateLambda([this]()
				{
					return ExportFlags.bMetallic ? EVisibility::Visible : EVisibility::Collapsed;
				}))]
			+ SUniformGridPanel::Slot(2, 1)[MakePreviewTile(
				ParentLabel(PBRTextureLab::EPBRParentParamSlot::AOTexture),
				&SPBRTextureLabNomad::AoBrush,
				TAttribute<EVisibility>::CreateLambda([this]()
				{
					return ExportFlags.bAO ? EVisibility::Visible : EVisibility::Collapsed;
				}))]
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::MakePreviewPrimitiveButton(const FText& Label, const EPBRPreviewPrimitive Primitive)
{
	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
		.IsChecked_Lambda([this, Primitive]()
		{
			return PreviewViewport.IsValid() && PreviewViewport->GetPreviewPrimitive() == Primitive
				? ECheckBoxState::Checked
				: ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([this, Primitive](ECheckBoxState)
		{
			if (PreviewViewport.IsValid())
			{
				PreviewViewport->SetPreviewPrimitive(Primitive);
			}
		})
		.Padding(FMargin(8.0f, 2.0f))
		[
			SNew(STextBlock).Text(Label)
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::BuildPreviewPane()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[BuildPreviewSection()]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					NomadSectionLabel(LOCTEXT("Preview3DHeading", "3D 预览"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					MakePreviewPrimitiveButton(LOCTEXT("PrimSphere", "球体"), EPBRPreviewPrimitive::Sphere)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					MakePreviewPrimitiveButton(LOCTEXT("PrimCube", "立方体"), EPBRPreviewPrimitive::Cube)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					MakePreviewPrimitiveButton(LOCTEXT("PrimPlane", "平面"), EPBRPreviewPrimitive::Plane)
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SBox)
				.MinDesiredHeight(280.0f)
				[
					PreviewViewport.IsValid()
						? StaticCastSharedRef<SWidget>(PreviewViewport.ToSharedRef())
						: StaticCastSharedRef<SWidget>(
							SNew(SBox)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Preview3DUnavailable", "当前环境无法显示 3D 预览（需要可渲染的 Editor）。默认形状为球体。"))
								.AutoWrapText(true)
							])
				]
			]
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
				.Text(LOCTEXT("PreviewBtn", "预览"))
				.IsEnabled_Lambda([this]() { return !bBusy; })
				.OnClicked(this, &SPBRTextureLabNomad::OnRefreshPreview)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text_Lambda([this]()
				{
					return bModifyExisting
						? LOCTEXT("ModifyGenerateBtn", "修改已有材质")
						: LOCTEXT("GenerateBtn", "生成贴图和材质");
				})
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
			NomadSectionLabel(LOCTEXT("UvHeading", "选中模型"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(this, &SPBRTextureLabNomad::GetUvSelectionText)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text_Lambda([this]()
			{
				if (LastGeneratedMaterial.IsValid())
				{
					return FText::Format(
						LOCTEXT("LastMatFmt", "最近材质：{0}"),
						FText::FromString(LastGeneratedMaterial->GetName()));
				}
				return LOCTEXT("NoLastMat", "尚未生成材质，无法赋给模型。");
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			MakeCheckRow(
				LOCTEXT("UvSyncLods", "同步修改其他 LOD"),
				TAttribute<ECheckBoxState>::CreateLambda([this]()
				{
					return bApplyOtherLods ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				}),
				[this](ECheckBoxState State) { bApplyOtherLods = State == ECheckBoxState::Checked; })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AssignLast", "把最近材质赋给选中模型"))
				.IsEnabled_Lambda([this]() { return LastGeneratedMaterial.IsValid() && !bBusy; })
				.OnClicked(this, &SPBRTextureLabNomad::OnAssignLastMaterial)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Uv100", "Set UV Tiling 100x100"))
				.OnClicked(this, &SPBRTextureLabNomad::OnUvScale100)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Uv500", "Set UV Tiling 500x500"))
				.OnClicked(this, &SPBRTextureLabNomad::OnUvScale500)
			]
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::MakePreviewTile(
	TAttribute<FText> Label,
	TSharedPtr<FSlateDynamicImageBrush> SPBRTextureLabNomad::* BrushMember,
	TAttribute<EVisibility> Visibility) const
{
	return SNew(SBox)
		.Visibility(Visibility)
		[
			SNew(SVerticalBox)
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
			]
		];
}

TSharedRef<SWidget> SPBRTextureLabNomad::MakeFloatRow(
	TAttribute<FText> Label,
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

TSharedRef<SWidget> SPBRTextureLabNomad::MakeCheckRow(
	TAttribute<FText> Label,
	TAttribute<ECheckBoxState> IsChecked,
	TFunction<void(ECheckBoxState)> Setter)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(SCheckBox)
			.IsChecked(IsChecked)
			.OnCheckStateChanged_Lambda([Setter](ECheckBoxState State) { Setter(State); })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[SNew(STextBlock).Text(Label)];
}

void SPBRTextureLabNomad::ApplySourceName(const FString& SourceName)
{
	const FString Sanitized = FPaths::GetBaseFilename(SourceName);
	if (Sanitized.IsEmpty())
	{
		return;
	}
	BaseName = Sanitized;
	if (!bInstanceNameCustomized)
	{
		MaterialInstanceName = Sanitized;
	}
}

void SPBRTextureLabNomad::AdvanceNamesAfterGenerate()
{
	BaseName = NomadNextNumberedName(BaseName);
	MaterialInstanceName = NomadNextNumberedName(MaterialInstanceName);
}

FString SPBRTextureLabNomad::GetParentMaterialPath() const
{
	return ParentMaterial.IsValid() ? ParentMaterial->GetPathName() : FString();
}

void SPBRTextureLabNomad::RebuildBundledParentOptions()
{
	ParentOptions.Reset();
	for (const PBRTextureLab::FPBRBundledParentDesc& Entry : PBRTextureLab::GetBundledParentCatalog())
	{
		ParentOptions.Add(MakeShared<FString>(Entry.DisplayName));
	}
	ParentOptions.Add(MakeShared<FString>(TEXT("内置 Metallic/Roughness")));
	if (ParentOptions.Num() > 0)
	{
		SelectedParentOption = ParentOptions[0];
	}
}

void SPBRTextureLabNomad::SelectBundledParent(const FString& DisplayName)
{
	if (DisplayName.Equals(TEXT("内置 Metallic/Roughness")))
	{
		ParentMaterial.Reset();
		RefreshParentTitles();
		SetStatus(TEXT("使用插件内置 Metallic/Roughness 母球（含基础色/粗糙度/高光度）。"));
		return;
	}

	if (UMaterialInterface* Found = PBRTextureLab::LoadBundledParentByName(DisplayName))
	{
		ParentMaterial = Found;
		RefreshParentTitles();
		SetStatus(FString::Printf(
			TEXT("已选用插件母球：%s。子材质会继承它的基础色、粗糙度、高光度等参数。"),
			*Found->GetName()));
		return;
	}

	SetStatus(FString::Printf(TEXT("插件里没有加载到母球：%s"), *DisplayName));
}

void SPBRTextureLabNomad::TrySelectDefaultUserParent()
{
	if (UMaterialInterface* Found = PBRTextureLab::FindPreferredUserParent())
	{
		ParentMaterial = Found;
		for (const TSharedPtr<FString>& Option : ParentOptions)
		{
			if (Option.IsValid() && Option->Equals(Found->GetName()))
			{
				SelectedParentOption = Option;
				break;
			}
		}
		RefreshParentTitles();
		SetStatus(FString::Printf(
			TEXT("已选用母球：%s。子材质会继承它的基础色、粗糙度、高光度等自定义参数。"),
			*Found->GetName()));
		return;
	}
	RefreshParentTitles();
	SetStatus(TEXT("未找到插件母球，请在下拉列表里选择，或在「其他母材质」中指定。"));
}

void SPBRTextureLabNomad::RefreshParentTitles()
{
	CachedParentTitles = PBRTextureLab::InspectParentParamTitles(ParentMaterial.Get());
}

FText SPBRTextureLabNomad::GetParentParamLabel(const PBRTextureLab::EPBRParentParamSlot Slot) const
{
	return FText::FromString(CachedParentTitles.Get(Slot));
}

TAttribute<FText> SPBRTextureLabNomad::ParentLabel(const PBRTextureLab::EPBRParentParamSlot Slot) const
{
	return TAttribute<FText>::CreateLambda([this, Slot]()
	{
		return GetParentParamLabel(Slot);
	});
}

void SPBRTextureLabNomad::OnParentMaterialChanged(const FAssetData& AssetData)
{
	ParentMaterial = Cast<UMaterialInterface>(AssetData.GetAsset());
	RefreshParentTitles();
	if (ParentMaterial.IsValid())
	{
		bool bMatchedCatalog = false;
		for (const TSharedPtr<FString>& Option : ParentOptions)
		{
			if (Option.IsValid() && Option->Equals(ParentMaterial->GetName()))
			{
				SelectedParentOption = Option;
				bMatchedCatalog = true;
				break;
			}
		}
		if (!bMatchedCatalog)
		{
			SelectedParentOption.Reset();
		}
		SetStatus(FString::Printf(TEXT("母材质：%s。界面按它的自定义参数标题显示。"), *ParentMaterial->GetName()));
	}
	else
	{
		SetStatus(TEXT("未选择母材质时，使用插件内置 Metallic/Roughness 母球。"));
	}
}

FString SPBRTextureLabNomad::GetModifyTargetPath() const
{
	return ModifyTarget.IsValid() ? ModifyTarget->GetPathName() : FString();
}

void SPBRTextureLabNomad::OnModifyTargetChanged(const FAssetData& AssetData)
{
	ModifyTarget = Cast<UMaterialInstanceConstant>(AssetData.GetAsset());
	if (!ModifyTarget.IsValid())
	{
		SetStatus(TEXT("请选择要修改的子材质。"));
		return;
	}

	MaterialInstanceName = ModifyTarget->GetName();
	bInstanceNameCustomized = true;
	DestinationPath = FPackageName::GetLongPackagePath(ModifyTarget->GetOutermost()->GetName());
	if (UMaterialInterface* ExistingParent = ModifyTarget->Parent)
	{
		ParentMaterial = ExistingParent;
		RefreshParentTitles();
	}
	SetStatus(FString::Printf(TEXT("将修改：%s"), *ModifyTarget->GetPathName()));
}

void SPBRTextureLabNomad::SetModifyMode(const bool bEnable)
{
	bModifyExisting = bEnable;
	if (!bModifyExisting)
	{
		return;
	}
	if (!ModifyTarget.IsValid())
	{
		ModifyTarget = Cast<UMaterialInstanceConstant>(LastGeneratedMaterial.Get());
	}
	if (ModifyTarget.IsValid())
	{
		OnModifyTargetChanged(FAssetData(ModifyTarget.Get()));
	}
	else
	{
		SetStatus(TEXT("修改模式：请选择要覆盖的已有子材质。"));
	}
}

FReply SPBRTextureLabNomad::SwitchToGeneratePage()
{
	ActivePage = 0;
	return FReply::Handled();
}

FReply SPBRTextureLabNomad::SwitchToAssemblePage()
{
	ActivePage = 1;
	return FReply::Handled();
}

FString SPBRTextureLabNomad::GetAssembleTexturePath(TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot) const
{
	const TWeakObjectPtr<UTexture2D>& Texture = this->*Slot;
	return Texture.IsValid() ? Texture->GetPathName() : FString();
}

void SPBRTextureLabNomad::OnAssembleTextureChanged(
	const FAssetData& AssetData,
	TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot)
{
	AssignAssembleTexture(Slot, Cast<UTexture2D>(AssetData.GetAsset()));
}

void SPBRTextureLabNomad::AssignAssembleTexture(
	TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot,
	UTexture2D* Texture)
{
	this->*Slot = Texture;
	if (Texture && !bInstanceNameCustomized && MaterialInstanceName == TEXT("PBR_Inst"))
	{
		ApplySourceName(Texture->GetName());
	}
}

FReply SPBRTextureLabNomad::OnBrowseAssembleLocalImage(
	PBRTextureLab::EPBRMapKind Kind,
	TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot)
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop)
	{
		SetStatus(TEXT("无法打开文件对话框。"));
		return FReply::Handled();
	}

	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	TArray<FString> Files;
	const bool bOpened = Desktop->OpenFileDialog(
		ParentWindow,
		TEXT("选择本地贴图"),
		LastLocalFolder.IsEmpty() ? FPaths::ProjectDir() : LastLocalFolder,
		TEXT(""),
		TEXT("图片文件 (*.png;*.jpg;*.jpeg;*.bmp;*.tga)|*.png;*.jpg;*.jpeg;*.bmp;*.tga"),
		EFileDialogFlags::None,
		Files);
	if (!bOpened || Files.Num() == 0)
	{
		return FReply::Handled();
	}

	LastLocalFolder = FPaths::GetPath(Files[0]);
	FString Error;
	UTexture2D* Texture = PBRTextureLab::ImportLocalImageFile(
		Files[0],
		TEXT("/Game/PBRTextureLab/Local"),
		FString(),
		Kind,
		PBRTextureLab::EPBRImportConflictPolicy::UniqueName,
		true,
		&Error,
		bMakeSeamless);
	if (!Texture)
	{
		SetStatus(Error.IsEmpty() ? TEXT("导入本地图片失败。") : Error);
		return FReply::Handled();
	}

	AssignAssembleTexture(Slot, Texture);
	SetStatus(FString::Printf(TEXT("已导入本地图片：%s"), *Texture->GetName()));
	return FReply::Handled();
}

FReply SPBRTextureLabNomad::OnBrowseAssembleFolder()
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop)
	{
		SetStatus(TEXT("无法打开文件夹对话框。"));
		return FReply::Handled();
	}

	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	FString Folder;
	const bool bOpened = Desktop->OpenDirectoryDialog(
		ParentWindow,
		TEXT("选择包含 PBR 贴图的本地文件夹"),
		LastLocalFolder.IsEmpty() ? FPaths::ProjectDir() : LastLocalFolder,
		Folder);
	if (!bOpened || Folder.IsEmpty())
	{
		return FReply::Handled();
	}

	LastLocalFolder = Folder;
	if (!bInstanceNameCustomized)
	{
		ApplySourceName(FPaths::GetCleanFilename(Folder));
	}

	PBRTextureLab::FPBRImportedTextures Imported;
	FString Error;
	const PBRTextureLab::EPBRImportStatus Status = PBRTextureLab::ImportPBRMapsFromLocalFolder(
		Folder,
		TEXT("/Game/PBRTextureLab/Local"),
		MaterialInstanceName,
		PBRTextureLab::EPBRImportConflictPolicy::UniqueName,
		true,
		Imported,
		&Error,
		bMakeSeamless);
	if (Status != PBRTextureLab::EPBRImportStatus::Success)
	{
		SetStatus(Error.IsEmpty() ? TEXT("本地文件夹里没有匹配到贴图。") : Error);
		return FReply::Handled();
	}

	if (Imported.BaseColor) { AssembleBaseColor = Imported.BaseColor; }
	if (Imported.Normal) { AssembleNormal = Imported.Normal; }
	if (Imported.Roughness) { AssembleRoughness = Imported.Roughness; }
	if (Imported.Metallic) { AssembleMetallic = Imported.Metallic; }
	if (Imported.Height) { AssembleHeight = Imported.Height; }
	if (Imported.AO) { AssembleAO = Imported.AO; }

	int32 Count = 0;
	if (Imported.BaseColor) { ++Count; }
	if (Imported.Normal) { ++Count; }
	if (Imported.Roughness) { ++Count; }
	if (Imported.Metallic) { ++Count; }
	if (Imported.Height) { ++Count; }
	if (Imported.AO) { ++Count; }
	SetStatus(FString::Printf(TEXT("已从文件夹匹配 %d 张贴图：%s"), Count, *Folder));
	return FReply::Handled();
}

FReply SPBRTextureLabNomad::OnCreateFromExisting()
{
	if (bModifyExisting && !ModifyTarget.IsValid())
	{
		SetStatus(TEXT("修改模式需要先选择要覆盖的子材质。"));
		return FReply::Handled();
	}

	PBRTextureLab::FPBRImportedTextures Selected;
	Selected.BaseColor = AssembleBaseColor.Get();
	Selected.Normal = AssembleNormal.Get();
	Selected.Roughness = AssembleRoughness.Get();
	Selected.Metallic = AssembleMetallic.Get();
	Selected.Height = AssembleHeight.Get();
	Selected.AO = AssembleAO.Get();

	PBRTextureLab::FPBRGenerateRequest Request = MakeRequest();
	Request.bCopyExistingTexturesToFolder = bCopyExistingTextures;

	PBRTextureLab::FPBRGenerateResult Result;
	FString Error;
	const PBRTextureLab::EPBRImportStatus Status = PBRTextureLab::CreateMaterialFromExistingTextures(
		Request,
		Selected,
		Result,
		&Error);
	if (Status != PBRTextureLab::EPBRImportStatus::Success)
	{
		SetStatus(Error.IsEmpty()
			? FString::Printf(TEXT("用现成贴图生成失败（%d）。"), static_cast<int32>(Status))
			: Error);
		return FReply::Handled();
	}

	const FString Created = Result.MaterialInstance
		? Result.MaterialInstance->GetPathName()
		: Result.OutputFolder;
	HandleGenerateSuccess(Result);
	return FReply::Handled();
}

TSharedRef<SWidget> SPBRTextureLabNomad::MakeIntRow(
	TAttribute<FText> Label,
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
		ApplySourceName(SourceTexture->GetName());
	}
	ClearPreview();
	SetStatus(TEXT("已更新 Texture2D 源。"));
}

FReply SPBRTextureLabNomad::OnBrowseLocalImage()
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop)
	{
		SetStatus(TEXT("无法打开文件对话框。"));
		return FReply::Handled();
	}

	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	TArray<FString> Files;
	const bool bOpened = Desktop->OpenFileDialog(
		ParentWindow,
		TEXT("选择源图片"),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("图片文件 (*.png;*.jpg;*.jpeg;*.bmp;*.tga)|*.png;*.jpg;*.jpeg;*.bmp;*.tga"),
		EFileDialogFlags::None,
		Files);
	if (bOpened && Files.Num() > 0)
	{
		LocalImagePath = Files[0];
		SourceTexture.Reset();
		ApplySourceName(LocalImagePath);
		ClearPreview();
		SetStatus(FString::Printf(TEXT("本地图片：%s"), *LocalImagePath));
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
			ApplySourceName(Texture->GetName());
			ClearPreview();
			SetStatus(FString::Printf(TEXT("使用 Texture2D %s"), *Texture->GetPathName()));
			return FReply::Handled();
		}
	}
	SetStatus(TEXT("内容浏览器没有选中 Texture2D。"));
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
		*OutError = TEXT("请先选择一张 Texture2D 或本地图片。");
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
	Request.MaterialInstanceName = MaterialInstanceName;
	Request.ParentMaterial = ParentMaterial.Get();
	Request.ExistingInstance = ModifyTarget.Get();
	Request.bModifyExisting = bModifyExisting;
	Request.ConflictPolicy = bModifyExisting
		? PBRTextureLab::EPBRImportConflictPolicy::Replace
		: ConflictPolicy;
	Request.ExportFlags = ExportFlags;
	Request.MaterialNormalStrength = PixelParams.NormalStrength;
	Request.MaterialRoughnessStrength = MaterialRoughnessStrength;
	Request.MaterialMetallicStrength = MaterialMetallicStrength;
	Request.MaterialHeightAmount = MaterialHeightAmount;
	Request.MaterialUVScale = MaterialUVScale;
	Request.MaterialRoughnessBrightness = PixelParams.RoughnessBias;
	Request.bMakeSeamless = bMakeSeamless;
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
	AoBrush.Reset();
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
	if (ExportFlags.bBaseColor)
	{
		MakePreviewBrush(Maps.BaseColor, TEXT("BaseColor"), BaseColorBrush);
	}
	else
	{
		BaseColorBrush.Reset();
	}
	if (ExportFlags.bNormal)
	{
		MakePreviewBrush(Maps.Normal, TEXT("Normal"), NormalBrush);
	}
	else
	{
		NormalBrush.Reset();
	}
	if (ExportFlags.bRoughness)
	{
		MakePreviewBrush(Maps.Roughness, TEXT("Roughness"), RoughnessBrush);
	}
	else
	{
		RoughnessBrush.Reset();
	}
	if (ExportFlags.bMetallic)
	{
		MakePreviewBrush(Maps.Metallic, TEXT("Metallic"), MetallicBrush);
	}
	else
	{
		MetallicBrush.Reset();
	}
	if (ExportFlags.bAO)
	{
		MakePreviewBrush(Maps.AO, TEXT("AO"), AoBrush);
	}
	else
	{
		AoBrush.Reset();
	}
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

	if (bMakeSeamless && !PBRTextureLab::MakeSeamlessImage(Working, false))
	{
		SetStatus(TEXT("预览时转为无缝贴图失败。"));
		return FReply::Handled();
	}

	PBRTextureLab::FPBRMaps Maps;
	FString NewDisclaimer;
	if (!PBRTextureLab::GeneratePBRMaps(Working, PixelParams, Maps, &NewDisclaimer))
	{
		SetStatus(TEXT("预览生成失败。"));
		return FReply::Handled();
	}

	Disclaimer = NewDisclaimer;
	ApplyPreviewMaps(Maps, Working);
	SetStatus(FString::Printf(TEXT("预览完成（%dx%d）。"), Working.Width, Working.Height));
	return FReply::Handled();
}

FReply SPBRTextureLabNomad::OnGenerate()
{
	if (bModifyExisting && !ModifyTarget.IsValid())
	{
		SetStatus(TEXT("修改模式需要先选择要覆盖的子材质。"));
		return FReply::Handled();
	}

	FString Error;
	PBRTextureLab::FPBRGenerateResult Result;
	const PBRTextureLab::EPBRImportStatus Status = PBRTextureLab::GenerateAndImportFromSource(
		MakeRequest(),
		Result,
		&Error);
	if (Status != PBRTextureLab::EPBRImportStatus::Success)
	{
		SetStatus(Error.IsEmpty()
			? FString::Printf(TEXT("生成失败（%d）。"), static_cast<int32>(Status))
			: Error);
		return FReply::Handled();
	}

	Disclaimer = Result.Disclaimer;
	ApplyPreviewMaps(Result.Maps, Result.WorkingImage);
	HandleGenerateSuccess(Result);
	return FReply::Handled();
}

void SPBRTextureLabNomad::HandleGenerateSuccess(const PBRTextureLab::FPBRGenerateResult& Result)
{
	LastGeneratedMaterial = Result.MaterialInstance;
	LastGeneratedFolder = Result.OutputFolder;
	const FString Created = Result.MaterialInstance
		? Result.MaterialInstance->GetPathName()
		: Result.OutputFolder;
	SetStatus(bModifyExisting
		? FString::Printf(TEXT("已修改 %s（贴图在 %s）"), *Created, *Result.OutputFolder)
		: FString::Printf(TEXT("已生成 %s（贴图在 %s）"), *Created, *Result.OutputFolder));
	if (Result.MaterialInstance)
	{
		ModifyTarget = Result.MaterialInstance;
		if (PreviewViewport.IsValid())
		{
			PreviewViewport->SetPreviewMaterial(Result.MaterialInstance);
		}
	}
	if (bSyncBrowserAfterGenerate)
	{
		PBRTextureLab::SyncContentBrowserToGeneratedFolder(
			Result.OutputFolder,
			Result.MaterialInstance);
	}
	if (!bModifyExisting)
	{
		AdvanceNamesAfterGenerate();
	}
}

FText SPBRTextureLabNomad::GetUvSelectionText() const
{
	const PBRTextureLab::FPBRUVSelection Selection = PBRTextureLab::GatherUVSelection();
	if (Selection.ConsideredCount == 0)
	{
		return LOCTEXT("UvEmpty", "未选中 Static Mesh。请在关卡或内容浏览器中选中模型。");
	}
	if (Selection.Items.Num() == 0)
	{
		return LOCTEXT("UvUnsupported", "当前选中的不是 Static Mesh。");
	}
	if (Selection.NonStaticMeshCount > 0)
	{
		return LOCTEXT("UvMixed", "选中项混杂了 Static Mesh 和非 Static Mesh。");
	}
	return FText::Format(
		LOCTEXT("UvReady", "已选中 {0} 个 Static Mesh。"),
		FText::AsNumber(Selection.Items.Num()));
}

FReply SPBRTextureLabNomad::OnAssignLastMaterial()
{
	FString Error;
	const PBRTextureLab::EPBRAssignMaterialStatus Status =
		PBRTextureLab::AssignMaterialToSelection(LastGeneratedMaterial.Get(), &Error);
	switch (Status)
	{
	case PBRTextureLab::EPBRAssignMaterialStatus::Success:
		SetStatus(LastGeneratedMaterial.IsValid()
			? FString::Printf(TEXT("已把 %s 赋给选中模型。"), *LastGeneratedMaterial->GetName())
			: TEXT("已赋给选中模型。"));
		break;
	case PBRTextureLab::EPBRAssignMaterialStatus::EmptySelection:
		SetStatus(TEXT("请先在关卡或内容浏览器中选中 Static Mesh。"));
		break;
	case PBRTextureLab::EPBRAssignMaterialStatus::Unsupported:
		SetStatus(TEXT("当前选中的不是 Static Mesh。"));
		break;
	case PBRTextureLab::EPBRAssignMaterialStatus::NoMaterial:
		SetStatus(TEXT("还没有最近生成的材质。请先生成。"));
		break;
	default:
		SetStatus(Error.IsEmpty() ? TEXT("赋材质失败。") : Error);
		break;
	}
	return FReply::Handled();
}

FReply SPBRTextureLabNomad::OnUvScale100()
{
	return DispatchUvScale(PBRTextureLab::EPBRUVPreset::Scale100, TEXT("100x100"));
}

FReply SPBRTextureLabNomad::OnUvScale500()
{
	return DispatchUvScale(PBRTextureLab::EPBRUVPreset::Scale500, TEXT("500x500"));
}

FReply SPBRTextureLabNomad::DispatchUvScale(const PBRTextureLab::EPBRUVPreset Preset, const TCHAR* Label)
{
	PBRTextureLab::FPBRUVCommandRequest Request;
	Request.Preset = Preset;
	Request.EditChoice = PBRTextureLab::EPBRUVEditChoice::Prompt;
	Request.bApplyOtherLods = bApplyOtherLods;
	FString Error;
	const PBRTextureLab::EPBRUVCommandStatus Status = PBRTextureLab::ExecuteUVCommand(Request, &Error);
	switch (Status)
	{
	case PBRTextureLab::EPBRUVCommandStatus::Success:
		SetStatus(FString::Printf(
			TEXT("已把选中模型的 UV0 设为 %s 平铺（NewUV = OldUV * 该倍率%s）。"),
			Label,
			bApplyOtherLods ? TEXT("，已同步其他 LOD") : TEXT("，仅 LOD0")));
		break;
	case PBRTextureLab::EPBRUVCommandStatus::Cancelled:
		SetStatus(TEXT("已取消 UV 缩放。"));
		break;
	case PBRTextureLab::EPBRUVCommandStatus::EmptySelection:
		SetStatus(TEXT("请先在关卡或内容浏览器中选中 Static Mesh。"));
		break;
	case PBRTextureLab::EPBRUVCommandStatus::MixedSelection:
		SetStatus(TEXT("选中项混杂了非 Static Mesh，请只选模型。"));
		break;
	case PBRTextureLab::EPBRUVCommandStatus::Unsupported:
		SetStatus(Error.IsEmpty() ? TEXT("当前选中的网格不支持改 UV0。") : Error);
		break;
	default:
		SetStatus(Error.IsEmpty() ? TEXT("UV 缩放失败。") : Error);
		break;
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
