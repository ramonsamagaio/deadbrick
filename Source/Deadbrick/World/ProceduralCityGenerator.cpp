#include "World/ProceduralCityGenerator.h"

#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Reference/ReferenceAssetResolver.h"
#include "World/DestructibleVoxelWorld.h"
#include "World/ReferenceDestructibleProp.h"

AProceduralCityGenerator::AProceduralCityGenerator()
{
    PrimaryActorTick.bCanEverTick = false;
}

EDeadbrickDistrictType AProceduralCityGenerator::PickDistrict(FRandomStream& Stream, int32 BlockX, int32 BlockY) const
{
    const float CenterDistance = FVector2D((float)BlockX - BlocksPerAxis * 0.5f, (float)BlockY - BlocksPerAxis * 0.5f).Size();
    if (CenterDistance < BlocksPerAxis * 0.22f && Stream.FRand() < 0.7f) return EDeadbrickDistrictType::Downtown;

    const int32 Roll = Stream.RandRange(0, 99);
    if (Roll < 24) return EDeadbrickDistrictType::Residential;
    if (Roll < 39) return EDeadbrickDistrictType::Commercial;
    if (Roll < 52) return EDeadbrickDistrictType::Industrial;
    if (Roll < 62) return EDeadbrickDistrictType::Suburban;
    if (Roll < 72) return EDeadbrickDistrictType::LowIncome;
    if (Roll < 80) return EDeadbrickDistrictType::Civic;
    if (Roll < 87) return EDeadbrickDistrictType::Medical;
    if (Roll < 94) return EDeadbrickDistrictType::University;
    return EDeadbrickDistrictType::Military;
}

int32 AProceduralCityGenerator::PickFloors(EDeadbrickDistrictType District, FRandomStream& Stream) const
{
    switch (District)
    {
        case EDeadbrickDistrictType::Downtown: return Stream.RandRange(6, 18);
        case EDeadbrickDistrictType::Commercial: return Stream.RandRange(2, 7);
        case EDeadbrickDistrictType::Industrial: return Stream.RandRange(1, 4);
        case EDeadbrickDistrictType::Medical: return Stream.RandRange(3, 8);
        case EDeadbrickDistrictType::University: return Stream.RandRange(2, 6);
        case EDeadbrickDistrictType::Military: return Stream.RandRange(1, 4);
        case EDeadbrickDistrictType::Suburban: return Stream.RandRange(1, 2);
        default: return Stream.RandRange(2, 5);
    }
}

void AProceduralCityGenerator::LoadReferencePropMeshes()
{
    ReferenceDoorMesh = DeadbrickReferenceAssets::FindStaticMesh({TEXT("door"), TEXT("gate")});
    ReferenceWindowMesh = DeadbrickReferenceAssets::FindStaticMesh({TEXT("window"), TEXT("glass")});
    ReferenceContainerMesh = DeadbrickReferenceAssets::FindStaticMesh({TEXT("container"), TEXT("crate"), TEXT("chest"), TEXT("box")});
}

void AProceduralCityGenerator::GenerateCity()
{
    if (!VoxelWorld)
    {
        for (TActorIterator<ADestructibleVoxelWorld> It(GetWorld()); It; ++It)
        {
            VoxelWorld = *It;
            break;
        }
    }
    if (!VoxelWorld) return;

    LoadReferencePropMeshes();

    FRandomStream Stream(Seed);
    Buildings.Reset();
    VoxelWorld->BeginBulkEdit();
    BuildRoadGrid(Stream);

    for (int32 Y = 0; Y < BlocksPerAxis; ++Y)
    for (int32 X = 0; X < BlocksPerAxis; ++X)
    {
        BuildBlock(X, Y, PickDistrict(Stream, X, Y), Stream);
    }

    VoxelWorld->EndBulkEdit();
}

void AProceduralCityGenerator::BuildRoadGrid(FRandomStream& Stream)
{
    const float CmPerMeter = 100.0f;
    const int32 BlockV = FMath::RoundToInt(BlockSizeMeters * CmPerMeter / VoxelWorld->VoxelSizeCm);
    const int32 StreetV = FMath::RoundToInt(StreetWidthMeters * CmPerMeter / VoxelWorld->VoxelSizeCm);
    const int32 Span = BlocksPerAxis * BlockV + (BlocksPerAxis + 1) * StreetV;

    // Ground and roads are also voxel cells, so bullets/explosions can damage them exactly like walls.
    VoxelWorld->FillBox(FIntVector(0, 0, -3), FIntVector(Span, Span, -1), EDeadbrickVoxelMaterial::Soil);

    for (int32 I = 0; I <= BlocksPerAxis; ++I)
    {
        const int32 Start = I * (BlockV + StreetV);
        VoxelWorld->FillBox(FIntVector(Start, 0, 0), FIntVector(Start + StreetV - 1, Span, 0), EDeadbrickVoxelMaterial::Asphalt);
        VoxelWorld->FillBox(FIntVector(0, Start, 0), FIntVector(Span, Start + StreetV - 1, 0), EDeadbrickVoxelMaterial::Asphalt);
    }
}

