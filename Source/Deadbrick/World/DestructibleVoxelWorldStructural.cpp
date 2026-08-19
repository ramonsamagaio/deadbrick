#include "World/DestructibleVoxelWorld.h"

#include "Engine/World.h"
#include "ProceduralMeshComponent.h"
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

    int32 AxisValue(const FIntVector& V, int32 Axis)
    {
        return Axis == 0 ? V.X : (Axis == 1 ? V.Y : V.Z);
    }
}

void ADestructibleVoxelWorld::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bDeferRuntimeChunkRebuilds && !bAsyncChunkCookingConfigured)
    {
        for (TPair<FIntVector, TObjectPtr<UProceduralMeshComponent>>& Pair : ChunkMeshes)
        {
            if (Pair.Value) Pair.Value->bUseAsyncCooking = true;
        }
        bAsyncChunkCookingConfigured = true;
        UE_LOG(LogTemp, Display, TEXT("DEADBRICK voxel runtime: async chunk collision cooking enabled."));
    }

    ProcessStructuralQueries();
    ProcessPendingCollapses();
    FlushDirtyChunkBudget();
}

void ADestructibleVoxelWorld::QueueStructuralCheckFromDestroyed(const TArray<FIntVector>& DestroyedCells)
{
    if (!bEnableStructuralGravity || DestroyedCells.Num() == 0) return;

    // Damage never launches a full connectivity walk inside the input event. It only contributes
    // candidate neighbours to the next fresh structural pass. Rapid fire therefore coalesces into
    // one follow-up query rather than starting one building-sized BFS per bullet.
    for (const FIntVector& Destroyed : DestroyedCells)
    {
        for (const FIntVector& Direction : StructuralDirections)
        {
            const FIntVector Neighbor = Destroyed + Direction;
            if (PendingCollapseCells.Contains(Neighbor)) continue;

            FDeadbrickVoxel Voxel;
            if (!GetVoxel(Neighbor, Voxel)) continue;
            if (IsStructuralAnchorMaterial(Voxel.Material)) continue;

            if (PendingStructuralSeeds.Num() < 8192)
                PendingStructuralSeeds.Add(Neighbor);
        }
    }
}

void ADestructibleVoxelWorld::EvaluateStructuralGravity(const FVector& WorldCenter, float RadiusCm)
{
    if (!bEnableStructuralGravity) return;

    const FIntVector Center = WorldToVoxel(WorldCenter);
    const int32 RequestedRadius = FMath::CeilToInt(RadiusCm / FMath::Max(1.0f, VoxelSizeCm));
    const int32 RadiusV = FMath::Clamp(RequestedRadius, 1, 12);
    const int32 RadiusSq = RadiusV * RadiusV;

    for (int32 Z = -RadiusV; Z <= RadiusV && PendingStructuralSeeds.Num() < 8192; ++Z)
    for (int32 Y = -RadiusV; Y <= RadiusV && PendingStructuralSeeds.Num() < 8192; ++Y)
    for (int32 X = -RadiusV; X <= RadiusV && PendingStructuralSeeds.Num() < 8192; ++X)
    {
        if (X * X + Y * Y + Z * Z > RadiusSq) continue;
        const FIntVector Candidate = Center + FIntVector(X, Y, Z);
        if (PendingCollapseCells.Contains(Candidate)) continue;

        FDeadbrickVoxel Voxel;
        if (!GetVoxel(Candidate, Voxel)) continue;
        if (IsStructuralAnchorMaterial(Voxel.Material)) continue;
        PendingStructuralSeeds.Add(Candidate);
    }
}

void ADestructibleVoxelWorld::ResolveStructuralGravityNear(const FVector& WorldCenter, float RadiusCm)
{
    EvaluateStructuralGravity(WorldCenter, RadiusCm);
}

void ADestructibleVoxelWorld::StartNextStructuralQueryIfNeeded()
{
    if (StructuralQueries.Num() > 0 || PendingStructuralSeeds.Num() == 0) return;

    FStructuralQueryState Query;
    Query.Seeds = PendingStructuralSeeds.Array();
    PendingStructuralSeeds.Reset();
    StructuralQueries.Add(MoveTemp(Query));
}

