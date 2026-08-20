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
    ReferenceDoorMeshes.Reset();
    ReferenceWindowMeshes.Reset();
    ReferenceContainerMeshes.Reset();
    ReferenceFurnitureMeshes.Reset();
    ReferenceUtilityMeshes.Reset();

    for (UStaticMesh* Mesh : DeadbrickReferenceAssets::FindStaticMeshes({TEXT("door"), TEXT("gate"), TEXT("hatch")}, 12))
        if (Mesh) ReferenceDoorMeshes.Add(Mesh);
    for (UStaticMesh* Mesh : DeadbrickReferenceAssets::FindStaticMeshes({TEXT("window"), TEXT("glass")}, 12))
        if (Mesh) ReferenceWindowMeshes.Add(Mesh);
    for (UStaticMesh* Mesh : DeadbrickReferenceAssets::FindStaticMeshes({TEXT("container"), TEXT("crate"), TEXT("locker"), TEXT("chest"), TEXT("box")}, 16))
        if (Mesh) ReferenceContainerMeshes.Add(Mesh);
    for (UStaticMesh* Mesh : DeadbrickReferenceAssets::FindStaticMeshes({TEXT("chair"), TEXT("table"), TEXT("desk"), TEXT("shelf"), TEXT("bed"), TEXT("cabinet")}, 20))
        if (Mesh) ReferenceFurnitureMeshes.Add(Mesh);
    for (UStaticMesh* Mesh : DeadbrickReferenceAssets::FindStaticMeshes({TEXT("pipe"), TEXT("vent"), TEXT("barrel"), TEXT("generator"), TEXT("utility"), TEXT("machine")}, 20))
        if (Mesh) ReferenceUtilityMeshes.Add(Mesh);

    UE_LOG(LogTemp, Display,
        TEXT("DEADBRICK reference prop pools: %d doors, %d windows, %d containers, %d furniture, %d utility"),
        ReferenceDoorMeshes.Num(),
        ReferenceWindowMeshes.Num(),
        ReferenceContainerMeshes.Num(),
        ReferenceFurnitureMeshes.Num(),
        ReferenceUtilityMeshes.Num());
}

UStaticMesh* AProceduralCityGenerator::PickReferenceMesh(const TArray<TObjectPtr<UStaticMesh>>& Pool, FRandomStream& Stream) const
{
    if (Pool.Num() == 0) return nullptr;
    return Pool[Stream.RandRange(0, Pool.Num() - 1)].Get();
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

    const int32 SoilDepth = FMath::Max(8, FMath::RoundToInt(2.4f * 100.0f / VoxelWorld->VoxelSizeCm));
    VoxelWorld->FillBox(FIntVector(0, 0, -SoilDepth), FIntVector(Span, Span, -1), EDeadbrickVoxelMaterial::Soil);

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
    const int32 FloorV = FMath::Max(8, FMath::RoundToInt(FloorHeightMeters * 100.0f / VoxelWorld->VoxelSizeCm));
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
        if (X1 - X0 < 12 || Y1 - Y0 < 12) continue;

        const int32 Floors = PickFloors(District, Stream);
        const int32 Z1 = Floors * FloorV;
        const EDeadbrickVoxelMaterial WallMaterial =
            (District == EDeadbrickDistrictType::Industrial || District == EDeadbrickDistrictType::Military)
                ? EDeadbrickVoxelMaterial::Concrete
                : EDeadbrickVoxelMaterial::Brick;

        // Raised pavement apron makes the lot read as an urban building rather than a box planted
        // directly into an asphalt plane. It remains voxel material and can still be broken.
        VoxelWorld->FillBox(
            FIntVector(X0 - 3, Y0 - 3, 1),
            FIntVector(X1 + 3, Y1 + 3, 1),
            EDeadbrickVoxelMaterial::Concrete);

        BuildShell(FIntVector(X0, Y0, 0), FIntVector(X1, Y1, Z1), FloorV, WallMaterial, Stream);
        SpawnReferenceClutter(FIntVector(X0, Y0, 0), FIntVector(X1, Y1, Z1), FloorV, Stream);

        if (ReferenceContainerMeshes.Num() > 0 && Stream.FRand() < 0.80f)
        {
            UStaticMesh* ContainerMesh = PickReferenceMesh(ReferenceContainerMeshes, Stream);
            const FVector ContainerLocation = VoxelWorld->VoxelToWorld(FIntVector(X0 + 3, FMath::Max(1, Y0 - 5), 3));
            SpawnReferenceProp(
                ContainerMesh,
                ContainerLocation,
                FRotator(0.0f, Stream.FRandRange(-20.0f, 20.0f), 0.0f),
                FVector(150.0f, 95.0f, 105.0f),
                EDeadbrickVoxelMaterial::Metal,
                110.0f,
                EDeadbrickReferencePropRole::Container);
        }

        FDeadbrickBuildingSpec Spec;
        Spec.Block = FIntPoint(BlockX, BlockY);
        Spec.Lot = FIntPoint(LX, LY);
        Spec.Floors = Floors;
        Spec.District = District;
        Buildings.Add(Spec);
    }
}