void AProceduralCityGenerator::BuildBlock(int32 BlockX, int32 BlockY, EDeadbrickDistrictType District, FRandomStream& Stream)
{
    const int32 BlockV = FMath::RoundToInt(BlockSizeMeters * 100.0f / VoxelWorld->VoxelSizeCm);
    const int32 StreetV = FMath::RoundToInt(StreetWidthMeters * 100.0f / VoxelWorld->VoxelSizeCm);
    const int32 FloorV = FMath::Max(3, FMath::RoundToInt(FloorHeightMeters * 100.0f / VoxelWorld->VoxelSizeCm));
    const int32 OriginX = StreetV + BlockX * (BlockV + StreetV);
    const int32 OriginY = StreetV + BlockY * (BlockV + StreetV);

    const int32 LotsPerSide = District == EDeadbrickDistrictType::Suburban ? 3 : 2;
    const int32 LotV = BlockV / LotsPerSide;

    for (int32 LY = 0; LY < LotsPerSide; ++LY)
    for (int32 LX = 0; LX < LotsPerSide; ++LX)
    {
        if (Stream.FRand() < 0.10f) continue;
        const int32 Margin = FMath::Max(4, FMath::RoundToInt(2.0f * 100.0f / VoxelWorld->VoxelSizeCm));
        const int32 X0 = OriginX + LX * LotV + Margin;
        const int32 Y0 = OriginY + LY * LotV + Margin;
        const int32 X1 = OriginX + (LX + 1) * LotV - Margin - 1;
        const int32 Y1 = OriginY + (LY + 1) * LotV - Margin - 1;
        const int32 Floors = PickFloors(District, Stream);
        const int32 Z1 = Floors * FloorV;

        const EDeadbrickVoxelMaterial WallMaterial =
            (District == EDeadbrickDistrictType::Industrial || District == EDeadbrickDistrictType::Military)
                ? EDeadbrickVoxelMaterial::Concrete
                : EDeadbrickVoxelMaterial::Brick;

        BuildShell(FIntVector(X0, Y0, 1), FIntVector(X1, Y1, Z1), FloorV, WallMaterial);

        if (ReferenceContainerMesh && Stream.FRand() < 0.65f)
        {
            const FVector ContainerLocation = VoxelWorld->VoxelToWorld(FIntVector(X0 + 3, FMath::Max(1, Y0 - 5), 3));
            SpawnReferenceProp(
                ReferenceContainerMesh,
                ContainerLocation,
                FRotator(0.0f, Stream.FRandRange(-20.0f, 20.0f), 0.0f),
                FVector(180.0f, 100.0f, 110.0f),
                EDeadbrickVoxelMaterial::Wood,
                75.0f);
        }

        FDeadbrickBuildingSpec Spec;
        Spec.Block = FIntPoint(BlockX, BlockY);
        Spec.Lot = FIntPoint(LX, LY);
        Spec.Floors = Floors;
        Spec.District = District;
        Buildings.Add(Spec);
    }
}

