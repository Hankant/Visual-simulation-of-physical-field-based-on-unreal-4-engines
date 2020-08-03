// Fill out your copyright notice in the Description page of Project Settings.

#include "PMConvertSM.h"

#include "ProceduralMeshComponent.h"
/*����Ŀ.build.cs��.uproject��plugin�����"ProceduralMeshComponent"���ͷ�ļ��Ҳ���·��*/
#include "Engine/StaticMesh.h"
#include "RawMesh/Public/RawMesh.h"
/*����Ŀ.build.cs�����"RawMesh"���ͷ�ļ��Ҳ���·���ͱ����������*/
#include "AssetRegistryModule.h"

//��д������Դ�����е�ת����̬�����尴ťOnClickConvertToStaticMesh
UStaticMesh* UPMConvertSM::ProceduralMeshConvertToStaticMesh(UProceduralMeshComponent* proMeshComp,FString outMeshName)
{
	UProceduralMeshComponent * ProMesh = proMeshComp;
	if (ProMesh != nullptr)
	{
		FString PathName = FString(TEXT("/Game/Mesh/"));
		FString PackageName = PathName + outMeshName;

		//Raw mesh data we are filling in
		FRawMesh RawMesh;
		// Materials to apply to new mesh
		TArray<UMaterialInterface*> MeshMaterials;

		const int32 NumSections = ProMesh->GetNumSections();
		int32 VertexBase = 0;
		for (int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
		{
			FProcMeshSection* ProSection = ProMesh->GetProcMeshSection(SectionIdx);

			// Copy verts
			for (FProcMeshVertex& Vert : ProSection->ProcVertexBuffer)
			{
				RawMesh.VertexPositions.Add(Vert.Position);
			}

			// Copy 'wedge' info
			int32 NumIndices = ProSection->ProcIndexBuffer.Num();
			for (int32 IndexIdx = 0; IndexIdx < NumIndices; IndexIdx++)
			{
				int32 Index = ProSection->ProcIndexBuffer[IndexIdx];

				RawMesh.WedgeIndices.Add(Index + VertexBase);

				FProcMeshVertex& ProcVertex = ProSection->ProcVertexBuffer[Index];

				FVector TangentX = ProcVertex.Tangent.TangentX;
				FVector TangentZ = ProcVertex.Normal;
				FVector TangentY = (TangentX ^ TangentZ).GetSafeNormal() * (ProcVertex.Tangent.bFlipTangentY ? -1.f : 1.f);

				RawMesh.WedgeTangentX.Add(TangentX);
				RawMesh.WedgeTangentY.Add(TangentY);
				RawMesh.WedgeTangentZ.Add(TangentZ);

				RawMesh.WedgeTexCoords[0].Add(ProcVertex.UV0);
				RawMesh.WedgeColors.Add(ProcVertex.Color);
			}

			// copy face info
			int32 NumTris = NumIndices / 3;
			for (int32 TriIdx = 0; TriIdx < NumTris; TriIdx++)
			{
				RawMesh.FaceMaterialIndices.Add(SectionIdx);
				RawMesh.FaceSmoothingMasks.Add(0); // Assume this is ignored as bRecomputeNormals is false
			}

			// Remember material
			MeshMaterials.Add(ProMesh->GetMaterial(SectionIdx));

			// Update offset for creating one big index/vertex buffer
			VertexBase += ProSection->ProcVertexBuffer.Num();

			// If we got some valid data.
			if (RawMesh.VertexPositions.Num() > 3 && RawMesh.WedgeIndices.Num() > 3)
			{
				// Then find/create it.
				UPackage* Package = CreatePackage(NULL, *PackageName);
				check(Package);

				// Create StaticMesh object
				UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, FName(*outMeshName), RF_Public | RF_Standalone);
				StaticMesh->InitResources();

				StaticMesh->LightingGuid = FGuid::NewGuid();

				// Add source to new StaticMesh
				FStaticMeshSourceModel* SrcModel = new (StaticMesh->SourceModels) FStaticMeshSourceModel();
				SrcModel->BuildSettings.bRecomputeNormals = false;
				SrcModel->BuildSettings.bRecomputeTangents = false;
				SrcModel->BuildSettings.bRemoveDegenerates = false;
				SrcModel->BuildSettings.bUseHighPrecisionTangentBasis = false;
				SrcModel->BuildSettings.bUseFullPrecisionUVs = false;
				SrcModel->BuildSettings.bGenerateLightmapUVs = true;
				SrcModel->BuildSettings.SrcLightmapIndex = 0;
				SrcModel->BuildSettings.DstLightmapIndex = 1;
				SrcModel->SaveRawMesh(RawMesh);

				// Copy materials to new mesh
				for (UMaterialInterface* Material : MeshMaterials)
				{
					StaticMesh->StaticMaterials.Add(FStaticMaterial(Material));
				}

				//Set the Imported version before calling the build
				StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

				// Build mesh from source
				StaticMesh->Build(false);
				StaticMesh->PostEditChange();

				// Notify asset registry of new asset
				FAssetRegistryModule::AssetCreated(StaticMesh);

				return StaticMesh;
			}
		}
	}
	return nullptr;
}

UStaticMesh* UPMConvertSM::LoadStaticMeshFromPath(const FString &path)
{
	if (path == "")
		return NULL;

	UStaticMesh* tempMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, *path));
	return tempMesh;
}
