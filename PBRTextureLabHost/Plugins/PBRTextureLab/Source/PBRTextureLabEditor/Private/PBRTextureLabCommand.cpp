#include "PBRTextureLabCommand.h"
#include "PBRTextureLabCommands.h"
#include "PBRTextureLabCompat.h"
#include "PBRTextureLabEditorApi.h"
#include "PBRTextureLabPixelCore.h"

#include "AssetRegistry/AssetData.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "UObject/UObjectIterator.h"

namespace PBRTextureLab
{
	namespace
	{
		void CommandSetWarning(FString* OutError, const FString& Message)
		{
			UE_LOG(LogPBRTextureLab, Warning, TEXT("%s"), *Message);
			if (OutError)
			{
				*OutError = Message;
			}
		}

		void CommandSetError(FString* OutError, const FString& Message)
		{
			UE_LOG(LogPBRTextureLab, Error, TEXT("%s"), *Message);
			if (OutError)
			{
				*OutError = Message;
			}
		}

		bool CommandIsGameThread(FString* OutError)
		{
			if (IsInGameThread())
			{
				return true;
			}
			CommandSetError(OutError, TEXT("PBR UV commands must run on the Game Thread."));
			return false;
		}

		UStaticMesh* CommandComponentMesh(const UStaticMeshComponent* Component)
		{
			return Component ? Component->GetStaticMesh() : nullptr;
		}

		void AddComponentToSelection(FPBRUVSelection& Selection, UStaticMeshComponent* Component)
		{
			UStaticMesh* Mesh = CommandComponentMesh(Component);
			if (!Component || !Mesh)
			{
				return;
			}

			for (FPBRUVSelectionItem& Item : Selection.Items)
			{
				if (Item.Mesh == Mesh)
				{
					Item.SelectedComponents.AddUnique(Component);
					return;
				}
			}

			FPBRUVSelectionItem Item;
			Item.Mesh = Mesh;
			Item.SelectedComponents.Add(Component);
			Selection.Items.Add(MoveTemp(Item));
		}

		void AddAssetToSelection(FPBRUVSelection& Selection, UStaticMesh* Mesh)
		{
			if (!Mesh)
			{
				return;
			}
			for (const FPBRUVSelectionItem& Item : Selection.Items)
			{
				if (Item.Mesh == Mesh)
				{
					return;
				}
			}
			FPBRUVSelectionItem Item;
			Item.Mesh = Mesh;
			Selection.Items.Add(MoveTemp(Item));
		}

		FText MakePromptText(const FPBRUVSelection& Selection, const EPBRUVPreset Preset)
		{
			const int32 Scale = static_cast<int32>(Preset);
			FString Body = FString::Printf(TEXT("Apply absolute UV0 %dx?\n\n"), Scale);
			Body += TEXT("Yes: Duplicate the mesh and rebind only the selected component(s).\n");
			Body += TEXT("No: Modify the source asset. Shared component count:\n");
			for (const FPBRUVSelectionItem& Item : Selection.Items)
			{
				Body += FString::Printf(
					TEXT("  %s -> %d component(s)\n"),
					*Item.Mesh->GetName(),
					CountStaticMeshComponentUsers(Item.Mesh));
			}
			Body += TEXT("\nCancel: Do nothing.");
			return FText::FromString(Body);
		}

		EPBRUVEditChoice ResolveEditChoice(
			const FPBRUVCommandRequest& Request,
			const FPBRUVSelection& Selection,
			FString* OutError)
		{
			if (Request.EditChoice != EPBRUVEditChoice::Prompt)
			{
				return Request.EditChoice;
			}

			const FText Title = NSLOCTEXT("PBRTextureLab", "UVCommandTitle", "PBR Texture Lab UV");
			const FText Message = MakePromptText(Selection, Request.Preset);
			const EAppReturnType::Type Answer = FApp::IsUnattended()
				? FMessageDialog::Open(EAppMsgType::YesNoCancel, EAppReturnType::Cancel, Message, Title)
				: FMessageDialog::Open(EAppMsgType::YesNoCancel, Message, Title);

			switch (Answer)
			{
			case EAppReturnType::Yes:
				return EPBRUVEditChoice::DuplicateAndRebind;
			case EAppReturnType::No:
				return EPBRUVEditChoice::ModifySource;
			default:
				if (OutError)
				{
					*OutError = TEXT("UV command cancelled.");
				}
				return EPBRUVEditChoice::Cancel;
			}
		}

