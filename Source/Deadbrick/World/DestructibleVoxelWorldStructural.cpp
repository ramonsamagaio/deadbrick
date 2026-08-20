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
                TEXT("DEADBRICK structural safety ceiling reached at %d voxels; component kept static."),
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
                    TEXT("DEADBRICK unanchored component has %d voxels and exceeds detached-component ceiling %d."),
                    Query.Component.Num(), MaxDetachedComponentVoxels);
            }

            Query.ResetComponent();
        }
    }
}

void ADestructibleVoxelWorld::QueueDetachedComponentForPhysics(const TArray<FIntVector>& Component)
{
    if (!GetWorld() || Component.Num() == 0) return;

    for (const FIntVector& Cell : Component)
    {
        if (PendingCollapseCells.Contains(Cell)) return;
    }

    const int32 BodyCap = FMath::Max(1, MaxPhysicsBodiesPerCollapse);
    const int32 PreferredCellsPerBody = FMath::Clamp(MaxPhysicsIslandVoxels, 8, 512);
    const int32 MinimumNeededPerBody = FMath::CeilToInt((float)Component.Num() / (float)BodyCap);
    const int32 EffectiveCellsPerBody = FMath::Max(PreferredCellsPerBody, MinimumNeededPerBody);

    // Flood-fill partitions preserve locality. The previous longest-axis slicing could turn an entire
    // floor or facade into one enormous rectangular rigid body, which looked like a moving building
    // slab and made interaction feel fake. These groups stay contiguous and rubble-sized.
    TSet<FIntVector> Unassigned;
    Unassigned.Reserve(Component.Num());
    for (const FIntVector& Cell : Component) Unassigned.Add(Cell);

    FPendingCollapseState Collapse;
    Collapse.Cells = Component;

    while (Unassigned.Num() > 0 && Collapse.Groups.Num() < BodyCap)
    {
        FIntVector Seed = FIntVector::ZeroValue;
        bool bFoundSeed = false;
        for (const FIntVector& Candidate : Unassigned)
        {
            Seed = Candidate;
            bFoundSeed = true;
            break;
        }
        if (!bFoundSeed) break;

        TArray<FIntVector> Group;
        Group.Reserve(EffectiveCellsPerBody);
        TArray<FIntVector> Queue;
        Queue.Reserve(EffectiveCellsPerBody * 2);
        TSet<FIntVector> Queued;
        Queue.Add(Seed);
        Queued.Add(Seed);
        int32 QueueReadIndex = 0;

        while (QueueReadIndex < Queue.Num() && Group.Num() < EffectiveCellsPerBody)
        {
            const FIntVector Current = Queue[QueueReadIndex++];
            if (!Unassigned.Remove(Current)) continue;

            Group.Add(Current);
            for (const FIntVector& Direction : StructuralDirections)
            {
                const FIntVector Neighbor = Current + Direction;
                if (Unassigned.Contains(Neighbor) && !Queued.Contains(Neighbor))
                {
                    Queued.Add(Neighbor);
                    Queue.Add(Neighbor);
                }
            }
        }

        if (Group.Num() > 0)
            Collapse.Groups.Add(MoveTemp(Group));
    }

    // Connected structural components should be exhausted by the capacity calculation above. Keep a
    // defensive merge for pathological data rather than dropping dynamic cells.
    if (Unassigned.Num() > 0)
    {
        if (Collapse.Groups.Num() == 0) Collapse.Groups.AddDefaulted();
        TArray<FIntVector>& LastGroup = Collapse.Groups.Last();
        for (const FIntVector& Cell : Unassigned) LastGroup.Add(Cell);
        Unassigned.Reset();
    }

    if (Collapse.Groups.Num() == 0) return;

    for (const FIntVector& Cell : Component)
        PendingCollapseCells.Add(Cell);

    UE_LOG(LogTemp, Display,
        TEXT("DEADBRICK COLLAPSE QUEUED: %d unanchored voxels -> %d contiguous rubble bodies, target=%d cells/body."),
        Component.Num(), Collapse.Groups.Num(), EffectiveCellsPerBody);

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
        TEXT("DEADBRICK STRUCTURAL COLLAPSE: %d voxels released as %d interactive rubble bodies."),
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

    for (const FIntVector& ChunkCoord : AffectedChunks) DirtyChunks.Add(ChunkCoord);
}

void ADestructibleVoxelWorld::SpawnDetachedComponentAsPhysics(const TArray<FIntVector>& Component)
{
    QueueDetachedComponentForPhysics(Component);
}
