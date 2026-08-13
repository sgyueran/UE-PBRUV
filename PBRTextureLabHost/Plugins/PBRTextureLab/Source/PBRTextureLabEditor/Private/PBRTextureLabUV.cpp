#include "PBRTextureLabUV.h"
#include "PBRTextureLabCompat.h"
#include "PBRTextureLabEditorApi.h"
#include "PBRTextureLabPixelCore.h"
#include "PBRTextureLabUVPresetData.h"

#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "Hash/CityHash.h"
#include "MeshDescription.h"
#include "ScopedTransaction.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"

namespace PBRTextureLab
{
	namespace
	{
		void UvSetError(FString* OutError, const FString& Message)
		{
			UE_LOG(LogPBRTextureLab, Error, TEXT("%s"), *Message);
			if (OutError)
			{
				*OutError = Message;
			}
		}

		void UvSetWarning(FString* OutError, const FString& Message)
		{
			UE_LOG(LogPBRTextureLab, Warning, TEXT("%s"), *Message);
			if (OutError)
			{
				*OutError = Message;
			}
		}

		bool UvIsGameThread(FString* OutError)
		{
			if (IsInGameThread())
			{
				return true;
			}
			UvSetError(OutError, TEXT("PBR UV APIs must run on the Game Thread."));
			return false;
		}

		bool IsSupportedPreset(const int32 Scale)
		{
			return Scale == PBRTEXTURELAB_UV_PRESET_100 || Scale == PBRTEXTURELAB_UV_PRESET_500;
		}

		void MixHash(uint64& Hash, const void* Data, const int32 Size)
		{
			if (Data && Size > 0)
			{
				Hash = CityHash64WithSeed(static_cast<const char*>(Data), static_cast<uint32>(Size), Hash);
			}
		}

		template <typename T>
		void MixValue(uint64& Hash, const T& Value)
		{
			MixHash(Hash, &Value, sizeof(T));
		}

		uint64 HashTopology(const FMeshDescription& MeshDescription)
		{
			uint64 Hash = 0xC0FFEEULL;
			const int32 VertexCount = MeshDescription.Vertices().Num();
			const int32 InstanceCount = MeshDescription.VertexInstances().Num();
			const int32 TriangleCount = MeshDescription.Triangles().Num();
			MixValue(Hash, VertexCount);
			MixValue(Hash, InstanceCount);
			MixValue(Hash, TriangleCount);

			for (const FVertexInstanceID VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs())
			{
				const int32 InstanceIndex = VertexInstanceID.GetValue();
				const int32 VertexIndex = MeshDescription.GetVertexInstanceVertex(VertexInstanceID).GetValue();
				MixValue(Hash, InstanceIndex);
				MixValue(Hash, VertexIndex);
			}

			for (const FTriangleID TriangleID : MeshDescription.Triangles().GetElementIDs())
			{
				MixValue(Hash, TriangleID.GetValue());
				for (const FVertexInstanceID VertexInstanceID : MeshDescription.GetTriangleVertexInstances(TriangleID))
				{
					MixValue(Hash, VertexInstanceID.GetValue());
				}
			}
			return Hash;
		}

		uint64 HashUvChannel(
			const TVertexInstanceAttributesConstRef<FVector2f>& UVs,
			const FMeshDescription& MeshDescription,
			const int32 Channel)
		{
			uint64 Hash = 0x550F00DULL;
			MixValue(Hash, Channel);
			if (!UVs.IsValid() || Channel < 0 || Channel >= UVs.GetNumChannels())
			{
				return Hash;
			}

			for (const FVertexInstanceID VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs())
			{
				const FVector2f Value = UVs.Get(VertexInstanceID, Channel);
				MixValue(Hash, Value);
			}
			return Hash;
		}

		uint64 HashScaledUVs(const TArray<FVector2f>& BaselineUVs, const int32 Scale)
		{
			uint64 Hash = 0x550F00DULL;
			MixValue(Hash, PBRTEXTURELAB_UV_CHANNEL_INDEX);
			const float ScaleValue = static_cast<float>(Scale);
			for (const FVector2f& Baseline : BaselineUVs)
			{
				const FVector2f Scaled(Baseline.X * ScaleValue, Baseline.Y * ScaleValue);
				MixValue(Hash, Scaled);
			}
			return Hash;
		}

