#include "World/DestructibleVoxelWorld.h"

#include "Engine/World.h"
#include "World/VoxelPhysicsIsland.h"

namespace
{
    const FIntVector GStructuralDirections[6] =
    {
        FIntVector(1, 0, 0),
        FIntVector(-1, 0, 0),
        FIntVector(0, 1, 0),
        FIntVector(0, -1, 0),
        FIntVector(0, 0, 1),
        FIntVector(0, 0, -1)
    };

    bool IsAnchorMaterial(EDeadbrickVoxelMaterial Material)
    {
        return Material == EDeadbrickVoxelMaterial::Soil || Material == EDeadbrickVoxelMaterial::Asphalt;
    }

    bool IsStructuralMaterial(EDeadbrickVoxelMaterial Material)
    {
        return Material != EDeadbrickVoxelMaterial::Air && !IsAnchorMaterial(Material);
    }
}

void ADestructibleVoxelWorld::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    FlushDirtyChunkBudget();

    if (bEnableStructuralGravity)
    {
        StartNextStructuralQueryIfNeeded();
        ProcessStructuralQueries();
        ProcessPendingCollapses();
    }
}

void ADestructibleVoxelWorld::EvaluateStructuralGravity(const FVector& WorldCenter, float RadiusCm)
{
    if (!bEnableStructuralGravity) return;
    ResolveStructuralGravityNear(WorldCenter, RadiusCm);
}

void ADestructibleVoxelWorld::ResolveStructuralGravityNear(const FVector& WorldCenter, float RadiusCm)
{
    const FIntVector Center = WorldToVoxel(WorldCenter);
    const int32 RadiusVoxels = FMath::Clamp(
        FMath::CeilToInt(FMath::Max(VoxelSizeCm, RadiusCm) / FMath::Max(1.0f, VoxelSizeCm)),
        1,
        96);

    const int32 RadiusSquared = RadiusVoxels * RadiusVoxels;
    for (int32 Z = -RadiusVoxels; Z <= RadiusVoxels; ++Z)
    for (int32 Y = -RadiusVoxels; Y <= RadiusVoxels; ++Y)
    for (int32 X = -RadiusVoxels; X <= RadiusVoxels; ++X)
    {
        if (X * X + Y * Y + Z * Z > RadiusSquared) continue;

        const FIntVector Coord = Center + FIntVector(X, Y, Z);
        if (PendingCollapseCells.Contains(Coord)) continue;

        FDeadbrickVoxel Cell;
        if (GetVoxel(Coord, Cell) && IsStructuralMaterial(Cell.Material))
            PendingStructuralSeeds.Add(Coord);
    }
}

void ADestructibleVoxelWorld::QueueStructuralCheckFromDestroyed(const TArray<FIntVector>& DestroyedCells)
{
    for (const FIntVector& Destroyed : DestroyedCells)
    {
        for (const FIntVector& Direction : GStructuralDirections)
        {
            const FIntVector Candidate = Destroyed + Direction;
            if (PendingCollapseCells.Contains(Candidate)) continue;

            FDeadbrickVoxel Cell;
            if (GetVoxel(Candidate, Cell) && IsStructuralMaterial(Cell.Material))
                PendingStructuralSeeds.Add(Candidate);
        }
    }
}

void ADestructibleVoxelWorld::StartNextStructuralQueryIfNeeded()
{
    if (StructuralQueries.Num() > 0 || PendingStructuralSeeds.Num() == 0) return;

    FStructuralQueryState Query;
    Query.Seeds = PendingStructuralSeeds.Array();
    PendingStructuralSeeds.Reset();
    Query.Seeds.Sort([](const FIntVector& A, const FIntVector& B)
    {
        if (A.Z != B.Z) return A.Z < B.Z;
        if (A.Y != B.Y) return A.Y < B.Y;
        return A.X < B.X;
    });
    StructuralQueries.Add(MoveTemp(Query));
}