void ADestructibleVoxelWorld::ProcessStructuralQueries()
{
    int32 WorkRemaining = FMath::Max(1, StructuralWorkBudgetPerFrame);
    StartNextStructuralQueryIfNeeded();

    while (WorkRemaining > 0)
    {
        if (StructuralQueries.Num() == 0)
        {
            StartNextStructuralQueryIfNeeded();
            if (StructuralQueries.Num() == 0) break;
        }

        FStructuralQueryState& Query = StructuralQueries[0];

        if (!Query.bComponentActive)
        {
            bool bStarted = false;
            while (Query.SeedIndex < Query.Seeds.Num())
            {
                const FIntVector Seed = Query.Seeds[Query.SeedIndex++];
                if (Query.Visited.Contains(Seed) || PendingCollapseCells.Contains(Seed)) continue;

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

            if (PendingCollapseCells.Contains(Current)) continue;

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
                Query.bHitScanLimit = true;
                break;
            }

            for (const FIntVector& Direction : StructuralDirections)
            {
                const FIntVector Neighbor = Current + Direction;
                if (Query.Visited.Contains(Neighbor) || PendingCollapseCells.Contains(Neighbor)) continue;

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
            UE_LOG(LogTemp, Warning,
                TEXT("DEADBRICK structural safety ceiling reached at %d voxels; component kept static. Raise MaxStructuralScanVoxels only if a real structure exceeds this."),
                Query.Component.Num());
            Query.ResetComponent();
            continue;
        }

        if (Query.bComponentActive && Query.QueueReadIndex >= Query.Queue.Num())
        {
            if (Query.Component.Num() > 0 && Query.Component.Num() <= MaxDetachedComponentVoxels)
            {
                QueueDetachedComponentForPhysics(Query.Component);
            }
            else if (Query.Component.Num() > MaxDetachedComponentVoxels)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("DEADBRICK unanchored component has %d voxels and exceeds the configured detached-component safety ceiling %d."),
                    Query.Component.Num(), MaxDetachedComponentVoxels);
            }

            Query.ResetComponent();
        }
    }
}

void ADestructibleVoxelWorld::QueueDetachedComponentForPhysics(const TArray<FIntVector>& Component)
{
    if (!GetWorld() || Component.Num() == 0) return;

    // Never queue overlapping conversions. A later fresh structural pass will re-evaluate whatever
    // remains after the current collapse has been committed to the static voxel field.
    for (const FIntVector& Cell : Component)
    {
        if (PendingCollapseCells.Contains(Cell)) return;
    }

    FIntVector Min = Component[0];
    FIntVector Max = Component[0];
    for (const FIntVector& Cell : Component)
    {
        Min.X = FMath::Min(Min.X, Cell.X); Min.Y = FMath::Min(Min.Y, Cell.Y); Min.Z = FMath::Min(Min.Z, Cell.Z);
        Max.X = FMath::Max(Max.X, Cell.X); Max.Y = FMath::Max(Max.Y, Cell.Y); Max.Z = FMath::Max(Max.Z, Cell.Z);
    }

    const FIntVector Extent = Max - Min;
    int32 SplitAxis = 0;
    if (Extent.Y > Extent.X && Extent.Y >= Extent.Z) SplitAxis = 1;
    else if (Extent.Z > Extent.X && Extent.Z > Extent.Y) SplitAxis = 2;

    const int32 TargetPerBody = FMath::Max(256, MaxPhysicsIslandVoxels);
    int32 BodyCount = FMath::CeilToInt((float)Component.Num() / (float)TargetPerBody);
    BodyCount = FMath::Clamp(BodyCount, 1, FMath::Max(1, MaxPhysicsBodiesPerCollapse));

    const int32 MinAxis = AxisValue(Min, SplitAxis);
    const int32 MaxAxis = AxisValue(Max, SplitAxis);
    const int32 AxisRange = FMath::Max(1, MaxAxis - MinAxis + 1);
    BodyCount = FMath::Min(BodyCount, AxisRange);

    FPendingCollapseState Collapse;
    Collapse.Cells = Component;
    Collapse.Groups.SetNum(BodyCount);

    for (const FIntVector& Cell : Component)
    {
        const int32 Relative = AxisValue(Cell, SplitAxis) - MinAxis;
        const int32 GroupIndex = FMath::Clamp((Relative * BodyCount) / AxisRange, 0, BodyCount - 1);
        Collapse.Groups[GroupIndex].Add(Cell);
        PendingCollapseCells.Add(Cell);
    }

    for (int32 Index = Collapse.Groups.Num() - 1; Index >= 0; --Index)
    {
        if (Collapse.Groups[Index].Num() == 0) Collapse.Groups.RemoveAt(Index);
    }

    if (Collapse.Groups.Num() == 0)
    {
        for (const FIntVector& Cell : Component) PendingCollapseCells.Remove(Cell);
        return;
    }

    UE_LOG(LogTemp, Display,
        TEXT("DEADBRICK COLLAPSE QUEUED: %d unanchored voxels -> %d macro physics bodies; preparation is frame-budgeted."),
        Component.Num(), Collapse.Groups.Num());

    PendingCollapses.Add(MoveTemp(Collapse));
}

