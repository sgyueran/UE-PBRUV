#include "PBRTextureLabNomad.h"
#include "PBRTextureLabPixelCore.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"

namespace PBRTextureLab
{
	FName GetNomadTabId()
	{
		return FName(TEXT("PBRTextureLabNomad"));
	}

	FName GetOpenNomadTabCommandName()
	{
		return FName(TEXT("OpenNomadTab"));
	}

	bool IsNomadTabRegistered()
	{
		return FGlobalTabmanager::Get()->HasTabSpawner(GetNomadTabId());
	}

	void InvokeNomadTab()
	{
		if (!FSlateApplication::IsInitialized())
		{
			UE_LOG(LogPBRTextureLab, Warning, TEXT("Cannot open PBR Texture Lab tab: Slate is not initialized."));
			return;
		}
		FGlobalTabmanager::Get()->TryInvokeTab(FTabId(GetNomadTabId()));
	}
}
