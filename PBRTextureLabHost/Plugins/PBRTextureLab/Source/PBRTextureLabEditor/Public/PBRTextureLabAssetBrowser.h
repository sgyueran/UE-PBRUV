#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PBRTextureLabAssetBrowser.generated.h"

/** Common editor asset groups exposed by the PBR Texture Lab asset browser. */
UENUM(BlueprintType)
enum class EPBRTextureLabAssetCategory : uint8
{
	All UMETA(DisplayName = "All"),
	Blueprints UMETA(DisplayName = "Blueprints"),
	BlueprintInterfaces UMETA(DisplayName = "Blueprint Interfaces"),
	Materials UMETA(DisplayName = "Materials"),
	Models UMETA(DisplayName = "Models"),
	Textures UMETA(DisplayName = "Textures"),
	Animations UMETA(DisplayName = "Animations"),
	Audio UMETA(DisplayName = "Audio"),
	Data UMETA(DisplayName = "Data")
};

/**
 * Blueprint-facing editor asset browser interface.
 *
 * This is intentionally backed by UE's Content Browser and Asset Registry rather
 * than YeCloud private types, so it remains usable when YeCloud source is absent.
 */
UCLASS()
class PBRTEXTURELABEDITOR_API UPBRTextureLabAssetBrowserLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "PBR Texture Lab|Asset Browser")
	static void OpenAssetBrowser();

	UFUNCTION(BlueprintPure, Category = "PBR Texture Lab|Asset Browser")
	static TArray<FString> GetSelectedAssetObjectPaths();

	UFUNCTION(BlueprintCallable, Category = "PBR Texture Lab|Asset Browser")
	static void SyncSelectedAssetsToContentBrowser();

	UFUNCTION(BlueprintCallable, Category = "PBR Texture Lab|Asset Browser")
	static bool OpenAssetByObjectPath(const FString& ObjectPath);

	UFUNCTION(BlueprintPure, Category = "PBR Texture Lab|Asset Browser")
	static TArray<FString> FindAssetObjectPaths(EPBRTextureLabAssetCategory Category, const FString& PackagePath);
};

namespace PBRTextureLabAssetBrowser
{
	PBRTEXTURELABEDITOR_API FName GetTabId();
	PBRTEXTURELABEDITOR_API bool IsTabRegistered();
	PBRTEXTURELABEDITOR_API void InvokeTab();
}