		bool CollectUv0(
			const TVertexInstanceAttributesConstRef<FVector2f>& UVs,
			const FMeshDescription& MeshDescription,
			TArray<FVector2f>& OutUVs,
			FString* OutError)
		{
			OutUVs.Reset();
			if (!UVs.IsValid() || UVs.GetNumChannels() <= PBRTEXTURELAB_UV_CHANNEL_INDEX)
			{
				UvSetError(OutError, TEXT("Static Mesh has no UV0 channel."));
				return false;
			}

			OutUVs.Reserve(MeshDescription.VertexInstances().Num());
			for (const FVertexInstanceID VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs())
			{
				OutUVs.Add(UVs.Get(VertexInstanceID, PBRTEXTURELAB_UV_CHANNEL_INDEX));
			}
			return true;
		}

		bool WriteUv0(
			const TVertexInstanceAttributesRef<FVector2f>& UVs,
			const FMeshDescription& MeshDescription,
			const TArray<FVector2f>& BaselineUVs,
			const int32 Scale)
		{
			if (!UVs.IsValid() || UVs.GetNumChannels() <= PBRTEXTURELAB_UV_CHANNEL_INDEX)
			{
				return false;
			}

			const float ScaleValue = static_cast<float>(Scale);
			int32 Index = 0;
			for (const FVertexInstanceID VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs())
			{
				if (!BaselineUVs.IsValidIndex(Index))
				{
					return false;
				}
				UVs.Set(
					VertexInstanceID,
					PBRTEXTURELAB_UV_CHANNEL_INDEX,
					FVector2f(BaselineUVs[Index].X * ScaleValue, BaselineUVs[Index].Y * ScaleValue));
				++Index;
			}
			return Index == BaselineUVs.Num();
		}

		UPBRTextureLabUVPresetData* FindUvData(const UStaticMesh* Mesh)
		{
			if (!Mesh)
			{
				return nullptr;
			}
			// UStaticMesh::GetAssetUserDataOfClass is not const on 5.6-5.8.
			return Cast<UPBRTextureLabUVPresetData>(
				const_cast<UStaticMesh*>(Mesh)->GetAssetUserDataOfClass(UPBRTextureLabUVPresetData::StaticClass()));
		}

		UPBRTextureLabUVPresetData* GetOrCreateUvData(UStaticMesh* Mesh)
		{
			if (UPBRTextureLabUVPresetData* Existing = FindUvData(Mesh))
			{
				return Existing;
			}

			UPBRTextureLabUVPresetData* Created = NewObject<UPBRTextureLabUVPresetData>(
				Mesh,
				NAME_None,
				RF_Transactional);
			Mesh->AddAssetUserData(Created);
			return Created;
		}

		bool CaptureLodRecord(
			UStaticMesh* Mesh,
			const int32 LodIndex,
			FPBRTextureLabUVLodRecord& OutRecord,
			FString* OutError)
		{
			FMeshDescription* MeshDescription = GetStaticMeshDescription(Mesh, LodIndex);
			if (!MeshDescription)
			{
				UvSetError(OutError, FString::Printf(TEXT("LOD %d has no editable mesh description."), LodIndex));
				return false;
			}

			const TVertexInstanceAttributesRef<FVector2f> UVs = GetVertexInstanceUVs(*MeshDescription);
			if (!CollectUv0(UVs, *MeshDescription, OutRecord.BaselineUVs, OutError))
			{
				return false;
			}

			OutRecord.LodIndex = LodIndex;
			OutRecord.TopologyFingerprint = HashTopology(*MeshDescription);
			OutRecord.BaselineUvFingerprint = HashUvChannel(UVs, *MeshDescription, PBRTEXTURELAB_UV_CHANNEL_INDEX);
			return true;
		}

