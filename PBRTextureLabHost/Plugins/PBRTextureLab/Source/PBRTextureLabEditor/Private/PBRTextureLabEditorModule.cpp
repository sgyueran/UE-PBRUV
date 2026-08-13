#include "PBRTextureLabEditorModule.h"
#include "PBRTextureLabCompat.h"
#include "PBRTextureLabPixelCore.h"

#define LOCTEXT_NAMESPACE "FPBRTextureLabEditorModule"

void FPBRTextureLabEditorModule::StartupModule()
{
	UE_LOG(LogPBRTextureLab, Log, TEXT("PBRTextureLabEditor started on UE %d.%d.%d"),
		PBRTEXTURELAB_ENGINE_MAJOR,
		PBRTEXTURELAB_ENGINE_MINOR,
		PBRTEXTURELAB_ENGINE_PATCH);
	UE_LOG(LogPBRTextureLab, Warning, TEXT("%s"), PBRTextureLab::MetallicDisclaimer);
}

void FPBRTextureLabEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPBRTextureLabEditorModule, PBRTextureLabEditor)
