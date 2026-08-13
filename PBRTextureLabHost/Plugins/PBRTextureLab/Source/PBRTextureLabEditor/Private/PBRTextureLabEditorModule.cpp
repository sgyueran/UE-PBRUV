#include "PBRTextureLabEditorModule.h"
#include "PBRTextureLabCompat.h"

#define LOCTEXT_NAMESPACE "FPBRTextureLabEditorModule"

void FPBRTextureLabEditorModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("PBRTextureLabEditor started on UE %d.%d.%d"),
		PBRTEXTURELAB_ENGINE_MAJOR,
		PBRTEXTURELAB_ENGINE_MINOR,
		PBRTEXTURELAB_ENGINE_PATCH);
}

void FPBRTextureLabEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPBRTextureLabEditorModule, PBRTextureLabEditor)