void AProceduralCityGenerator::BuildShell(
    const FIntVector& Min,
    const FIntVector& Max,
    int32 FloorHeightVoxels,
    EDeadbrickVoxelMaterial WallMaterial,
    FRandomStream& Stream)
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

    BuildInterior(Min, Max, FloorHeightVoxels, Stream);
    BuildVoxelStairwell(Min, Max, FloorHeightVoxels);

    const int32 DoorWidth = FMath::Clamp((Max.X - Min.X) / 8, 4, 8);
    const int32 DoorCenter = (Min.X + Max.X) / 2;
    const int32 DoorTop = Min.Z + FMath::Min(FloorHeightVoxels - 3, FMath::Max(9, FloorHeightVoxels * 3 / 4));

    for (int32 Z = Min.Z + 2; Z <= DoorTop; ++Z)
    for (int32 X = DoorCenter - DoorWidth / 2; X <= DoorCenter + DoorWidth / 2; ++X)
    {
        VoxelWorld->SetVoxel(FIntVector(X, Min.Y, Z), EDeadbrickVoxelMaterial::Air, 0);
        VoxelWorld->SetVoxel(FIntVector(X, Min.Y + 1, Z), EDeadbrickVoxelMaterial::Air, 0);
    }

    if (UStaticMesh* DoorMesh = PickReferenceMesh(ReferenceDoorMeshes, Stream))
    {
        const int32 DoorMidZ = (Min.Z + 2 + DoorTop) / 2;
        SpawnReferenceProp(
            DoorMesh,
            VoxelWorld->VoxelToWorld(FIntVector(DoorCenter, Min.Y, DoorMidZ)),
            FRotator::ZeroRotator,
            FVector((DoorWidth + 1) * VoxelWorld->VoxelSizeCm, 16.0f, (DoorTop - Min.Z) * VoxelWorld->VoxelSizeCm),
            EDeadbrickVoxelMaterial::Wood,
            90.0f,
            EDeadbrickReferencePropRole::Door);
    }

    const int32 Width = Max.X - Min.X + 1;
    const int32 WindowHalfWidth = FMath::Clamp(Width / 14, 2, 5);
    const int32 WindowCenters[2] = { Min.X + Width / 3, Min.X + (Width * 2) / 3 };

    for (int32 FloorBase = Min.Z; FloorBase + FloorHeightVoxels <= Max.Z + 1; FloorBase += FloorHeightVoxels)
    {
        const int32 WindowBottom = FloorBase + FMath::Max(4, FloorHeightVoxels / 3);
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

            // Destructible metal sill/header gives openings actual construction detail even before a
            // reference window mesh is available.
            VoxelWorld->FillBox(
                FIntVector(WindowCenter - WindowHalfWidth - 1, Min.Y - 1, WindowBottom - 1),
                FIntVector(WindowCenter + WindowHalfWidth + 1, Min.Y - 1, WindowBottom - 1),
                EDeadbrickVoxelMaterial::Metal);
            VoxelWorld->FillBox(
                FIntVector(WindowCenter - WindowHalfWidth - 1, Min.Y - 1, WindowTop + 1),
                FIntVector(WindowCenter + WindowHalfWidth + 1, Min.Y - 1, WindowTop + 1),
                EDeadbrickVoxelMaterial::Metal);

            if (UStaticMesh* WindowMesh = PickReferenceMesh(ReferenceWindowMeshes, Stream))
            {
                const int32 WindowMidZ = (WindowBottom + WindowTop) / 2;
                SpawnReferenceProp(
                    WindowMesh,
                    VoxelWorld->VoxelToWorld(FIntVector(WindowCenter, Min.Y, WindowMidZ)),
                    FRotator::ZeroRotator,
                    FVector((WindowHalfWidth * 2 + 1) * VoxelWorld->VoxelSizeCm, 10.0f, (WindowTop - WindowBottom + 1) * VoxelWorld->VoxelSizeCm),
                    EDeadbrickVoxelMaterial::Glass,
                    35.0f,
                    EDeadbrickReferencePropRole::Window);
            }
        }
    }

    BuildFacadeDetails(Min, Max, FloorHeightVoxels, Stream);
    BuildRoofDetails(Min, Max, Stream);
}