		bool CaptureAllLods(UStaticMesh* Mesh, TArray<FPBRTextureLabUVLodRecord>& OutRecords, FString* OutError)
		{
			OutRecords.Reset();
			const int32 LodCount = GetStaticMeshSourceModelCount(Mesh);
			if (LodCount <= 0)
			{
				UvSetError(OutError, TEXT("Static Mesh has no source LODs."));
				return false;
			}

			int32 EditableLods = 0;
			for (int32 LodIndex = 0; LodIndex < LodCount; ++LodIndex)
			{
				if (!IsStaticMeshDescriptionValid(Mesh, LodIndex))
				{
					continue;
				}
				FPBRTextureLabUVLodRecord Record;
				if (!CaptureLodRecord(Mesh, LodIndex, Record, OutError))
				{
					return false;
				}
				OutRecords.Add(MoveTemp(Record));
				++EditableLods;
			}

			if (EditableLods == 0)
			{
				UvSetError(OutError, TEXT("Static Mesh has no editable source LODs."));
				return false;
			}
			return true;
		}

		bool CurrentMatchesRecord(
			UStaticMesh* Mesh,
			const FPBRTextureLabUVLodRecord& Record,
			const int32 AppliedScale,
			FString* OutError)
		{
			FMeshDescription* MeshDescription = GetStaticMeshDescription(Mesh, Record.LodIndex);
			if (!MeshDescription)
			{
				UvSetError(OutError, FString::Printf(TEXT("LOD %d is no longer editable."), Record.LodIndex));
				return false;
			}

			const uint64 TopologyFingerprint = HashTopology(*MeshDescription);
			if (TopologyFingerprint != Record.TopologyFingerprint)
			{
				UvSetWarning(OutError, FString::Printf(
					TEXT("LOD %d topology changed; re-establish the UV baseline."),
					Record.LodIndex));
				return false;
			}

			const TVertexInstanceAttributesRef<FVector2f> UVs = GetVertexInstanceUVs(*MeshDescription);
			const uint64 CurrentUvFingerprint = HashUvChannel(UVs, *MeshDescription, PBRTEXTURELAB_UV_CHANNEL_INDEX);
			const uint64 ExpectedUvFingerprint = AppliedScale <= 1
				? Record.BaselineUvFingerprint
				: HashScaledUVs(Record.BaselineUVs, AppliedScale);
			if (CurrentUvFingerprint != ExpectedUvFingerprint)
			{
				UvSetWarning(OutError, FString::Printf(
					TEXT("LOD %d UV0 changed; re-establish the UV baseline."),
					Record.LodIndex));
				return false;
			}
			return true;
		}

		bool WriteAllLods(
			UStaticMesh* Mesh,
			const TArray<FPBRTextureLabUVLodRecord>& Records,
			const int32 Scale,
			FString* OutError)
		{
			for (const FPBRTextureLabUVLodRecord& Record : Records)
			{
				if (!ModifyStaticMeshDescription(Mesh, Record.LodIndex))
				{
					UvSetError(OutError, FString::Printf(TEXT("Failed to transact LOD %d."), Record.LodIndex));
					return false;
				}

				FMeshDescription* MeshDescription = GetStaticMeshDescription(Mesh, Record.LodIndex);
				if (!MeshDescription)
				{
					UvSetError(OutError, FString::Printf(TEXT("LOD %d lost its mesh description."), Record.LodIndex));
					return false;
				}

				const TVertexInstanceAttributesRef<FVector2f> UVs = GetVertexInstanceUVs(*MeshDescription);
				if (!WriteUv0(UVs, *MeshDescription, Record.BaselineUVs, Scale))
				{
					UvSetError(OutError, FString::Printf(TEXT("Failed to write UV0 on LOD %d."), Record.LodIndex));
					return false;
				}
				CommitStaticMeshDescription(Mesh, Record.LodIndex);
			}
			return true;
		}

		bool SaveMeshIfRequested(UStaticMesh* Mesh, const bool bSave, FString* OutError)
		{
			if (!bSave)
			{
				return true;
			}

			TArray<UPackage*> Packages;
			Packages.Add(Mesh->GetOutermost());
			if (!UEditorLoadingAndSavingUtils::SavePackages(Packages, false))
			{
				UvSetError(OutError, FString::Printf(TEXT("Failed to save %s"), *Mesh->GetPathName()));
				return false;
			}
			return true;
		}