		UStaticMesh* DuplicateMeshForCommand(UStaticMesh* Source, FString* OutError)
		{
			const FString SourcePackage = Source->GetOutermost()->GetName();
			if (!SourcePackage.StartsWith(TEXT("/Game")))
			{
				CommandSetError(OutError, FString::Printf(
					TEXT("Duplicate requires a /Game Static Mesh asset: %s"), *Source->GetPathName()));
				return nullptr;
			}

			FString UniquePackage;
			FString UniqueName;
			GetAssetTools().CreateUniqueAssetName(SourcePackage, TEXT("_UV"), UniquePackage, UniqueName);
			UStaticMesh* Copy = Cast<UStaticMesh>(GetAssetTools().DuplicateAsset(
				UniqueName,
				FPackageName::GetLongPackagePath(UniquePackage),
				Source));
			if (!Copy)
			{
				CommandSetError(OutError, FString::Printf(TEXT("Failed to duplicate %s"), *Source->GetPathName()));
			}
			return Copy;
		}

		EPBRUVCommandStatus ApplyToMesh(
			UStaticMesh* Mesh,
			const EPBRUVPreset Preset,
			const bool bSave,
			FString* OutError)
		{
			FPBRUVScaleRequest ScaleRequest;
			ScaleRequest.bSave = bSave;
			ScaleRequest.bTransact = false;
			const EPBRUVStatus Status = ApplyUVPreset(Mesh, Preset, ScaleRequest, OutError);
			switch (Status)
			{
			case EPBRUVStatus::Success:
				return EPBRUVCommandStatus::Success;
			case EPBRUVStatus::Cancelled:
				return EPBRUVCommandStatus::Cancelled;
			case EPBRUVStatus::BaselineMismatch:
				return EPBRUVCommandStatus::BaselineMismatch;
			case EPBRUVStatus::Unsupported:
				return EPBRUVCommandStatus::Unsupported;
			default:
				return EPBRUVCommandStatus::Failed;
			}
		}
	}

	FName GetUVCommandContextName()
	{
		return TEXT("PBRTextureLab");
	}

	FName GetUVScale100CommandName()
	{
		return TEXT("UVScale100");
	}

	FName GetUVScale500CommandName()
	{
		return TEXT("UVScale500");
	}

