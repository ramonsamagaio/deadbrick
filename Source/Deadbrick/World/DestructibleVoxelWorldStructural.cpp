#include "World/DestructibleVoxelWorld.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "World/VoxelPhysicsIsland.h"

void ADestructibleVoxelWorld::EvaluateStructuralGravity(const FVector& WorldCenter, float RadiusCm)
{
    if (!bEnableStructuralGravity || !GetWorld() || MaxStructuralScanVoxels <= 0 || MaxPhysicsIslandVoxels <= 0) return;

    const FIntVector Center = WorldToVoxel(WorldCenter);
    const int32 RadiusV = FMath::Clamp(FMath::CeilToInt(RadiusCm / FMath::Max(1.0f, VoxelSizeCm)), 4, 96);
    const int32 CandidateRadiusSq = RadiusV * RadiusV;
    const FIntVector Directions[6] =
    {
        FIntVector(1,0,0), FIntVector(-1,0,0), FIntVector(0,1,0),
        FIntVector(0,-1,0), FIntVector(0,0,1), FIntVector(0,0,-1)
    };

    TSet<FIntVector> Visited;
    int32 CollapsedVoxels = 0;

    for (int32 Z = -RadiusV; Z <= RadiusV; ++Z)
    for (int32 Y = -RadiusV; Y <= RadiusV; ++Y)
    for (int32 X = -RadiusV; X <= RadiusV; ++X)
    {
        if (X * X + Y * Y + Z * Z > CandidateRadiusSq) continue;

        const FIntVector Start = Center + FIntVector(X, Y, Z);
        if (Visited.Contains(Start)) continue;

        FDeadbrickVoxel StartVoxel;
        if (!GetVoxel(Start, StartVoxel)) continue;
        if (StartVoxel.Material == EDeadbrickVoxelMaterial::Soil || StartVoxel.Material == EDeadbrickVoxelMaterial::Asphalt) continue;

        TArray<FIntVector> Queue;
        TArray<FIntVector> Component;
        TMap<int32, int32> LayerCounts;
        Queue.Reserve(FMath::Min(MaxStructuralScanVoxels, 8192));
        Component.Reserve(FMath::Min(MaxStructuralScanVoxels, 8192));
        Queue.Add(Start);
        Visited.Add(Start);

        int32 ReadIndex = 0;
        bool bTouchesGround = false;
        bool bHitScanLimit = false;
        int32 MinZ = Start.Z;
        int32 MaxZ = Start.Z;

        while (ReadIndex < Queue.Num())
        {
            const FIntVector Current = Queue[ReadIndex++];
            FDeadbrickVoxel CurrentVoxel;
            if (!GetVoxel(Current, CurrentVoxel)) continue;
            if (CurrentVoxel.Material == EDeadbrickVoxelMaterial::Soil || CurrentVoxel.Material == EDeadbrickVoxelMaterial::Asphalt) continue;

            Component.Add(Current);
            LayerCounts.FindOrAdd(Current.Z) += 1;
            MinZ = FMath::Min(MinZ, Current.Z);
            MaxZ = FMath::Max(MaxZ, Current.Z);

            if (Current.Z <= 0)
            {
                bTouchesGround = true;
            }
            else
            {
                FDeadbrickVoxel Below;
                if (GetVoxel(Current + FIntVector(0,0,-1), Below) &&
                    (Below.Material == EDeadbrickVoxelMaterial::Soil || Below.Material == EDeadbrickVoxelMaterial::Asphalt))
                {
                    bTouchesGround = true;
                }
            }

            if (Component.Num() >= MaxStructuralScanVoxels)
            {
                bHitScanLimit = true;
                break;
            }

            for (const FIntVector& Direction : Directions)
            {
                const FIntVector Neighbor = Current + Direction;
                if (Visited.Contains(Neighbor)) continue;

                FDeadbrickVoxel NeighborVoxel;
                if (!GetVoxel(Neighbor, NeighborVoxel)) continue;
                if (NeighborVoxel.Material == EDeadbrickVoxelMaterial::Soil || NeighborVoxel.Material == EDeadbrickVoxelMaterial::Asphalt) continue;

                Visited.Add(Neighbor);
                Queue.Add(Neighbor);
            }
        }

        if (Component.Num() == 0) continue;

        int32 FailureZ = bTouchesGround ? MAX_int32 : MinZ;

        if (bTouchesGround)
        {
            int32 VoxelsAbove = Component.Num();
            for (int32 LayerZ = MinZ; LayerZ <= MaxZ; ++LayerZ)
            {
                const int32 LayerCount = LayerCounts.FindRef(LayerZ);
                VoxelsAbove -= LayerCount;
                if (VoxelsAbove <= 0) break;

                const int32 RequiredSupport = FMath::Max(
                    1,
                    FMath::CeilToInt((float)VoxelsAbove / FMath::Max(1.0f, SupportCapacityPerGroundVoxel)));

                if (LayerCount < RequiredSupport)
                {
                    FailureZ = LayerZ;
                    break;
                }
            }
        }

        if (FailureZ == MAX_int32)
        {
            if (bHitScanLimit)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("DEADBRICK structural scan reached %d voxels on a supported component."),
                    MaxStructuralScanVoxels);
            }
            continue;
        }

        TArray<FIntVector> Detached;
        Detached.Reserve(Component.Num());
        for (const FIntVector& Cell : Component)
        {
            if (!bTouchesGround || Cell.Z >= FailureZ) Detached.Add(Cell);
        }
        if (Detached.Num() == 0) continue;

        // Build the dynamic fragments while the source cells still exist, so the fragment mesh can
        // read the original material of every voxel. Then remove the static cells from the world.
        SpawnDetachedComponentAsPhysics(Detached);

        BeginBulkEdit();
        for (const FIntVector& Cell : Detached)
        {
            SetVoxel(Cell, EDeadbrickVoxelMaterial::Air, 0);
        }
        EndBulkEdit();

        CollapsedVoxels += Detached.Num();

        UE_LOG(LogTemp, Display,
            TEXT("DEADBRICK STRUCTURAL FAILURE: %d voxels detached at Z=%d (component=%d, ground=%s, scanLimit=%s)"),
            Detached.Num(), FailureZ, Component.Num(), bTouchesGround ? TEXT("yes") : TEXT("no"), bHitScanLimit ? TEXT("yes") : TEXT("no"));
    }

    if (CollapsedVoxels > 0 && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
            FString::Printf(TEXT("STRUCTURAL COLLAPSE: %d voxels detached into physics fragments"), CollapsedVoxels));
    }
}

