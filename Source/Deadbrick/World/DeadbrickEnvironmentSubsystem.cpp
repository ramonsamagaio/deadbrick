#include "World/DeadbrickEnvironmentSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "World/DestructibleVoxelWorld.h"

bool UDeadbrickEnvironmentSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

void UDeadbrickEnvironmentSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    InWorld.GetTimerManager().SetTimer(SimulationTimer, this, &UDeadbrickEnvironmentSubsystem::StepSimulation, 0.20f, true, 0.20f);
    UE_LOG(LogTemp, Display, TEXT("DEADBRICK environment simulation active: water/gas/fuel/fire/smoke sparse cells."));
}

ADestructibleVoxelWorld* UDeadbrickEnvironmentSubsystem::FindVoxelWorld() const
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;
    for (TActorIterator<ADestructibleVoxelWorld> It(World); It; ++It) return *It;
    return nullptr;
}

bool UDeadbrickEnvironmentSubsystem::IsOpenCell(ADestructibleVoxelWorld* VoxelWorld, const FIntVector& Coord) const
{
    if (!VoxelWorld) return false;
    FDeadbrickVoxel Voxel;
    return !VoxelWorld->GetVoxel(Coord, Voxel);
}

void UDeadbrickEnvironmentSubsystem::AddWaterAtWorld(const FVector& WorldLocation, float Amount)
{
    if (ADestructibleVoxelWorld* VoxelWorld = FindVoxelWorld())
    {
        Cells.FindOrAdd(VoxelWorld->WorldToVoxel(WorldLocation)).Water += FMath::Max(0.0f, Amount);
    }
}

void UDeadbrickEnvironmentSubsystem::AddGasAtWorld(const FVector& WorldLocation, float Amount)
{
    if (ADestructibleVoxelWorld* VoxelWorld = FindVoxelWorld())
    {
        Cells.FindOrAdd(VoxelWorld->WorldToVoxel(WorldLocation)).Gas += FMath::Max(0.0f, Amount);
    }
}

void UDeadbrickEnvironmentSubsystem::AddFuelAtWorld(const FVector& WorldLocation, float Amount)
{
    if (ADestructibleVoxelWorld* VoxelWorld = FindVoxelWorld())
    {
        Cells.FindOrAdd(VoxelWorld->WorldToVoxel(WorldLocation)).Fuel += FMath::Max(0.0f, Amount);
    }
}

void UDeadbrickEnvironmentSubsystem::IgniteAtWorld(const FVector& WorldLocation, float Intensity)
{
    if (ADestructibleVoxelWorld* VoxelWorld = FindVoxelWorld())
    {
        FDeadbrickEnvironmentCell& Cell = Cells.FindOrAdd(VoxelWorld->WorldToVoxel(WorldLocation));
        Cell.Fire = FMath::Max(Cell.Fire, FMath::Max(0.0f, Intensity));
        Cell.Temperature = FMath::Max(Cell.Temperature, 220.0f);
    }
}

FDeadbrickEnvironmentCell UDeadbrickEnvironmentSubsystem::GetCellAtWorld(const FVector& WorldLocation) const
{
    if (ADestructibleVoxelWorld* VoxelWorld = FindVoxelWorld())
    {
        if (const FDeadbrickEnvironmentCell* Cell = Cells.Find(VoxelWorld->WorldToVoxel(WorldLocation))) return *Cell;
    }
    return FDeadbrickEnvironmentCell();
}

