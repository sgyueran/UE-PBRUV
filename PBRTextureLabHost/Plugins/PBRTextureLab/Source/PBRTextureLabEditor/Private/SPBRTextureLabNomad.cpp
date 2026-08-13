#include "SPBRTextureLabNomad.h"

#include "PBRTextureLabEditorApi.h"
#include "PBRTextureLabPixelCore.h"

#include "AssetRegistry/AssetData.h"
#include "DesktopPlatformModule.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
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
#include "Widgets/Layout/SWidgetSwitcher.h"
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
	ExportFlags.bORM = false;

	StatusMessage = TEXT("选择源图和母材质，然后预览或生成。贴图会放进与材质同名的文件夹。");
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
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[BuildModeTabs()]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([this]() { return ActivePage; })
					+ SWidgetSwitcher::Slot()[BuildGeneratePage()]
					+ SWidgetSwitcher::Slot()[BuildAssemblePage()]
				]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildUvSection()]
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
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildParameterSection()]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildPreviewSection()]
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
			.Text(LOCTEXT("AssembleHint", "选择母材质和已有 PBR 贴图，直接生成子材质。贴图会复制到与材质同名的文件夹。"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[BuildOutputSection()]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
		[
			MakeAssembleTextureRow(LOCTEXT("AsmBase", "基础贴图"), &SPBRTextureLabNomad::AssembleBaseColor)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			MakeAssembleTextureRow(LOCTEXT("AsmNormal", "法线贴图"), &SPBRTextureLabNomad::AssembleNormal)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			MakeAssembleTextureRow(LOCTEXT("AsmRough", "粗糙贴图"), &SPBRTextureLabNomad::AssembleRoughness)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			MakeAssembleTextureRow(LOCTEXT("AsmMetal", "金属贴图"), &SPBRTextureLabNomad::AssembleMetallic)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			MakeAssembleTextureRow(LOCTEXT("AsmHeight", "置换/高度"), &SPBRTextureLabNomad::AssembleHeight)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			MakeAssembleTextureRow(LOCTEXT("AsmAO", "AO"), &SPBRTextureLabNomad::AssembleAO)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			MakeAssembleTextureRow(LOCTEXT("AsmORM", "ORM"), &SPBRTextureLabNomad::AssembleORM)
		]
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
			.Text(LOCTEXT("AssembleBtn", "用选中贴图生成材质"))
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
	const FText& Label,
	TWeakObjectPtr<UTexture2D> SPBRTextureLabNomad::* Slot)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
		[SNew(STextBlock).Text(Label)]
		+ SHorizontalBox::Slot().FillWidth(0.75f)
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
			[SNew(STextBlock).Text(LOCTEXT("ParentLabel", "母材质"))]
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
				MakeCheckRow(LOCTEXT("ExpBase", "基础贴图"),
					TAttribute<ECheckBoxState>::CreateLambda([this]()
					{
						return ExportFlags.bBaseColor ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					[this](ECheckBoxState State) { ExportFlags.bBaseColor = State == ECheckBoxState::Checked; })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				MakeCheckRow(LOCTEXT("ExpNormal", "法线"),
					TAttribute<ECheckBoxState>::CreateLambda([this]()
					{
						return ExportFlags.bNormal ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					[this](ECheckBoxState State) { ExportFlags.bNormal = State == ECheckBoxState::Checked; })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				MakeCheckRow(LOCTEXT("ExpRough", "粗糙"),
					TAttribute<ECheckBoxState>::CreateLambda([this]()
					{
						return ExportFlags.bRoughness ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					[this](ECheckBoxState State) { ExportFlags.bRoughness = State == ECheckBoxState::Checked; })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				MakeCheckRow(LOCTEXT("ExpMetal", "金属"),
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
				MakeCheckRow(LOCTEXT("ExpHeight", "置换/高度"),
					TAttribute<ECheckBoxState>::CreateLambda([this]()
					{
						return ExportFlags.bHeight ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					[this](ECheckBoxState State) { ExportFlags.bHeight = State == ECheckBoxState::Checked; })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				MakeCheckRow(LOCTEXT("ExpAO", "AO"),
					TAttribute<ECheckBoxState>::CreateLambda([this]()
					{
						return ExportFlags.bAO ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					[this](ECheckBoxState State) { ExportFlags.bAO = State == ECheckBoxState::Checked; })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				MakeCheckRow(LOCTEXT("ExpORM", "ORM"),
					TAttribute<ECheckBoxState>::CreateLambda([this]()
					{
						return ExportFlags.bORM ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					[this](ECheckBoxState State) { ExportFlags.bORM = State == ECheckBoxState::Checked; })
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

TSharedRef<SWidget> SPBRTextureLabNomad::MakeCheckRow(
	const FText& Label,
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

void SPBRTextureLabNomad::OnParentMaterialChanged(const FAssetData& AssetData)
{
	ParentMaterial = Cast<UMaterialInterface>(AssetData.GetAsset());
	SetStatus(ParentMaterial.IsValid()
		? FString::Printf(TEXT("母材质：%s"), *ParentMaterial->GetName())
		: TEXT("未选择母材质时，使用插件内置 Metallic/Roughness 母球。"));
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
	this->*Slot = Cast<UTexture2D>(AssetData.GetAsset());
	if ((this->*Slot).IsValid() && !bInstanceNameCustomized && MaterialInstanceName == TEXT("PBR_Inst"))
	{
		ApplySourceName((this->*Slot)->GetName());
	}
}

FReply SPBRTextureLabNomad::OnCreateFromExisting()
{
	PBRTextureLab::FPBRImportedTextures Selected;
	Selected.BaseColor = AssembleBaseColor.Get();
	Selected.Normal = AssembleNormal.Get();
	Selected.Roughness = AssembleRoughness.Get();
	Selected.Metallic = AssembleMetallic.Get();
	Selected.Height = AssembleHeight.Get();
	Selected.AO = AssembleAO.Get();
	Selected.ORM = AssembleORM.Get();

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
	SetStatus(FString::Printf(TEXT("已生成 %s（贴图在 %s）"), *Created, *Result.OutputFolder));
	AdvanceNamesAfterGenerate();
	return FReply::Handled();
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
	Request.MaterialInstanceName = MaterialInstanceName;
	Request.ParentMaterial = ParentMaterial.Get();
	Request.ConflictPolicy = ConflictPolicy;
	Request.ExportFlags = ExportFlags;
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
	const FString Created = Result.MaterialInstance
		? Result.MaterialInstance->GetPathName()
		: Result.OutputFolder;
	SetStatus(FString::Printf(TEXT("已生成 %s（贴图在 %s）"), *Created, *Result.OutputFolder));
	AdvanceNamesAfterGenerate();
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
