#include "World/ProceduralCityGenerator.h"

#include "Engine/StaticMesh.h"
#include "World/DestructibleVoxelWorld.h"
#include "World/ReferenceDestructibleProp.h"

void AProceduralCityGenerator::ImproveTraversalAndStreetLife()
{
    if (!VoxelWorld || Buildings.Num() == 0) return;

    const int32 BlockV = FMath::RoundToInt(BlockSizeMeters * 100.0f / VoxelWorld->VoxelSizeCm);
    const int32 StreetV = FMath::RoundToInt(StreetWidthMeters * 100.0f / VoxelWorld->VoxelSizeCm);
    const int32 FloorV = FMath::Max(8, FMath::RoundToInt(FloorHeightMeters * 100.0f / VoxelWorld->VoxelSizeCm));
    const int32 Margin = FMath::Max(4, FMath::RoundToInt(2.0f * 100.0f / VoxelWorld->VoxelSizeCm));
    const int32 StairWidth = FMath::Max(6, FMath::CeilToInt(120.0f / VoxelWorld->VoxelSizeCm));
    const int32 Headroom = FMath::Max(10, FMath::CeilToInt(210.0f / VoxelWorld->VoxelSizeCm));

    UStaticMesh* FallbackContainerMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    FRandomStream DetailStream(Seed ^ 0x6D2B79F5);

    VoxelWorld->BeginBulkEdit();

    for (int32 BuildingIndex = 0; BuildingIndex < Buildings.Num(); ++BuildingIndex)
    {
        const FDeadbrickBuildingSpec& Spec = Buildings[BuildingIndex];
        const int32 LotsPerSide = Spec.District == EDeadbrickDistrictType::Suburban ? 3 : 2;
        const int32 LotV = BlockV / LotsPerSide;
        const int32 OriginX = StreetV + Spec.Block.X * (BlockV + StreetV);
        const int32 OriginY = StreetV + Spec.Block.Y * (BlockV + StreetV);

        const int32 X0 = OriginX + Spec.Lot.X * LotV + Margin;
        const int32 Y0 = OriginY + Spec.Lot.Y * LotV + Margin;
        const int32 X1 = OriginX + (Spec.Lot.X + 1) * LotV - Margin - 1;
        const int32 Y1 = OriginY + (Spec.Lot.Y + 1) * LotV - Margin - 1;
        const int32 Z1 = Spec.Floors * FloorV;
        if (X1 - X0 < StairWidth + 8 || Y1 - Y0 < FloorV + 10) continue;

        // Rebuild the original narrow stair as a real FPS circulation corridor. The clear width is at
        // least 1.2 m at the current voxel scale and every tread has more than 2.1 m headroom.
        const int32 StairX0 = X0 + 4;
        const int32 StairY0 = Y0 + 4;
        const int32 StairRun = FMath::Clamp(FloorV + 4, 10, FMath::Max(10, Y1 - StairY0 - 5));

        for (int32 FloorBase = 0; FloorBase + FloorV < Z1; FloorBase += FloorV)
        {
            const int32 SlabZ = FloorBase + FloorV;

            // Wide opening through the slab and enough landing depth to turn without capsule snagging.
            for (int32 X = StairX0 - 1; X <= StairX0 + StairWidth; ++X)
            for (int32 Y = StairY0 - 1; Y <= StairY0 + StairRun + 2; ++Y)
            for (int32 Z = SlabZ - 1; Z <= SlabZ + 2; ++Z)
                VoxelWorld->SetVoxel(FIntVector(X, Y, Z), EDeadbrickVoxelMaterial::Air, 0);

            for (int32 Step = 0; Step < StairRun; ++Step)
            {
                const float Alpha = StairRun > 1 ? (float)Step / (float)(StairRun - 1) : 0.0f;
                const int32 Height = FMath::Clamp(FMath::FloorToInt(Alpha * (FloorV - 3)), 0, FloorV - 3);
                const int32 TreadZ = FloorBase + 2 + Height;
                const int32 TreadY = StairY0 + Step;

                for (int32 X = StairX0; X < StairX0 + StairWidth; ++X)
                {
                    VoxelWorld->SetVoxel(FIntVector(X, TreadY, TreadZ), EDeadbrickVoxelMaterial::Concrete);
                    for (int32 ClearZ = TreadZ + 1; ClearZ <= TreadZ + Headroom; ++ClearZ)
                        VoxelWorld->SetVoxel(FIntVector(X, TreadY, ClearZ), EDeadbrickVoxelMaterial::Air, 0);
                }
            }

            // Full landing at the top keeps movement from catching the lip of the next floor.
            const int32 LandingZ = SlabZ - 1;
            for (int32 X = StairX0; X < StairX0 + StairWidth; ++X)
            for (int32 Y = StairY0 + StairRun - 2; Y <= StairY0 + StairRun + 3; ++Y)
                VoxelWorld->SetVoxel(FIntVector(X, Y, LandingZ), EDeadbrickVoxelMaterial::Concrete);
        }

        // District-specific silhouette/details. These stay voxel-backed and therefore remain part of
        // the destruction/gathering loop instead of becoming decorative static geometry.
        const int32 CenterX = (X0 + X1) / 2;
        const int32 MidY = (Y0 + Y1) / 2;
        switch (Spec.District)
        {
            case EDeadbrickDistrictType::Commercial:
            case EDeadbrickDistrictType::Downtown:
                VoxelWorld->FillBox(
                    FIntVector(CenterX - 5, Y0 - 3, FMath::Max(6, FloorV / 2)),
                    FIntVector(CenterX + 5, Y0 - 1, FMath::Max(7, FloorV / 2 + 1)),
                    EDeadbrickVoxelMaterial::Metal);
                break;

            case EDeadbrickDistrictType::Industrial:
            case EDeadbrickDistrictType::Military:
                VoxelWorld->FillBox(
                    FIntVector(X1 + 1, MidY - 5, 2),
                    FIntVector(X1 + 5, MidY + 5, 2),
                    EDeadbrickVoxelMaterial::Metal);
                VoxelWorld->FillBox(
                    FIntVector(X1 + 4, MidY - 5, 3),
                    FIntVector(X1 + 4, MidY - 5, 8),
                    EDeadbrickVoxelMaterial::Metal);
                break;

            case EDeadbrickDistrictType::Residential:
            case EDeadbrickDistrictType::Suburban:
            case EDeadbrickDistrictType::LowIncome:
                VoxelWorld->FillBox(
                    FIntVector(CenterX - 4, Y0 - 4, 6),
                    FIntVector(CenterX + 4, Y0 - 1, 6),
                    EDeadbrickVoxelMaterial::Wood);
                break;

            default:
                VoxelWorld->FillBox(
                    FIntVector(CenterX - 3, Y0 - 2, 7),
                    FIntVector(CenterX + 3, Y0 - 1, 8),
                    EDeadbrickVoxelMaterial::Concrete);
                break;
        }

        // Guaranteed scavenging layer. When a LOTL container mesh is available it is used; otherwise
        // a clearly proportioned physical box stands in without blocking the gameplay system.
        if (BuildingIndex % 2 == 0)
        {
            UStaticMesh* ContainerMesh = ReferenceContainerMeshes.Num() > 0
                ? PickReferenceMesh(ReferenceContainerMeshes, DetailStream)
                : FallbackContainerMesh;

            if (ContainerMesh)
            {
                const int32 ContainerX = X1 + 4;
                const int32 ContainerY = FMath::Clamp(MidY + DetailStream.RandRange(-4, 4), Y0 + 4, Y1 - 4);
                SpawnReferenceProp(
                    ContainerMesh,
                    VoxelWorld->VoxelToWorld(FIntVector(ContainerX, ContainerY, 3)),
                    FRotator(0.0f, DetailStream.FRandRange(-12.0f, 12.0f), 0.0f),
                    FVector(135.0f, 85.0f, 90.0f),
                    EDeadbrickVoxelMaterial::Metal,
                    180.0f,
                    EDeadbrickReferencePropRole::Container);
            }
        }
    }

    VoxelWorld->EndBulkEdit();

    UE_LOG(LogTemp, Display,
        TEXT("DEADBRICK city traversal pass: %d buildings received FPS-width stair clearance, district detail and scavenging support."),
        Buildings.Num());
}