void UDeadbrickEnvironmentSubsystem::StepSimulation()
{
    ADestructibleVoxelWorld* VoxelWorld = FindVoxelWorld();
    if (!VoxelWorld || Cells.Num() == 0) return;

    const TMap<FIntVector, FDeadbrickEnvironmentCell> Previous = Cells;
    TMap<FIntVector, FDeadbrickEnvironmentCell> Next = Cells;
    TArray<FIntVector> Keys;
    Previous.GetKeys(Keys);

    const FIntVector Laterals[4] = {
        FIntVector(1,0,0), FIntVector(-1,0,0), FIntVector(0,1,0), FIntVector(0,-1,0)
    };
    const FIntVector Neighbors[6] = {
        FIntVector(1,0,0), FIntVector(-1,0,0), FIntVector(0,1,0), FIntVector(0,-1,0), FIntVector(0,0,1), FIntVector(0,0,-1)
    };

    VoxelWorld->BeginBulkEdit();
    int32 Processed = 0;
    for (const FIntVector& Coord : Keys)
    {
        if (++Processed > MaxCellsPerStep) break;
        const FDeadbrickEnvironmentCell* SourcePtr = Previous.Find(Coord);
        if (!SourcePtr) continue;
        const FDeadbrickEnvironmentCell Source = *SourcePtr;
        FDeadbrickEnvironmentCell& Current = Next.FindOrAdd(Coord);

        // Water obeys gravity first. Once supported, it fans sideways into open cells.
        if (Source.Water > 0.01f)
        {
            const FIntVector Below = Coord + FIntVector(0,0,-1);
            if (IsOpenCell(VoxelWorld, Below))
            {
                const float Transfer = FMath::Min(Current.Water, Source.Water * 0.55f);
                Current.Water -= Transfer;
                Next.FindOrAdd(Below).Water += Transfer;
            }
            else
            {
                float RemainingBudget = FMath::Min(Current.Water, Source.Water * 0.28f);
                for (const FIntVector& Offset : Laterals)
                {
                    if (RemainingBudget <= 0.001f) break;
                    const FIntVector Side = Coord + Offset;
                    if (!IsOpenCell(VoxelWorld, Side)) continue;
                    const float Transfer = FMath::Min(RemainingBudget, Source.Water * 0.07f);
                    Current.Water -= Transfer;
                    RemainingBudget -= Transfer;
                    Next.FindOrAdd(Side).Water += Transfer;
                }
            }
        }

        // Natural gas and smoke prefer to rise and then diffuse sideways through rooms/openings.
        if (Source.Gas > 0.01f)
        {
            const FIntVector Up = Coord + FIntVector(0,0,1);
            if (IsOpenCell(VoxelWorld, Up))
            {
                const float Transfer = FMath::Min(Current.Gas, Source.Gas * 0.24f);
                Current.Gas -= Transfer;
                Next.FindOrAdd(Up).Gas += Transfer;
            }
            for (const FIntVector& Offset : Laterals)
            {
                const FIntVector Side = Coord + Offset;
                if (!IsOpenCell(VoxelWorld, Side)) continue;
                const float Transfer = FMath::Min(Current.Gas, Source.Gas * 0.035f);
                Current.Gas -= Transfer;
                Next.FindOrAdd(Side).Gas += Transfer;
            }
        }

        if (Source.Smoke > 0.01f)
        {
            const FIntVector Up = Coord + FIntVector(0,0,1);
            if (IsOpenCell(VoxelWorld, Up))
            {
                const float Transfer = FMath::Min(Current.Smoke, Source.Smoke * 0.35f);
                Current.Smoke -= Transfer;
                Next.FindOrAdd(Up).Smoke += Transfer;
            }
            Current.Smoke *= 0.992f;
        }

        if (Source.Fire > 0.005f)
        {
            if (Current.Water > 0.02f)
            {
                const float Extinguish = FMath::Min(Current.Fire, Current.Water * 0.9f);
                Current.Fire -= Extinguish;
                Current.Water = FMath::Max(0.0f, Current.Water - Extinguish * 0.28f);
                Current.Temperature = FMath::Max(20.0f, Current.Temperature - Extinguish * 150.0f);
            }
            else
            {
                const float Combustible = Current.Gas + Current.Fuel;
                if (Combustible > 0.01f)
                {
                    const float Burn = FMath::Min(Combustible, 0.08f + Source.Fire * 0.08f);
                    const float GasBurn = FMath::Min(Current.Gas, Burn * 0.65f);
                    Current.Gas -= GasBurn;
                    Current.Fuel = FMath::Max(0.0f, Current.Fuel - (Burn - GasBurn));
                    Current.Fire = FMath::Min(3.0f, Current.Fire + Burn * 1.6f);
                }

                FDeadbrickVoxel Solid;
                if (VoxelWorld->GetVoxel(Coord, Solid) && Solid.Material == EDeadbrickVoxelMaterial::Wood)
                {
                    const int32 FireDamage = FMath::Clamp(FMath::RoundToInt(8.0f + Source.Fire * 18.0f), 1, 80);
                    if (FireDamage >= Solid.Integrity)
                        VoxelWorld->SetVoxel(Coord, EDeadbrickVoxelMaterial::Air, 0);
                    else
                        VoxelWorld->SetVoxel(Coord, Solid.Material, (uint8)(Solid.Integrity - FireDamage));
                    Current.Fire = FMath::Min(2.0f, Current.Fire + 0.08f);
                }

                for (const FIntVector& Offset : Neighbors)
                {
                    const FIntVector NeighborCoord = Coord + Offset;
                    FDeadbrickVoxel NeighborVoxel;
                    if (VoxelWorld->GetVoxel(NeighborCoord, NeighborVoxel) && NeighborVoxel.Material == EDeadbrickVoxelMaterial::Wood)
                    {
                        FDeadbrickEnvironmentCell& NeighborCell = Next.FindOrAdd(NeighborCoord);
                        if (FMath::FRand() < FMath::Clamp(Source.Fire * 0.07f, 0.01f, 0.22f))
                            NeighborCell.Fire = FMath::Max(NeighborCell.Fire, 0.15f);
                    }
                    else if (const FDeadbrickEnvironmentCell* OldNeighbor = Previous.Find(NeighborCoord))
                    {
                        if (OldNeighbor->Gas > 0.10f || OldNeighbor->Fuel > 0.10f)
                            Next.FindOrAdd(NeighborCoord).Fire = FMath::Max(Next.FindOrAdd(NeighborCoord).Fire, Source.Fire * 0.25f);
                    }
                }
            }

            Current.Smoke += Source.Fire * 0.12f;
            Current.Temperature = FMath::Max(Current.Temperature, 180.0f + Source.Fire * 220.0f);
            Current.Fire *= 0.975f;
        }
        else
        {
            Current.Temperature = FMath::Lerp(Current.Temperature, 20.0f, 0.08f);
        }

        Current.Water = FMath::Clamp(Current.Water, 0.0f, 4.0f);
        Current.Gas = FMath::Clamp(Current.Gas, 0.0f, 4.0f);
        Current.Fuel = FMath::Clamp(Current.Fuel, 0.0f, 4.0f);
        Current.Fire = FMath::Clamp(Current.Fire, 0.0f, 3.0f);
        Current.Smoke = FMath::Clamp(Current.Smoke, 0.0f, 4.0f);
    }
    VoxelWorld->EndBulkEdit();

    TArray<FIntVector> NextKeys;
    Next.GetKeys(NextKeys);
    for (const FIntVector& Coord : NextKeys)
    {
        if (FDeadbrickEnvironmentCell* Cell = Next.Find(Coord))
        {
            Cell->Gas *= 0.998f;
            Cell->Fuel *= 0.9995f;
            if (!Cell->IsActive()) Next.Remove(Coord);
        }
    }
    Cells = MoveTemp(Next);
}