void AProceduralCityGenerator::BuildInterior(
    const FIntVector& Min,
    const FIntVector& Max,
    int32 FloorHeightVoxels,
    FRandomStream& Stream)
{
    const int32 InnerMinX = Min.X + 3;
    const int32 InnerMaxX = Max.X - 3;
    const int32 InnerMinY = Min.Y + 3;
    const int32 InnerMaxY = Max.Y - 3;
    if (InnerMaxX - InnerMinX < 8 || InnerMaxY - InnerMinY < 8) return;

    for (int32 FloorBase = Min.Z; FloorBase + FloorHeightVoxels <= Max.Z + 1; FloorBase += FloorHeightVoxels)
    {
        const int32 WallBottom = FloorBase + 2;
        const int32 WallTop = FMath::Min(Max.Z - 2, FloorBase + FloorHeightVoxels - 3);
        if (WallTop <= WallBottom) continue;

        const int32 SplitX = FMath::Clamp((InnerMinX + InnerMaxX) / 2 + Stream.RandRange(-2, 2), InnerMinX + 3, InnerMaxX - 3);
        const int32 SplitY = FMath::Clamp((InnerMinY + InnerMaxY) / 2 + Stream.RandRange(-2, 2), InnerMinY + 3, InnerMaxY - 3);

        VoxelWorld->FillBox(FIntVector(SplitX, InnerMinY, WallBottom), FIntVector(SplitX, InnerMaxY, WallTop), EDeadbrickVoxelMaterial::Brick);
        VoxelWorld->FillBox(FIntVector(InnerMinX, SplitY, WallBottom), FIntVector(InnerMaxX, SplitY, WallTop), EDeadbrickVoxelMaterial::Brick);

        const int32 DoorHeight = FMath::Min(WallTop, WallBottom + FMath::Max(6, FloorHeightVoxels * 2 / 3));
        const int32 DoorHalfWidth = 2;

        const int32 DoorY = (InnerMinY + SplitY) / 2;
        for (int32 Y = DoorY - DoorHalfWidth; Y <= DoorY + DoorHalfWidth; ++Y)
        for (int32 Z = WallBottom; Z <= DoorHeight; ++Z)
            VoxelWorld->SetVoxel(FIntVector(SplitX, Y, Z), EDeadbrickVoxelMaterial::Air, 0);

        const int32 DoorX = (SplitX + InnerMaxX) / 2;
        for (int32 X = DoorX - DoorHalfWidth; X <= DoorX + DoorHalfWidth; ++X)
        for (int32 Z = WallBottom; Z <= DoorHeight; ++Z)
            VoxelWorld->SetVoxel(FIntVector(X, SplitY, Z), EDeadbrickVoxelMaterial::Air, 0);

        // Load-bearing columns make the interior visually legible and give structural destruction
        // meaningful targets instead of every floor being an empty cross-shaped shell.
        const int32 ColumnXs[2] = { InnerMinX + 3, InnerMaxX - 3 };
        const int32 ColumnYs[2] = { InnerMinY + 3, InnerMaxY - 3 };
        for (const int32 CX : ColumnXs)
        for (const int32 CY : ColumnYs)
        {
            VoxelWorld->FillBox(
                FIntVector(CX, CY, WallBottom),
                FIntVector(CX + 1, CY + 1, WallTop),
                EDeadbrickVoxelMaterial::Concrete);
        }

        // Small counters, shelving and utility benches. They are voxel-backed, material-aware and
        // salvageable, so interior clutter participates in the same destruction loop.
        if (Stream.FRand() < 0.80f)
        {
            const int32 CounterLength = FMath::Clamp((InnerMaxX - InnerMinX) / 4, 4, 10);
            VoxelWorld->FillBox(
                FIntVector(InnerMinX + 2, InnerMinY + 1, WallBottom),
                FIntVector(InnerMinX + 2 + CounterLength, InnerMinY + 3, FMath::Min(WallBottom + 3, WallTop)),
                Stream.FRand() < 0.5f ? EDeadbrickVoxelMaterial::Wood : EDeadbrickVoxelMaterial::Metal);
        }

        if (Stream.FRand() < 0.55f)
        {
            const int32 ClosetX = InnerMaxX - 5;
            const int32 ClosetY = InnerMaxY - 5;
            VoxelWorld->FillBox(
                FIntVector(ClosetX, ClosetY, WallBottom),
                FIntVector(InnerMaxX, ClosetY, FMath::Min(WallBottom + 5, WallTop)),
                EDeadbrickVoxelMaterial::Wood);
            VoxelWorld->FillBox(
                FIntVector(ClosetX, ClosetY, WallBottom),
                FIntVector(ClosetX, InnerMaxY, FMath::Min(WallBottom + 5, WallTop)),
                EDeadbrickVoxelMaterial::Wood);
        }
    }
}

