#include "PBRTextureLabEditorModule.h"
#include "PBRTextureLabCommand.h"
#include "PBRTextureLabCommands.h"
#include "PBRTextureLabCompat.h"
#include "PBRTextureLabNomad.h"
#include "PBRTextureLabPixelCore.h"
#include "SPBRTextureLabNomad.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "LevelEditor.h"
#include "Styling/AppStyle.h"
#include "ToolMenuOwner.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FPBRTextureLabEditorModule"

void FPBRTextureLabEditorModule::StartupModule()
{
	UE_LOG(LogPBRTextureLab, Log, TEXT("PBRTextureLabEditor started on UE %d.%d.%d"),
		PBRTEXTURELAB_ENGINE_MAJOR,
		PBRTEXTURELAB_ENGINE_MINOR,
		PBRTEXTURELAB_ENGINE_PATCH);
	UE_LOG(LogPBRTextureLab, Warning, TEXT("%s"), PBRTextureLab::MetallicDisclaimer);

	FPBRTextureLabCommands::Register();
	BindCommands();
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		PBRTextureLab::GetNomadTabId(),
		FOnSpawnTab::CreateRaw(this, &FPBRTextureLabEditorModule::SpawnNomadTab))
		.SetDisplayName(LOCTEXT("NomadTabTitle", "PBR Texture Lab"))
		.SetTooltipText(LOCTEXT("NomadTabTip", "Generate Metallic/Roughness maps from a Texture2D or local image and apply UV scale presets."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.Texture2D")));
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPBRTextureLabEditorModule::RegisterMenus));
}

void FPBRTextureLabEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PBRTextureLab::GetNomadTabId());
	}
	FPBRTextureLabCommands::Unregister();
	CommandList.Reset();
}

TSharedRef<SDockTab> FPBRTextureLabEditorModule::SpawnNomadTab(const FSpawnTabArgs& Args)
{
	(void)Args;
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("NomadTabTitle", "PBR Texture Lab"))
		[
			SNew(SPBRTextureLabNomad)
		];
}

void FPBRTextureLabEditorModule::BindCommands()
{
	CommandList = MakeShared<FUICommandList>();
	const FPBRTextureLabCommands& Commands = FPBRTextureLabCommands::Get();
	CommandList->MapAction(
		Commands.UVScale100,
		FExecuteAction::CreateStatic(&PBRTextureLab::ExecuteRegisteredUVCommand, PBRTextureLab::EPBRUVPreset::Scale100));
	CommandList->MapAction(
		Commands.UVScale500,
		FExecuteAction::CreateStatic(&PBRTextureLab::ExecuteRegisteredUVCommand, PBRTextureLab::EPBRUVPreset::Scale500));
	CommandList->MapAction(
		Commands.OpenNomadTab,
		FExecuteAction::CreateStatic(&PBRTextureLab::InvokeNomadTab));

	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.GetGlobalLevelEditorActions()->Append(CommandList.ToSharedRef());
}

void FPBRTextureLabEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	const FPBRTextureLabCommands& Commands = FPBRTextureLabCommands::Get();
	const FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), TEXT("ClassIcon.StaticMesh"));

	if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
	{
		FToolMenuSection& Section = ToolsMenu->FindOrAddSection(
			"PBRTextureLab",
			LOCTEXT("ToolsSection", "PBR Texture Lab"));
		Section.AddMenuEntryWithCommandList(Commands.UVScale100, CommandList, TAttribute<FText>(), TAttribute<FText>(), Icon);
		Section.AddMenuEntryWithCommandList(Commands.UVScale500, CommandList, TAttribute<FText>(), TAttribute<FText>(), Icon);
		Section.AddMenuEntryWithCommandList(Commands.OpenNomadTab, CommandList);
	}

	if (UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User"))
	{
		FToolMenuSection& Section = Toolbar->FindOrAddSection("PBRTextureLab");
		Section.AddMenuEntryWithCommandList(Commands.UVScale100, CommandList, TAttribute<FText>(), TAttribute<FText>(), Icon);
		Section.AddMenuEntryWithCommandList(Commands.UVScale500, CommandList, TAttribute<FText>(), TAttribute<FText>(), Icon);
		Section.AddMenuEntryWithCommandList(Commands.OpenNomadTab, CommandList);
	}

	if (UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu"))
	{
		FToolMenuSection& Section = AssetMenu->FindOrAddSection(
			"PBRTextureLab",
			LOCTEXT("AssetSection", "PBR Texture Lab"));
		Section.AddMenuEntryWithCommandList(Commands.UVScale100, CommandList, TAttribute<FText>(), TAttribute<FText>(), Icon);
		Section.AddMenuEntryWithCommandList(Commands.UVScale500, CommandList, TAttribute<FText>(), TAttribute<FText>(), Icon);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPBRTextureLabEditorModule, PBRTextureLabEditor)
