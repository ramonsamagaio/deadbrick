#include "World/VoxelPhysicsIsland.h"

#include "ProceduralMeshComponent.h"
#include "World/DestructibleVoxelWorld.h"

AVoxelPhysicsIsland::AVoxelPhysicsIsland()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("VoxelPhysicsMesh"));
    SetRootComponent(MeshComponent);
    MeshComponent->bUseComplexAsSimpleCollision = false;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
}

void AVoxelPhysicsIsland::InitializeFromVoxels(ADestructibleVoxelWorld* SourceWorld, const TArray<FIntVector>& Voxels)
{
    if (!SourceWorld || Voxels.Num() == 0 || !MeshComponent) return;

    const float VoxelSize = SourceWorld->VoxelSizeCm;
    const FVector Origin = SourceWorld->VoxelToWorld(Voxels[0]);
    SetActorLocation(Origin);

    TSet<FIntVector> CellSet;
    CellSet.Reserve(Voxels.Num());
    for (const FIntVector& Voxel : Voxels) CellSet.Add(Voxel);

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    const FIntVector Directions[6] = {
        FIntVector(1,0,0), FIntVector(-1,0,0), FIntVector(0,1,0),
        FIntVector(0,-1,0), FIntVector(0,0,1), FIntVector(0,0,-1)
    };
    const FVector FaceNormals[6] = {
        FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0),
        FVector(0,-1,0), FVector(0,0,1), FVector(0,0,-1)
    };

    auto AddFace = [&](const FVector& Center, int32 Face)
    {
        const FVector N = FaceNormals[Face];
        const FVector AxisA = FMath::Abs(N.Z) > 0.5f ? FVector(1,0,0) : FVector(0,0,1);
        const FVector AxisB = FVector::CrossProduct(N, AxisA);
        const float H = VoxelSize * 0.5f;
        const FVector FaceCenter = Center + N * H;
        const int32 Base = Vertices.Num();

        Vertices.Add(FaceCenter + (-AxisA - AxisB) * H);
        Vertices.Add(FaceCenter + ( AxisA - AxisB) * H);
        Vertices.Add(FaceCenter + ( AxisA + AxisB) * H);
        Vertices.Add(FaceCenter + (-AxisA + AxisB) * H);

        Triangles.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
        for (int32 I = 0; I < 4; ++I)
        {
            Normals.Add(N);
            Colors.Add(FLinearColor::White);
            Tangents.Add(FProcMeshTangent(AxisA, false));
        }
        UVs.Append({FVector2D(0,0), FVector2D(1,0), FVector2D(1,1), FVector2D(0,1)});
    };

    for (const FIntVector& Cell : Voxels)
    {
        const FVector Center = SourceWorld->VoxelToWorld(Cell) - Origin;
        for (int32 Face = 0; Face < 6; ++Face)
        {
            if (!CellSet.Contains(Cell + Directions[Face])) AddFace(Center, Face);
        }
    }

    MeshComponent->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false, false);

    // Chaos cannot simulate a movable body using a complex triangle mesh as its only collision shape.
    // A bounded convex hull keeps detached voxel groups physical without turning every microvoxel into a rigid body.
    if (Vertices.Num() > 0)
    {
        FBox LocalBounds(EForceInit::ForceInit);
        for (const FVector& Vertex : Vertices) LocalBounds += Vertex;
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
    MeshComponent->WakeAllRigidBodies();
}
