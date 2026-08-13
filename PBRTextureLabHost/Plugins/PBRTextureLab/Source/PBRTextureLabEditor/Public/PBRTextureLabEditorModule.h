#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"

class FUICommandList;
class SDockTab;

class FPBRTextureLabEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void BindCommands();
	TSharedRef<SDockTab> SpawnNomadTab(const FSpawnTabArgs& Args);

	TSharedPtr<FUICommandList> CommandList;
};