void AProceduralCityGenerator::BuildShell(const FIntVector& Min, const FIntVector& Max, int32 FloorHeightVoxels, EDeadbrickVoxelMaterial WallMaterial)
{
    const int32 Wall = 2;
    VoxelWorld->FillBox(FIntVector(Min.X, Min.Y, Min.Z), FIntVector(Max.X, Max.Y, Min.Z + Wall - 1), EDeadbrickVoxelMaterial::Concrete);

    for (int32 Z = Min.Z; Z <= Max.Z; ++Z)
    {
        const bool IsSlab = ((Z - Min.Z) % FloorHeightVoxels) < Wall || Z >= Max.Z - Wall + 1;
        if (IsSlab)
        {
            VoxelWorld->FillBox(FIntVector(Min.X, Min.Y, Z), FIntVector(Max.X, Max.Y, FMath::Min(Max.Z, Z + Wall - 1)), EDeadbrickVoxelMaterial::Concrete);
            Z += Wall - 1;
            continue;
        }

        VoxelWorld->FillBox(FIntVector(Min.X, Min.Y, Z), FIntVector(Min.X + Wall - 1, Max.Y, Z), WallMaterial);
        VoxelWorld->FillBox(FIntVector(Max.X - Wall + 1, Min.Y, Z), FIntVector(Max.X, Max.Y, Z), WallMaterial);
        VoxelWorld->FillBox(FIntVector(Min.X, Min.Y, Z), FIntVector(Max.X, Min.Y + Wall - 1, Z), WallMaterial);
        VoxelWorld->FillBox(FIntVector(Min.X, Max.Y - Wall + 1, Z), FIntVector(Max.X, Max.Y, Z), WallMaterial);
    }

    const int32 DoorWidth = FMath::Max(4, (Max.X - Min.X) / 8);
    const int32 DoorCenter = (Min.X + Max.X) / 2;
    const int32 DoorTop = Min.Z + FMath::Min(FloorHeightVoxels - 2, 12);

    for (int32 Z = Min.Z + 2; Z <= DoorTop; ++Z)
    for (int32 X = DoorCenter - DoorWidth / 2; X <= DoorCenter + DoorWidth / 2; ++X)
    {
        VoxelWorld->SetVoxel(FIntVector(X, Min.Y, Z), EDeadbrickVoxelMaterial::Air, 0);
        VoxelWorld->SetVoxel(FIntVector(X, Min.Y + 1, Z), EDeadbrickVoxelMaterial::Air, 0);
    }

    if (ReferenceDoorMesh)
    {
        const int32 DoorMidZ = (Min.Z + 2 + DoorTop) / 2;
        SpawnReferenceProp(
            ReferenceDoorMesh,
            VoxelWorld->VoxelToWorld(FIntVector(DoorCenter, Min.Y, DoorMidZ)),
            FRotator::ZeroRotator,
            FVector((DoorWidth + 1) * VoxelWorld->VoxelSizeCm, 16.0f, (DoorTop - Min.Z) * VoxelWorld->VoxelSizeCm),
            EDeadbrickVoxelMaterial::Wood,
            90.0f);
    }

    // Cut real voxel window openings on the facade. When a reference window mesh is available,
    // it occupies the opening as a destructible visual and converts to voxel debris when broken.
    const int32 Width = Max.X - Min.X + 1;
    const int32 WindowHalfWidth = FMath::Clamp(Width / 14, 2, 5);
    const int32 WindowCenters[2] = { Min.X + Width / 3, Min.X + (Width * 2) / 3 };

    for (int32 FloorBase = Min.Z; FloorBase + FloorHeightVoxels <= Max.Z + 1; FloorBase += FloorHeightVoxels)
    {
        const int32 WindowBottom = FloorBase + FMath::Max(3, FloorHeightVoxels / 3);
        const int32 WindowTop = FMath::Min(FloorBase + FloorHeightVoxels - 3, WindowBottom + FMath::Max(3, FloorHeightVoxels / 3));
        if (WindowTop <= WindowBottom) continue;

        for (const int32 WindowCenter : WindowCenters)
        {
            if (FloorBase == Min.Z && FMath::Abs(WindowCenter - DoorCenter) <= DoorWidth) continue;

            for (int32 Z = WindowBottom; Z <= WindowTop; ++Z)
            for (int32 X = WindowCenter - WindowHalfWidth; X <= WindowCenter + WindowHalfWidth; ++X)
            {
                VoxelWorld->SetVoxel(FIntVector(X, Min.Y, Z), EDeadbrickVoxelMaterial::Air, 0);
                VoxelWorld->SetVoxel(FIntVector(X, Min.Y + 1, Z), EDeadbrickVoxelMaterial::Air, 0);
            }

            if (ReferenceWindowMesh)
            {
                const int32 WindowMidZ = (WindowBottom + WindowTop) / 2;
                SpawnReferenceProp(
                    ReferenceWindowMesh,
                    VoxelWorld->VoxelToWorld(FIntVector(WindowCenter, Min.Y, WindowMidZ)),
                    FRotator::ZeroRotator,
                    FVector((WindowHalfWidth * 2 + 1) * VoxelWorld->VoxelSizeCm, 10.0f, (WindowTop - WindowBottom + 1) * VoxelWorld->VoxelSizeCm),
                    EDeadbrickVoxelMaterial::Glass,
                    35.0f);
            }
        }
    }
}

void AProceduralCityGenerator::SpawnReferenceProp(
    UStaticMesh* Mesh,
    const FVector& WorldLocation,
    const FRotator& Rotation,
    const FVector& TargetDimensionsCm,
    EDeadbrickVoxelMaterial BreakMaterial,
    float Health)
{
    if (!Mesh || !GetWorld() || !VoxelWorld) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AReferenceDestructibleProp* Prop = GetWorld()->SpawnActor<AReferenceDestructibleProp>(
        AReferenceDestructibleProp::StaticClass(), WorldLocation, Rotation, SpawnParams);
    if (Prop)
    {
        Prop->InitializeFromReference(Mesh, VoxelWorld, BreakMaterial, TargetDimensionsCm, Health);
    }
}
