#include "World/DestructibleVoxelWorld.h"

#include "Engine/World.h"
#include "World/VoxelPhysicsIsland.h"

namespace
{
    const FIntVector StructuralDirections[6] =
    {
        FIntVector(1,0,0), FIntVector(-1,0,0), FIntVector(0,1,0),
        FIntVector(0,-1,0), FIntVector(0,0,1), FIntVector(0,0,-1)
    };

    bool IsStructuralAnchorMaterial(EDeadbrickVoxelMaterial Material)
    {
        return Material == EDeadbrickVoxelMaterial::Soil || Material == EDeadbrickVoxelMaterial::Asphalt;
    }
}

void ADestructibleVoxelWorld::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Keep expensive topology and mesh work out of the input callback. The player can fire without
    // a full connected-component traversal and multiple chunk cooks occurring in that same frame.
    ProcessStructuralQueries();
    FlushDirtyChunkBudget();
}

void ADestructibleVoxelWorld::QueueStructuralCheckFromDestroyed(const TArray<FIntVector>& DestroyedCells)
{
    if (!bEnableStructuralGravity || DestroyedCells.Num() == 0) return;

    TSet<FIntVector> UniqueSeeds;
    for (const FIntVector& Destroyed : DestroyedCells)
    {
        for (const FIntVector& Direction : StructuralDirections)
        {
            const FIntVector Neighbor = Destroyed + Direction;
            FDeadbrickVoxel Voxel;
            if (!GetVoxel(Neighbor, Voxel)) continue;
            if (IsStructuralAnchorMaterial(Voxel.Material)) continue;
            UniqueSeeds.Add(Neighbor);
        }
    }

    if (UniqueSeeds.Num() == 0) return;

    FStructuralQueryState Query;
    Query.Seeds = UniqueSeeds.Array();
    StructuralQueries.Add(MoveTemp(Query));
}

void ADestructibleVoxelWorld::EvaluateStructuralGravity(const FVector& WorldCenter, float RadiusCm)
{
    if (!bEnableStructuralGravity) return;

    // Compatibility path for explosions/tools. It only gathers nearby starting points; connectivity
    // itself is processed later under the per-frame budget. Firearms no longer invoke a second scan.
    const FIntVector Center = WorldToVoxel(WorldCenter);
    const int32 RequestedRadius = FMath::CeilToInt(RadiusCm / FMath::Max(1.0f, VoxelSizeCm));
    const int32 RadiusV = FMath::Clamp(RequestedRadius, 1, 8);
    const int32 RadiusSq = RadiusV * RadiusV;

    TSet<FIntVector> UniqueSeeds;
    for (int32 Z = -RadiusV; Z <= RadiusV && UniqueSeeds.Num() < 512; ++Z)
    for (int32 Y = -RadiusV; Y <= RadiusV && UniqueSeeds.Num() < 512; ++Y)
    for (int32 X = -RadiusV; X <= RadiusV && UniqueSeeds.Num() < 512; ++X)
    {
        if (X * X + Y * Y + Z * Z > RadiusSq) continue;
        const FIntVector Candidate = Center + FIntVector(X, Y, Z);
        FDeadbrickVoxel Voxel;
        if (!GetVoxel(Candidate, Voxel)) continue;
        if (IsStructuralAnchorMaterial(Voxel.Material)) continue;
        UniqueSeeds.Add(Candidate);
    }

    if (UniqueSeeds.Num() == 0) return;

    FStructuralQueryState Query;
    Query.Seeds = UniqueSeeds.Array();
    StructuralQueries.Add(MoveTemp(Query));
}

void ADestructibleVoxelWorld::ResolveStructuralGravityNear(const FVector& WorldCenter, float RadiusCm)
{
    EvaluateStructuralGravity(WorldCenter, RadiusCm);
}