		bool ValidateMesh(UStaticMesh* Mesh, FString* OutError)
		{
			if (!Mesh)
			{
				UvSetWarning(OutError, TEXT("ApplyUVPreset requires a Static Mesh."));
				return false;
			}

			if (GetStaticMeshSourceModelCount(Mesh) <= 0)
			{
				UvSetWarning(OutError, TEXT("Static Mesh has no source LODs."));
				return false;
			}

			if (GetLightMapUVChannel(Mesh) == PBRTEXTURELAB_UV_CHANNEL_INDEX)
			{
				UvSetWarning(OutError, TEXT("Refusing to modify UV0 because it is the Lightmap UV channel."));
				return false;
			}
			return true;
		}
	}

	int32 GetAppliedUVScale(const UStaticMesh* Mesh)
	{
		if (const UPBRTextureLabUVPresetData* Data = FindUvData(Mesh))
		{
			return Data->AppliedScale;
		}
		return 0;
	}

	EPBRUVStatus EstablishUVBaseline(UStaticMesh* Mesh, FString* OutError)
	{
		if (!UvIsGameThread(OutError))
		{
			return EPBRUVStatus::Failed;
		}
		if (!ValidateMesh(Mesh, OutError))
		{
			return EPBRUVStatus::Unsupported;
		}

		TArray<FPBRTextureLabUVLodRecord> Records;
		if (!CaptureAllLods(Mesh, Records, OutError))
		{
			return EPBRUVStatus::Unsupported;
		}

		FScopedTransaction Transaction(NSLOCTEXT("PBRTextureLab", "EstablishUVBaseline", "PBR Texture Lab UV Baseline"));
		Mesh->Modify();
		UPBRTextureLabUVPresetData* Data = GetOrCreateUvData(Mesh);
		Data->Modify();
		Data->AppliedScale = 1;
		Data->Lods = MoveTemp(Records);
		Mesh->MarkPackageDirty();
		return EPBRUVStatus::Success;
	}

	EPBRUVStatus ApplyUVPreset(
		UStaticMesh* Mesh,
		const EPBRUVPreset Preset,
		const FPBRUVScaleRequest& Request,
		FString* OutError)
	{
		if (!UvIsGameThread(OutError))
		{
			return EPBRUVStatus::Failed;
		}

		if (Request.bCancelled)
		{
			if (OutError)
			{
				*OutError = TEXT("UV preset cancelled.");
			}
			return EPBRUVStatus::Cancelled;
		}

		const int32 Scale = static_cast<int32>(Preset);
		if (!IsSupportedPreset(Scale))
		{
			UvSetError(OutError, FString::Printf(TEXT("Unsupported UV preset %d."), Scale));
			return EPBRUVStatus::Failed;
		}

		if (!ValidateMesh(Mesh, OutError))
		{
			return EPBRUVStatus::Unsupported;
		}

		TArray<FPBRTextureLabUVLodRecord> Records;
		UPBRTextureLabUVPresetData* Existing = FindUvData(Mesh);
		if (Request.bRebaseline || !Existing || Existing->AppliedScale <= 0 || Existing->Lods.Num() == 0)
		{
			if (!CaptureAllLods(Mesh, Records, OutError))
			{
				return EPBRUVStatus::Unsupported;
			}
		}
		else
		{
			for (const FPBRTextureLabUVLodRecord& Record : Existing->Lods)
			{
				if (!CurrentMatchesRecord(Mesh, Record, Existing->AppliedScale, OutError))
				{
					return EPBRUVStatus::BaselineMismatch;
				}
			}
			Records = Existing->Lods;
		}

		FScopedTransaction Transaction(NSLOCTEXT("PBRTextureLab", "ApplyUVPreset", "PBR Texture Lab UV Preset"), Request.bTransact);
		Mesh->Modify();
		UPBRTextureLabUVPresetData* Data = GetOrCreateUvData(Mesh);
		Data->Modify();
		Data->Lods = Records;
		if (!WriteAllLods(Mesh, Data->Lods, Scale, OutError))
		{
			return EPBRUVStatus::Failed;
		}
		Data->AppliedScale = Scale;
		Mesh->MarkPackageDirty();

		if (!SaveMeshIfRequested(Mesh, Request.bSave, OutError))
		{
			return EPBRUVStatus::Failed;
		}

		UE_LOG(LogPBRTextureLab, Log, TEXT("Applied absolute UV0 %dx to %s"), Scale, *Mesh->GetPathName());
		return EPBRUVStatus::Success;
	}
}