void AProceduralCityGenerator::BuildVoxelStairwell(const FIntVector& Min, const FIntVector& Max, int32 FloorHeightVoxels)
{
    if (FloorHeightVoxels < 8) return;

    const int32 StairX0 = Min.X + 4;
    const int32 StairY0 = Min.Y + 4;
    const int32 StairWidth = 3;
    const int32 StairRun = FMath::Clamp(FloorHeightVoxels, 8, FMath::Max(8, Max.Y - StairY0 - 4));

    for (int32 FloorBase = Min.Z; FloorBase + FloorHeightVoxels < Max.Z; FloorBase += FloorHeightVoxels)
    {
        const int32 SlabZ = FloorBase + FloorHeightVoxels;

        for (int32 X = StairX0; X < StairX0 + StairWidth; ++X)
        for (int32 Y = StairY0; Y <= StairY0 + StairRun; ++Y)
        for (int32 Z = SlabZ; Z <= SlabZ + 1; ++Z)
            VoxelWorld->SetVoxel(FIntVector(X, Y, Z), EDeadbrickVoxelMaterial::Air, 0);

        for (int32 Step = 0; Step < StairRun; ++Step)
        {
            const float Alpha = StairRun > 1 ? (float)Step / (float)(StairRun - 1) : 0.0f;
            const int32 Height = FMath::Clamp(FMath::FloorToInt(Alpha * (FloorHeightVoxels - 2)), 0, FloorHeightVoxels - 2);
            for (int32 X = StairX0; X < StairX0 + StairWidth; ++X)
            {
                VoxelWorld->SetVoxel(FIntVector(X, StairY0 + Step, FloorBase + 2 + Height), EDeadbrickVoxelMaterial::Concrete);
            }
        }
    }
}