void ADestructibleVoxelWorld::ProcessStructuralQueries()
{
    if (StructuralQueries.Num() == 0) return;

    int32 WorkRemaining = FMath::Max(256, StructuralWorkBudgetPerFrame);

    while (WorkRemaining > 0 && StructuralQueries.Num() > 0)
    {
        FStructuralQueryState& Query = StructuralQueries[0];

        if (!Query.bComponentActive)
        {
            bool bFoundSeed = false;
            while (Query.SeedIndex < Query.Seeds.Num())
            {
                const FIntVector Seed = Query.Seeds[Query.SeedIndex++];
                if (Query.Visited.Contains(Seed) || PendingCollapseCells.Contains(Seed)) continue;

                FDeadbrickVoxel SeedCell;
                if (!GetVoxel(Seed, SeedCell) || !IsStructuralMaterial(SeedCell.Material)) continue;

                Query.ResetComponent();
                Query.bComponentActive = true;
                Query.Queue.Add(Seed);
                Query.Visited.Add(Seed);
                bFoundSeed = true;
                break;
            }

            if (!bFoundSeed)
            {
                StructuralQueries.RemoveAt(0);
                StartNextStructuralQueryIfNeeded();
                continue;
            }
        }

        while (WorkRemaining > 0 && Query.bComponentActive && Query.QueueReadIndex < Query.Queue.Num())
        {
            const FIntVector Coord = Query.Queue[Query.QueueReadIndex++];
            --WorkRemaining;

            if (PendingCollapseCells.Contains(Coord)) continue;

            FDeadbrickVoxel Cell;
            if (!GetVoxel(Coord, Cell) || !IsStructuralMaterial(Cell.Material)) continue;

            Query.Component.Add(Coord);
            if (Query.Component.Num() >= FMath::Min(MaxStructuralScanVoxels, MaxDetachedComponentVoxels))
            {
                Query.bHitScanLimit = true;
                Query.QueueReadIndex = Query.Queue.Num();
                break;
            }

            const FIntVector Below = Coord + FIntVector(0, 0, -1);
            FDeadbrickVoxel BelowCell;
            const bool bBelowSolid = GetVoxel(Below, BelowCell);
            const bool bTouchesAnchor = Coord.Z <= 0 || (bBelowSolid && IsAnchorMaterial(BelowCell.Material));

            if (bTouchesAnchor)
            {
                // A foundation slab is not hundreds of independent columns. Count only anchor contacts
                // that continue vertically through the slab into an actual wall/pier/column. This is
                // what makes a mostly destroyed first floor lose load capacity before the last pixel of
                // contact is gone.
                FDeadbrickVoxel AboveOne;
                FDeadbrickVoxel AboveTwo;
                const bool bVerticalOne = GetVoxel(Coord + FIntVector(0, 0, 1), AboveOne) && IsStructuralMaterial(AboveOne.Material);
                const bool bVerticalTwo = GetVoxel(Coord + FIntVector(0, 0, 2), AboveTwo) && IsStructuralMaterial(AboveTwo.Material);
                if (bVerticalOne && bVerticalTwo)
                {
                    ++Query.GroundSupportCount;
                    Query.bGrounded = true;
                }
            }

            for (const FIntVector& Direction : GStructuralDirections)
            {
                const FIntVector Neighbor = Coord + Direction;
                if (Query.Visited.Contains(Neighbor) || PendingCollapseCells.Contains(Neighbor)) continue;

                FDeadbrickVoxel NeighborCell;
                if (!GetVoxel(Neighbor, NeighborCell) || !IsStructuralMaterial(NeighborCell.Material)) continue;

                Query.Visited.Add(Neighbor);
                Query.Queue.Add(Neighbor);
            }
        }

        if (Query.bComponentActive && Query.QueueReadIndex >= Query.Queue.Num())
        {
            const int32 ComponentSize = Query.Component.Num();
            const float Capacity = Query.GroundSupportCount * FMath::Max(1.0f, SupportCapacityPerGroundVoxel);
            const bool bInsufficientSupport = Query.GroundSupportCount <= 0 || (float)ComponentSize > Capacity;

            if (!Query.bHitScanLimit && ComponentSize > 0 && bInsufficientSupport)
            {
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("DEADBRICK STRUCTURAL COLLAPSE queued | voxels=%d | verticalSupports=%d | capacity=%.0f | gravity-driven"),
                    ComponentSize,
                    Query.GroundSupportCount,
                    Capacity);
                QueueDetachedComponentForPhysics(Query.Component);
            }

            Query.ResetComponent();
        }
    }
}