	int32 CountStaticMeshComponentUsers(const UStaticMesh* Mesh)
	{
		if (!Mesh)
		{
			return 0;
		}

		int32 Count = 0;
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			UStaticMeshComponent* Component = *It;
			if (!Component || Component->HasAnyFlags(RF_ClassDefaultObject) || !IsValid(Component))
			{
				continue;
			}
			if (CommandComponentMesh(Component) == Mesh)
			{
				++Count;
			}
		}
		return Count;
	}

	FPBRUVSelection GatherUVSelection()
	{
		FPBRUVSelection Selection;

		if (USelection* ActorSelection = GetSelectedActors())
		{
			for (int32 Index = 0; Index < ActorSelection->Num(); ++Index)
			{
				AActor* Actor = Cast<AActor>(ActorSelection->GetSelectedObject(Index));
				if (!Actor)
				{
					continue;
				}
				++Selection.ConsideredCount;

				if (AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor))
				{
					AddComponentToSelection(Selection, MeshActor->GetStaticMeshComponent());
					continue;
				}

				TArray<UStaticMeshComponent*> ActorComponents;
				Actor->GetComponents<UStaticMeshComponent>(ActorComponents);
				if (ActorComponents.Num() == 0)
				{
					++Selection.NonStaticMeshCount;
					continue;
				}
				for (UStaticMeshComponent* Component : ActorComponents)
				{
					AddComponentToSelection(Selection, Component);
				}
			}
		}

		if (USelection* ComponentSelection = GetSelectedComponents())
		{
			for (int32 Index = 0; Index < ComponentSelection->Num(); ++Index)
			{
				UObject* Object = ComponentSelection->GetSelectedObject(Index);
				if (!Object)
				{
					continue;
				}
				if (UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Object))
				{
					++Selection.ConsideredCount;
					AddComponentToSelection(Selection, Component);
				}
				else
				{
					++Selection.ConsideredCount;
					++Selection.NonStaticMeshCount;
				}
			}
		}

		if (Selection.Items.Num() == 0)
		{
			TArray<FAssetData> BrowserAssets;
			GetContentBrowserSelections(BrowserAssets);
			for (const FAssetData& Asset : BrowserAssets)
			{
				++Selection.ConsideredCount;
				if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset()))
				{
					AddAssetToSelection(Selection, Mesh);
				}
				else
				{
					++Selection.NonStaticMeshCount;
				}
			}
		}

		return Selection;
	}

	EPBRUVCommandStatus ExecuteUVCommand(const FPBRUVCommandRequest& Request, FString* OutError)
	{
		return ExecuteUVCommand(Request, GatherUVSelection(), OutError);
	}

	EPBRUVCommandStatus ExecuteUVCommand(
		const FPBRUVCommandRequest& Request,
		const FPBRUVSelection& Selection,
		FString* OutError)
	{
		if (!CommandIsGameThread(OutError))
		{
			return EPBRUVCommandStatus::Failed;
		}

		if (Selection.Items.Num() == 0 && Selection.ConsideredCount == 0)
		{
			CommandSetWarning(OutError, TEXT("PBRTextureLab UV command: empty selection."));
			return EPBRUVCommandStatus::EmptySelection;
		}

		if (Selection.Items.Num() == 0)
		{
			CommandSetWarning(OutError, TEXT("PBRTextureLab UV command: selection is not a Static Mesh."));
			return EPBRUVCommandStatus::Unsupported;
		}

		if (Selection.NonStaticMeshCount > 0)
		{
			CommandSetWarning(OutError, TEXT("PBRTextureLab UV command: mixed selection. Select only Static Mesh actors, components, or assets."));
			return EPBRUVCommandStatus::MixedSelection;
		}

		const EPBRUVEditChoice Choice = ResolveEditChoice(Request, Selection, OutError);
		if (Choice == EPBRUVEditChoice::Cancel)
		{
			return EPBRUVCommandStatus::Cancelled;
		}

		FScopedTransaction Transaction(NSLOCTEXT("PBRTextureLab", "UVCommand", "PBR Texture Lab UV Command"));

		for (const FPBRUVSelectionItem& Item : Selection.Items)
		{
			if (!Item.Mesh)
			{
				CommandSetError(OutError, TEXT("Selection contained a null Static Mesh."));
				return EPBRUVCommandStatus::Failed;
			}

			if (Choice == EPBRUVEditChoice::DuplicateAndRebind)
			{
				UStaticMesh* Copy = DuplicateMeshForCommand(Item.Mesh, OutError);
				if (!Copy)
				{
					return EPBRUVCommandStatus::Failed;
				}

				const EPBRUVCommandStatus ApplyStatus = ApplyToMesh(Copy, Request.Preset, Request.bSave, OutError);
				if (ApplyStatus != EPBRUVCommandStatus::Success)
				{
					return ApplyStatus;
				}

				for (UStaticMeshComponent* Component : Item.SelectedComponents)
				{
					if (!Component)
					{
						continue;
					}
					Component->Modify();
					if (AActor* Owner = Component->GetOwner())
					{
						Owner->Modify();
					}
					Component->SetStaticMesh(Copy);
				}

				UE_LOG(
					LogPBRTextureLab,
					Log,
					TEXT("Duplicated %s -> %s and rebound %d selected component(s)."),
					*Item.Mesh->GetName(),
					*Copy->GetName(),
					Item.SelectedComponents.Num());
			}
			else
			{
				const int32 Shared = CountStaticMeshComponentUsers(Item.Mesh);
				UE_LOG(
					LogPBRTextureLab,
					Log,
					TEXT("Modifying source %s used by %d component(s)."),
					*Item.Mesh->GetPathName(),
					Shared);
				const EPBRUVCommandStatus ApplyStatus = ApplyToMesh(Item.Mesh, Request.Preset, Request.bSave, OutError);
				if (ApplyStatus != EPBRUVCommandStatus::Success)
				{
					return ApplyStatus;
				}
			}
		}

		return EPBRUVCommandStatus::Success;
	}

	void ExecuteRegisteredUVCommand(const EPBRUVPreset Preset)
	{
		FPBRUVCommandRequest Request;
		Request.Preset = Preset;
		Request.EditChoice = EPBRUVEditChoice::Prompt;
		FString Error;
		ExecuteUVCommand(Request, &Error);
	}
}