void ADestructibleVoxelWorld::ProcessStructuralQueries()
{
    int32 WorkRemaining = FMath::Max(1, StructuralWorkBudgetPerFrame);

    while (WorkRemaining > 0 && StructuralQueries.Num() > 0)
    {
        FStructuralQueryState& Query = StructuralQueries[0];

        if (!Query.bComponentActive)
        {
            bool bStarted = false;
            while (Query.SeedIndex < Query.Seeds.Num())
            {
                const FIntVector Seed = Query.Seeds[Query.SeedIndex++];
                if (Query.Visited.Contains(Seed)) continue;

                FDeadbrickVoxel SeedVoxel;
                if (!GetVoxel(Seed, SeedVoxel)) continue;
                if (IsStructuralAnchorMaterial(SeedVoxel.Material)) continue;

                Query.ResetComponent();
                Query.bComponentActive = true;
                Query.Queue.Add(Seed);
                Query.Visited.Add(Seed);
                bStarted = true;
                break;
            }

            if (!bStarted)
            {
                StructuralQueries.RemoveAt(0, 1, EAllowShrinking::No);
                continue;
            }
        }

        while (Query.bComponentActive &&
               Query.QueueReadIndex < Query.Queue.Num() &&
               WorkRemaining > 0 &&
               !Query.bGrounded &&
               !Query.bHitScanLimit)
        {
            const FIntVector Current = Query.Queue[Query.QueueReadIndex++];
            --WorkRemaining;

            FDeadbrickVoxel CurrentVoxel;
            if (!GetVoxel(Current, CurrentVoxel)) continue;

            if (Current.Z <= 0 || IsStructuralAnchorMaterial(CurrentVoxel.Material))
            {
                Query.bGrounded = true;
                break;
            }

            Query.Component.Add(Current);
            if (Query.Component.Num() >= MaxStructuralScanVoxels)
            {
                // A very large component is deliberately treated as anchored/unsolved in this pass.
                // Never turn an entire tower into tens of thousands of rigid bodies from one bullet.
                Query.bHitScanLimit = true;
                break;
            }

            for (const FIntVector& Direction : StructuralDirections)
            {
                const FIntVector Neighbor = Current + Direction;
                if (Query.Visited.Contains(Neighbor)) continue;

                FDeadbrickVoxel NeighborVoxel;
                if (!GetVoxel(Neighbor, NeighborVoxel)) continue;

                Query.Visited.Add(Neighbor);
                if (Neighbor.Z <= 0 || IsStructuralAnchorMaterial(NeighborVoxel.Material))
                {
                    Query.bGrounded = true;
                    break;
                }

                Query.Queue.Add(Neighbor);
            }
        }

        if (Query.bGrounded)
        {
            Query.ResetComponent();
            continue;
        }

        if (Query.bHitScanLimit)
        {
            UE_LOG(LogTemp, VeryVerbose,
                TEXT("DEADBRICK structural query kept large component static after %d visited voxels."),
                Query.Component.Num());
            Query.ResetComponent();
            continue;
        }

        if (Query.bComponentActive && Query.QueueReadIndex >= Query.Queue.Num())
        {
            if (Query.Component.Num() > 0 && Query.Component.Num() <= MaxDetachedComponentVoxels)
            {
                // Build the rigid fragment while source material cells still exist, then remove the
                // anchored representation. This is the anchored -> unanchored transition.
                const TArray<FIntVector> Detached = Query.Component;
                SpawnDetachedComponentAsPhysics(Detached);

                BeginBulkEdit();
                for (const FIntVector& Cell : Detached)
                    SetVoxel(Cell, EDeadbrickVoxelMaterial::Air, 0);
                EndBulkEdit();

                UE_LOG(LogTemp, Display,
                    TEXT("DEADBRICK UNANCHORED: %d voxel component converted to physics."),
                    Detached.Num());
            }
            else if (Query.Component.Num() > MaxDetachedComponentVoxels)
            {
                UE_LOG(LogTemp, VeryVerbose,
                    TEXT("DEADBRICK unanchored component has %d voxels; kept static to stay within physics budget."),
                    Query.Component.Num());
            }

            Query.ResetComponent();
        }
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