void ADestructibleVoxelWorld::ProcessPendingCollapses()
{
    if (!GetWorld() || PendingCollapses.Num() == 0) return;

    FPendingCollapseState& Collapse = PendingCollapses[0];
    int32 BuildBudget = FMath::Max(1, PhysicsIslandBuildBudgetPerFrame);

    while (BuildBudget > 0 && Collapse.NextGroupIndex < Collapse.Groups.Num())
    {
        const TArray<FIntVector>& Group = Collapse.Groups[Collapse.NextGroupIndex++];
        --BuildBudget;
        if (Group.Num() == 0) continue;

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (AVoxelPhysicsIsland* Island = GetWorld()->SpawnActor<AVoxelPhysicsIsland>(
            AVoxelPhysicsIsland::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
        {
            // Build visual/collision data without simulation while the original static voxels still exist.
            // Once every macro body is prepared, the static source is removed and all bodies wake together.
            Island->InitializeFromVoxels(this, Group, false);
            Collapse.PreparedIslands.Add(Island);
        }
    }

    if (Collapse.NextGroupIndex < Collapse.Groups.Num()) return;

    if (Collapse.PreparedIslands.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("DEADBRICK collapse preparation failed; keeping %d voxels static."), Collapse.Cells.Num());
        for (const FIntVector& Cell : Collapse.Cells) PendingCollapseCells.Remove(Cell);
        PendingCollapses.RemoveAt(0, 1, EAllowShrinking::No);
        return;
    }

    DetachCellsFromStaticWorld(Collapse.Cells);

    int32 ActivatedBodies = 0;
    for (const TWeakObjectPtr<AVoxelPhysicsIsland>& WeakIsland : Collapse.PreparedIslands)
    {
        if (AVoxelPhysicsIsland* Island = WeakIsland.Get())
        {
            Island->ActivatePhysics();
            ++ActivatedBodies;
        }
    }

    for (const FIntVector& Cell : Collapse.Cells) PendingCollapseCells.Remove(Cell);

    UE_LOG(LogTemp, Display,
        TEXT("DEADBRICK STRUCTURAL COLLAPSE: %d voxels released as %d macro bodies."),
        Collapse.Cells.Num(), ActivatedBodies);

    PendingCollapses.RemoveAt(0, 1, EAllowShrinking::No);
}

void ADestructibleVoxelWorld::DetachCellsFromStaticWorld(const TArray<FIntVector>& Cells)
{
    TSet<FIntVector> AffectedChunks;

    for (const FIntVector& Voxel : Cells)
    {
        const FIntVector ChunkCoord = ToChunkCoord(Voxel);
        FDeadbrickVoxelChunk* Chunk = Chunks.Find(ChunkCoord);
        if (!Chunk || Chunk->Voxels.Num() == 0) continue;

        const FIntVector Local = ToLocalCoord(Voxel);
        FDeadbrickVoxel& Cell = Chunk->Voxels[ToIndex(Local)];
        if (!Cell.IsSolid()) continue;

        Cell.Material = EDeadbrickVoxelMaterial::Air;
        Cell.Integrity = 0;
        if (bRecordRuntimeEdits) RuntimeEdits.Add(Voxel, Cell);
        AffectedChunks.Add(ChunkCoord);

        if (Local.X == 0)             { const FIntVector N = ChunkCoord + FIntVector(-1,0,0); if (Chunks.Contains(N)) AffectedChunks.Add(N); }
        if (Local.X == ChunkSize - 1) { const FIntVector N = ChunkCoord + FIntVector( 1,0,0); if (Chunks.Contains(N)) AffectedChunks.Add(N); }
        if (Local.Y == 0)             { const FIntVector N = ChunkCoord + FIntVector(0,-1,0); if (Chunks.Contains(N)) AffectedChunks.Add(N); }
        if (Local.Y == ChunkSize - 1) { const FIntVector N = ChunkCoord + FIntVector(0, 1,0); if (Chunks.Contains(N)) AffectedChunks.Add(N); }
        if (Local.Z == 0)             { const FIntVector N = ChunkCoord + FIntVector(0,0,-1); if (Chunks.Contains(N)) AffectedChunks.Add(N); }
        if (Local.Z == ChunkSize - 1) { const FIntVector N = ChunkCoord + FIntVector(0,0, 1); if (Chunks.Contains(N)) AffectedChunks.Add(N); }
    }

    // Do not rebuild the entire detached structure synchronously. The normal runtime chunk budget
    // will retire stale static geometry over the following frames while the prepared macro bodies fall.
    for (const FIntVector& ChunkCoord : AffectedChunks) DirtyChunks.Add(ChunkCoord);
}

void ADestructibleVoxelWorld::SpawnDetachedComponentAsPhysics(const TArray<FIntVector>& Component)
{
    QueueDetachedComponentForPhysics(Component);
}