void ADestructibleVoxelWorld::SpawnDetachedComponentAsPhysics(const TArray<FIntVector>& Component)
{
    if (!GetWorld() || Component.Num() == 0) return;

    const int32 IslandSpan = FMath::Clamp(
        FMath::FloorToInt(FMath::Pow((float)FMath::Max(64, MaxPhysicsIslandVoxels), 1.0f / 3.0f)),
        4,
        18);

    TMap<FIntVector, TArray<FIntVector>> Buckets;
    for (const FIntVector& Cell : Component)
    {
        const FIntVector BucketKey(
            FloorDiv(Cell.X, IslandSpan),
            FloorDiv(Cell.Y, IslandSpan),
            FloorDiv(Cell.Z, IslandSpan));
        Buckets.FindOrAdd(BucketKey).Add(Cell);
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    for (TPair<FIntVector, TArray<FIntVector>>& Pair : Buckets)
    {
        TArray<FIntVector>& Cells = Pair.Value;
        if (Cells.Num() == 0) continue;

        for (int32 Offset = 0; Offset < Cells.Num(); Offset += MaxPhysicsIslandVoxels)
        {
            const int32 Count = FMath::Min(MaxPhysicsIslandVoxels, Cells.Num() - Offset);
            TArray<FIntVector> IslandCells;
            IslandCells.Append(Cells.GetData() + Offset, Count);

            if (AVoxelPhysicsIsland* Island = GetWorld()->SpawnActor<AVoxelPhysicsIsland>(
                AVoxelPhysicsIsland::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
            {
                Island->InitializeFromVoxels(this, IslandCells);
            }
        }
    }
}