void AProceduralCityGenerator::BuildFacadeDetails(
    const FIntVector& Min,
    const FIntVector& Max,
    int32 FloorHeightVoxels,
    FRandomStream& Stream)
{
    // Corner piers stop the procedural shell from reading as four perfectly flat planes.
    VoxelWorld->FillBox(FIntVector(Min.X - 1, Min.Y - 1, Min.Z + 2), FIntVector(Min.X, Min.Y, Max.Z - 1), EDeadbrickVoxelMaterial::Concrete);
    VoxelWorld->FillBox(FIntVector(Max.X, Min.Y - 1, Min.Z + 2), FIntVector(Max.X + 1, Min.Y, Max.Z - 1), EDeadbrickVoxelMaterial::Concrete);
    VoxelWorld->FillBox(FIntVector(Min.X - 1, Max.Y, Min.Z + 2), FIntVector(Min.X, Max.Y + 1, Max.Z - 1), EDeadbrickVoxelMaterial::Concrete);
    VoxelWorld->FillBox(FIntVector(Max.X, Max.Y, Min.Z + 2), FIntVector(Max.X + 1, Max.Y + 1, Max.Z - 1), EDeadbrickVoxelMaterial::Concrete);

    // Floor bands, facade ledges and a real projecting entrance canopy.
    for (int32 FloorBase = Min.Z + FloorHeightVoxels; FloorBase < Max.Z; FloorBase += FloorHeightVoxels)
    {
        VoxelWorld->FillBox(FIntVector(Min.X, Min.Y - 1, FloorBase), FIntVector(Max.X, Min.Y - 1, FloorBase), EDeadbrickVoxelMaterial::Concrete);
        VoxelWorld->FillBox(FIntVector(Min.X, Max.Y + 1, FloorBase), FIntVector(Max.X, Max.Y + 1, FloorBase), EDeadbrickVoxelMaterial::Concrete);
    }

    const int32 CenterX = (Min.X + Max.X) / 2;
    const int32 CanopyZ = Min.Z + FMath::Max(7, FloorHeightVoxels * 3 / 4) + 2;
    const int32 CanopyHalf = FMath::Clamp((Max.X - Min.X) / 10, 4, 8);
    VoxelWorld->FillBox(
        FIntVector(CenterX - CanopyHalf, Min.Y - 4, CanopyZ),
        FIntVector(CenterX + CanopyHalf, Min.Y - 1, CanopyZ),
        EDeadbrickVoxelMaterial::Metal);

    // A sparse destructible fire escape/balcony breaks the silhouette and creates navigable rubble.
    if (Max.Z - Min.Z >= FloorHeightVoxels * 2 && Stream.FRand() < 0.72f)
    {
        const int32 BalconyY0 = (Min.Y + Max.Y) / 2 - 4;
        const int32 BalconyY1 = BalconyY0 + 8;
        for (int32 FloorBase = Min.Z + FloorHeightVoxels; FloorBase < Max.Z; FloorBase += FloorHeightVoxels * 2)
        {
            const int32 PlatformZ = FloorBase + 2;
            VoxelWorld->FillBox(
                FIntVector(Max.X + 1, BalconyY0, PlatformZ),
                FIntVector(Max.X + 5, BalconyY1, PlatformZ),
                EDeadbrickVoxelMaterial::Metal);

            VoxelWorld->FillBox(
                FIntVector(Max.X + 5, BalconyY0, PlatformZ + 1),
                FIntVector(Max.X + 5, BalconyY0, PlatformZ + 4),
                EDeadbrickVoxelMaterial::Metal);
            VoxelWorld->FillBox(
                FIntVector(Max.X + 5, BalconyY1, PlatformZ + 1),
                FIntVector(Max.X + 5, BalconyY1, PlatformZ + 4),
                EDeadbrickVoxelMaterial::Metal);
            VoxelWorld->FillBox(
                FIntVector(Max.X + 5, BalconyY0, PlatformZ + 4),
                FIntVector(Max.X + 5, BalconyY1, PlatformZ + 4),
                EDeadbrickVoxelMaterial::Metal);
        }
    }
}