void ADestructibleVoxelWorld::QueueDetachedComponentForPhysics(const TArray<FIntVector>& Component)
{
    if (Component.Num() == 0 || !GetWorld()) return;

    TArray<FIntVector> UniqueCells;
    UniqueCells.Reserve(Component.Num());
    TSet<FIntVector> Remaining;
    Remaining.Reserve(Component.Num());

    for (const FIntVector& Cell : Component)
    {
        if (PendingCollapseCells.Contains(Cell)) continue;

        FDeadbrickVoxel Existing;
        if (!GetVoxel(Cell, Existing) || !IsStructuralMaterial(Existing.Material)) continue;

        PendingCollapseCells.Add(Cell);
        Remaining.Add(Cell);
        UniqueCells.Add(Cell);
    }

    if (UniqueCells.Num() == 0) return;

    const int32 TargetGroupSize = FMath::Clamp(MaxPhysicsIslandVoxels, 8, 128);
    TArray<TArray<FIntVector>> AllGroups;
    AllGroups.Reserve(FMath::CeilToInt((float)UniqueCells.Num() / (float)TargetGroupSize));

    while (Remaining.Num() > 0)
    {
        TSet<FIntVector>::TConstIterator SeedIt = Remaining.CreateConstIterator();
        if (!SeedIt) break;
        const FIntVector Seed = *SeedIt;

        TArray<FIntVector> Queue;
        TSet<FIntVector> Queued;
        TArray<FIntVector> Group;
        Queue.Add(Seed);
        Queued.Add(Seed);
        int32 ReadIndex = 0;

        while (ReadIndex < Queue.Num() && Group.Num() < TargetGroupSize)
        {
            const FIntVector Cell = Queue[ReadIndex++];
            if (!Remaining.Contains(Cell)) continue;

            Remaining.Remove(Cell);
            Group.Add(Cell);

            for (const FIntVector& Direction : GStructuralDirections)
            {
                const FIntVector Neighbor = Cell + Direction;
                if (Remaining.Contains(Neighbor) && !Queued.Contains(Neighbor))
                {
                    Queued.Add(Neighbor);
                    Queue.Add(Neighbor);
                }
            }
        }

        if (Group.Num() > 0)
            AllGroups.Add(MoveTemp(Group));
    }

    FPendingCollapseState Collapse;
    Collapse.Cells = MoveTemp(UniqueCells);

    const int32 BodyCeiling = FMath::Clamp(MaxPhysicsBodiesPerCollapse, 1, 384);
    if (AllGroups.Num() <= BodyCeiling)
    {
        Collapse.Groups = MoveTemp(AllGroups);
    }
    else
    {
        Collapse.Groups.Reserve(BodyCeiling);
        const double Step = (double)AllGroups.Num() / (double)BodyCeiling;
        for (int32 BodyIndex = 0; BodyIndex < BodyCeiling; ++BodyIndex)
        {
            const int32 SourceIndex = FMath::Clamp(FMath::FloorToInt(BodyIndex * Step), 0, AllGroups.Num() - 1);
            Collapse.Groups.Add(MoveTemp(AllGroups[SourceIndex]));
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("DEADBRICK collapse body budget: %d rubble groups sampled from %d source groups; giant merged chunks were intentionally avoided."),
            Collapse.Groups.Num(),
            AllGroups.Num());
    }

    PendingCollapses.Add(MoveTemp(Collapse));
}

void ADestructibleVoxelWorld::ProcessPendingCollapses()
{
    if (PendingCollapses.Num() == 0 || !GetWorld()) return;

    const int32 Budget = FMath::Max(1, PhysicsIslandBuildBudgetPerFrame);
    int32 WorkLeft = Budget;

    for (int32 CollapseIndex = 0; CollapseIndex < PendingCollapses.Num() && WorkLeft > 0; )
    {
        FPendingCollapseState& Collapse = PendingCollapses[CollapseIndex];

        while (WorkLeft > 0 && Collapse.NextGroupIndex < Collapse.Groups.Num())
        {
            const TArray<FIntVector>& Group = Collapse.Groups[Collapse.NextGroupIndex++];
            if (Group.Num() == 0) continue;

            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AVoxelPhysicsIsland* Island = GetWorld()->SpawnActor<AVoxelPhysicsIsland>(
                AVoxelPhysicsIsland::StaticClass(),
                VoxelToWorld(Group[0]),
                FRotator::ZeroRotator,
                SpawnParams);

            if (Island)
            {
                Island->InitializeFromVoxels(this, Group, false);
                Collapse.PreparedIslands.Add(Island);
            }
            --WorkLeft;
        }

        if (Collapse.NextGroupIndex >= Collapse.Groups.Num() && !Collapse.bDetachedFromStatic)
        {
            DetachCellsFromStaticWorld(Collapse.Cells);
            Collapse.bDetachedFromStatic = true;
        }

        while (WorkLeft > 0 && Collapse.bDetachedFromStatic && Collapse.NextActivateIndex < Collapse.PreparedIslands.Num())
        {
            if (AVoxelPhysicsIsland* Island = Collapse.PreparedIslands[Collapse.NextActivateIndex].Get())
                Island->ActivatePhysics();
            ++Collapse.NextActivateIndex;
            --WorkLeft;
        }

        if (Collapse.bDetachedFromStatic && Collapse.NextActivateIndex >= Collapse.PreparedIslands.Num())
        {
            for (const FIntVector& Cell : Collapse.Cells)
                PendingCollapseCells.Remove(Cell);

            PendingCollapses.RemoveAt(CollapseIndex);
            continue;
        }

        ++CollapseIndex;
    }
}

void ADestructibleVoxelWorld::DetachCellsFromStaticWorld(const TArray<FIntVector>& Cells)
{
    if (Cells.Num() == 0) return;

    BeginBulkEdit();
    for (const FIntVector& Cell : Cells)
    {
        FDeadbrickVoxel Existing;
        if (GetVoxel(Cell, Existing) && IsStructuralMaterial(Existing.Material))
            SetVoxel(Cell, EDeadbrickVoxelMaterial::Air, 0);
    }
    EndBulkEdit();
}

void ADestructibleVoxelWorld::SpawnDetachedComponentAsPhysics(const TArray<FIntVector>& Component)
{
    if (!GetWorld() || Component.Num() == 0) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AVoxelPhysicsIsland* Island = GetWorld()->SpawnActor<AVoxelPhysicsIsland>(
        AVoxelPhysicsIsland::StaticClass(),
        VoxelToWorld(Component[0]),
        FRotator::ZeroRotator,
        SpawnParams);

    if (Island)
        Island->InitializeFromVoxels(this, Component, true);
}
