#include "PBRTextureLabAssetBrowser.h"
#include "PBRTextureLabAssetBrowserUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/SoftObjectPath.h"

void UPBRTextureLabAssetBrowserLibrary::OpenAssetBrowser()
{
	PBRTextureLabAssetBrowser::InvokeTab();
}

TArray<FString> UPBRTextureLabAssetBrowserLibrary::GetSelectedAssetObjectPaths()
{
	TArray<FString> Result;
	TArray<FAssetData> SelectedAssets;
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	Result.Reserve(SelectedAssets.Num());
	for (const FAssetData& AssetData : SelectedAssets)
	{
		Result.Add(AssetData.GetObjectPathString());
	}
	return Result;
}

void UPBRTextureLabAssetBrowserLibrary::SyncSelectedAssetsToContentBrowser()
{
	TArray<FAssetData> SelectedAssets;
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
	if (SelectedAssets.Num() > 0)
	{
		ContentBrowserModule.Get().SyncBrowserToAssets(SelectedAssets, false, true);
	}
}

bool UPBRTextureLabAssetBrowserLibrary::OpenAssetByObjectPath(const FString& ObjectPath)
{
	if (!GEditor || ObjectPath.IsEmpty())
	{
		return false;
	}

	UObject* Asset = FSoftObjectPath(ObjectPath).TryLoad();
	if (!Asset)
	{
		return false;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	return AssetEditorSubsystem && AssetEditorSubsystem->OpenEditorForAsset(Asset);
}

TArray<FString> UPBRTextureLabAssetBrowserLibrary::FindAssetObjectPaths(
	const EPBRTextureLabAssetCategory Category,
	const FString& PackagePath)
{
	TArray<FString> Result;
	FARFilter Filter = PBRTextureLabAssetBrowser::MakeFilter(Category);
	if (!PackagePath.IsEmpty())
	{
		Filter.PackagePaths.Add(*PackagePath);
		Filter.bRecursivePaths = true;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);
	for (const FAssetData& AssetData : Assets)
	{
		if (!PBRTextureLabAssetBrowser::ShouldFilterAsset(Category, AssetData))
		{
			Result.Add(AssetData.GetObjectPathString());
		}
	}
	Result.Sort();
	return Result;
}

namespace PBRTextureLabAssetBrowser
{
	FName GetTabId()
	{
		static const FName TabId(TEXT("PBRTextureLabAssetBrowser"));
		return TabId;
	}

	bool IsTabRegistered()
	{
		return FGlobalTabmanager::Get()->HasTabSpawner(GetTabId());
	}

	void InvokeTab()
	{
		if (IsTabRegistered())
		{
			FGlobalTabmanager::Get()->TryInvokeTab(GetTabId());
		}
	}
}
