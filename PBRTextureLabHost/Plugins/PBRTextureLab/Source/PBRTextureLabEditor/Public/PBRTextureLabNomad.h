#pragma once

#include "CoreMinimal.h"

/**
 * Nomad Tab entry for PBR Texture Lab.
 * Registration lives in the editor module; these helpers are safe to call from tests.
 */
namespace PBRTextureLab
{
	FName GetNomadTabId();
	FName GetOpenNomadTabCommandName();
	bool IsNomadTabRegistered();
	void InvokeNomadTab();
}
