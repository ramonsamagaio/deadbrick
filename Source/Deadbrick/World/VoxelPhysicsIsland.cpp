#include "World/VoxelPhysicsIsland.h"

#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "World/DestructibleVoxelWorld.h"

namespace
{
    struct FIslandMeshBuffers
    {
        TArray<FVector> Vertices;
        TArray<int32> Triangles;
        TArray<FVector> Normals;
        TArray<FVector2D> UVs;
        TArray<FLinearColor> Colors;
        TArray<FProcMeshTangent> Tangents;
    };

    float DensityKgPerM3(EDeadbrickVoxelMaterial Material)
    {
        switch (Material)
        {
            case EDeadbrickVoxelMaterial::Wood: return 700.0f;
            case EDeadbrickVoxelMaterial::Brick: return 1800.0f;
            case EDeadbrickVoxelMaterial::Concrete: return 2400.0f;
            case EDeadbrickVoxelMaterial::Glass: return 2500.0f;
            case EDeadbrickVoxelMaterial::Metal: return 7800.0f;
            case EDeadbrickVoxelMaterial::Asphalt: return 2300.0f;
            case EDeadbrickVoxelMaterial::Soil: return 1600.0f;
            default: return 1000.0f;
        }
    }
}

AVoxelPhysicsIsland::AVoxelPhysicsIsland()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("VoxelPhysicsMesh"));
    SetRootComponent(MeshComponent);
    MeshComponent->bUseComplexAsSimpleCollision = false;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
    MeshComponent->SetLinearDamping(0.08f);
    MeshComponent->SetAngularDamping(0.25f);
}

void AVoxelPhysicsIsland::InitializeFromVoxels(ADestructibleVoxelWorld* SourceWorld, const TArray<FIntVector>& Voxels)
{
    if (!SourceWorld || Voxels.Num() == 0 || !MeshComponent) return;

    const float VoxelSize = SourceWorld->VoxelSizeCm;
    const float VoxelSizeMeters = VoxelSize / 100.0f;
    const float VoxelVolumeM3 = VoxelSizeMeters * VoxelSizeMeters * VoxelSizeMeters;
    const FVector Origin = SourceWorld->VoxelToWorld(Voxels[0]);
    SetActorLocation(Origin);

    TSet<FIntVector> CellSet;
    CellSet.Reserve(Voxels.Num());
    for (const FIntVector& Voxel : Voxels) CellSet.Add(Voxel);

    TArray<FIslandMeshBuffers> Buffers;
    Buffers.SetNum(8);
    TArray<FVector> AllVertices;
    float EstimatedMassKg = 0.0f;

    const FIntVector Directions[6] =
    {
        FIntVector(1,0,0), FIntVector(-1,0,0), FIntVector(0,1,0),
        FIntVector(0,-1,0), FIntVector(0,0,1), FIntVector(0,0,-1)
    };
    const FVector FaceNormals[6] =
    {
        FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0),
        FVector(0,-1,0), FVector(0,0,1), FVector(0,0,-1)
    };

    auto AddFace = [&](FIslandMeshBuffers& Buffer, const FVector& Center, int32 Face)
    {
        const FVector N = FaceNormals[Face];
        const FVector AxisA = FMath::Abs(N.Z) > 0.5f ? FVector(1,0,0) : FVector(0,0,1);
        const FVector AxisB = FVector::CrossProduct(N, AxisA);
        const float H = VoxelSize * 0.5f;
        const FVector FaceCenter = Center + N * H;
        const int32 Base = Buffer.Vertices.Num();

        const FVector V0 = FaceCenter + (-AxisA - AxisB) * H;
        const FVector V1 = FaceCenter + ( AxisA - AxisB) * H;
        const FVector V2 = FaceCenter + ( AxisA + AxisB) * H;
        const FVector V3 = FaceCenter + (-AxisA + AxisB) * H;
        Buffer.Vertices.Append({V0, V1, V2, V3});
        AllVertices.Append({V0, V1, V2, V3});
        Buffer.Triangles.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
        for (int32 I = 0; I < 4; ++I)
        {
            Buffer.Normals.Add(N);
            Buffer.Colors.Add(FLinearColor::White);
            Buffer.Tangents.Add(FProcMeshTangent(AxisA, false));
        }
        Buffer.UVs.Append({FVector2D(0,0), FVector2D(1,0), FVector2D(1,1), FVector2D(0,1)});
    };

    for (const FIntVector& Cell : Voxels)
    {
        FDeadbrickVoxel SourceVoxel;
        if (!SourceWorld->GetVoxel(Cell, SourceVoxel)) continue;

        const int32 MaterialIndex = (int32)SourceVoxel.Material;
        if (!Buffers.IsValidIndex(MaterialIndex) || MaterialIndex <= 0) continue;

        EstimatedMassKg += DensityKgPerM3(SourceVoxel.Material) * VoxelVolumeM3;

        FIslandMeshBuffers& Buffer = Buffers[MaterialIndex];
        const FVector Center = SourceWorld->VoxelToWorld(Cell) - Origin;
        for (int32 Face = 0; Face < 6; ++Face)
        {
            if (!CellSet.Contains(Cell + Directions[Face])) AddFace(Buffer, Center, Face);
        }
    }

    int32 SectionIndex = 0;
    for (int32 MaterialIndex = 1; MaterialIndex < Buffers.Num(); ++MaterialIndex)
    {
        FIslandMeshBuffers& Buffer = Buffers[MaterialIndex];
        if (Buffer.Vertices.Num() == 0) continue;

        MeshComponent->CreateMeshSection_LinearColor(
            SectionIndex,
            Buffer.Vertices,
            Buffer.Triangles,
            Buffer.Normals,
            Buffer.UVs,
            Buffer.Colors,
            Buffer.Tangents,
            false,
            false);

        if (UMaterialInterface* Material = SourceWorld->GetSurfaceMaterialForVoxel((EDeadbrickVoxelMaterial)MaterialIndex))
        {
            MeshComponent->SetMaterial(SectionIndex, Material);
        }
        ++SectionIndex;
    }

    if (AllVertices.Num() > 0)
    {
        FBox LocalBounds(EForceInit::ForceInit);
        for (const FVector& Vertex : AllVertices) LocalBounds += Vertex;
        const FVector Min = LocalBounds.Min;
        const FVector Max = LocalBounds.Max;
        TArray<FVector> Convex;
        Convex.Reserve(8);
        Convex.Add(FVector(Min.X, Min.Y, Min.Z));
        Convex.Add(FVector(Max.X, Min.Y, Min.Z));
        Convex.Add(FVector(Max.X, Max.Y, Min.Z));
        Convex.Add(FVector(Min.X, Max.Y, Min.Z));
        Convex.Add(FVector(Min.X, Min.Y, Max.Z));
        Convex.Add(FVector(Max.X, Min.Y, Max.Z));
        Convex.Add(FVector(Max.X, Max.Y, Max.Z));
        Convex.Add(FVector(Min.X, Max.Y, Max.Z));
        MeshComponent->AddCollisionConvexMesh(Convex);
    }

    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetEnableGravity(true);
    MeshComponent->SetMassOverrideInKg(NAME_None, FMath::Clamp(EstimatedMassKg, 1.0f, 250000.0f), true);
    MeshComponent->WakeAllRigidBodies();
}
