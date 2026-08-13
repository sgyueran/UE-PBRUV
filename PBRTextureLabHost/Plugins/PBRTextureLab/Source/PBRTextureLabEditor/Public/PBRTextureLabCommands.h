#pragma once

#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FPBRTextureLabCommands final : public TCommands<FPBRTextureLabCommands>
{
public:
	FPBRTextureLabCommands()
		: TCommands<FPBRTextureLabCommands>(
			TEXT("PBRTextureLab"),
			NSLOCTEXT("PBRTextureLab", "CommandContext", "PBR Texture Lab"),
			NAME_None,
			FAppStyle::GetAppStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> UVScale100;
	TSharedPtr<FUICommandInfo> UVScale500;
	TSharedPtr<FUICommandInfo> OpenNomadTab;
};