void AProceduralCityGenerator::BuildRoofDetails(const FIntVector& Min, const FIntVector& Max, FRandomStream& Stream)
{
    const int32 RoofZ = Max.Z + 1;

    // Parapet perimeter.
    VoxelWorld->FillBox(FIntVector(Min.X, Min.Y, RoofZ), FIntVector(Max.X, Min.Y, RoofZ + 2), EDeadbrickVoxelMaterial::Concrete);
    VoxelWorld->FillBox(FIntVector(Min.X, Max.Y, RoofZ), FIntVector(Max.X, Max.Y, RoofZ + 2), EDeadbrickVoxelMaterial::Concrete);
    VoxelWorld->FillBox(FIntVector(Min.X, Min.Y, RoofZ), FIntVector(Min.X, Max.Y, RoofZ + 2), EDeadbrickVoxelMaterial::Concrete);
    VoxelWorld->FillBox(FIntVector(Max.X, Min.Y, RoofZ), FIntVector(Max.X, Max.Y, RoofZ + 2), EDeadbrickVoxelMaterial::Concrete);

    const int32 MidX = (Min.X + Max.X) / 2;
    const int32 MidY = (Min.Y + Max.Y) / 2;

    // HVAC housings and vents are intentionally separate material islands attached to the roof so
    // they can tear loose and become metal salvage during a collapse.
    VoxelWorld->FillBox(
        FIntVector(MidX - 5, MidY - 4, RoofZ),
        FIntVector(MidX, MidY + 1, RoofZ + 4),
        EDeadbrickVoxelMaterial::Metal);

    if (Stream.FRand() < 0.75f)
    {
        VoxelWorld->FillBox(
            FIntVector(MidX + 4, MidY + 3, RoofZ),
            FIntVector(MidX + 7, MidY + 6, RoofZ + 3),
            EDeadbrickVoxelMaterial::Metal);
    }

    VoxelWorld->FillBox(
        FIntVector(MidX + 2, MidY - 5, RoofZ),
        FIntVector(MidX + 2, MidY - 5, RoofZ + 7),
        EDeadbrickVoxelMaterial::Metal);
}

void AProceduralCityGenerator::SpawnReferenceClutter(
    const FIntVector& Min,
    const FIntVector& Max,
    int32 FloorHeightVoxels,
    FRandomStream& Stream)
{
    if (!GetWorld() || !VoxelWorld) return;

    const int32 MidX = (Min.X + Max.X) / 2;
    const int32 MidY = (Min.Y + Max.Y) / 2;

    if (UStaticMesh* Furniture = PickReferenceMesh(ReferenceFurnitureMeshes, Stream))
    {
        SpawnReferenceProp(
            Furniture,
            VoxelWorld->VoxelToWorld(FIntVector(MidX + 4, MidY + 4, Min.Z + 3)),
            FRotator(0.0f, Stream.FRandRange(-180.0f, 180.0f), 0.0f),
            FVector(95.0f, 85.0f, 100.0f),
            EDeadbrickVoxelMaterial::Wood,
            65.0f,
            EDeadbrickReferencePropRole::Generic);

        if (Max.Z >= FloorHeightVoxels * 2 && Stream.FRand() < 0.65f)
        {
            SpawnReferenceProp(
                Furniture,
                VoxelWorld->VoxelToWorld(FIntVector(MidX - 5, MidY + 3, Min.Z + FloorHeightVoxels + 3)),
                FRotator(0.0f, Stream.FRandRange(-180.0f, 180.0f), 0.0f),
                FVector(90.0f, 80.0f, 95.0f),
                EDeadbrickVoxelMaterial::Wood,
                65.0f,
                EDeadbrickReferencePropRole::Generic);
        }
    }

    if (UStaticMesh* Utility = PickReferenceMesh(ReferenceUtilityMeshes, Stream))
    {
        SpawnReferenceProp(
            Utility,
            VoxelWorld->VoxelToWorld(FIntVector(Max.X + 3, MidY - 5, Min.Z + 3)),
            FRotator(0.0f, Stream.FRandRange(-35.0f, 35.0f), 0.0f),
            FVector(90.0f, 90.0f, 120.0f),
            EDeadbrickVoxelMaterial::Metal,
            95.0f,
            EDeadbrickReferencePropRole::Generic);
    }
}

void AProceduralCityGenerator::SpawnReferenceProp(
    UStaticMesh* Mesh,
    const FVector& WorldLocation,
    const FRotator& Rotation,
    const FVector& TargetDimensionsCm,
    EDeadbrickVoxelMaterial BreakMaterial,
    float Health,
    EDeadbrickReferencePropRole PropRole)
{
    if (!Mesh || !GetWorld() || !VoxelWorld) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AReferenceDestructibleProp* Prop = GetWorld()->SpawnActor<AReferenceDestructibleProp>(
        AReferenceDestructibleProp::StaticClass(), WorldLocation, Rotation, SpawnParams);
    if (Prop)
    {
        Prop->InitializeFromReference(Mesh, VoxelWorld, BreakMaterial, TargetDimensionsCm, Health, PropRole);
    }
}
