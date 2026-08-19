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

    for (UStaticMesh* Mesh : DeadbrickReferenceAssets::FindStaticMeshes({TEXT("door"), TEXT("gate"), TEXT("hatch")}, 12))
        if (Mesh) ReferenceDoorMeshes.Add(Mesh);
    for (UStaticMesh* Mesh : DeadbrickReferenceAssets::FindStaticMeshes({TEXT("window"), TEXT("glass")}, 12))
        if (Mesh) ReferenceWindowMeshes.Add(Mesh);
    for (UStaticMesh* Mesh : DeadbrickReferenceAssets::FindStaticMeshes({TEXT("container"), TEXT("crate"), TEXT("locker"), TEXT("chest"), TEXT("box")}, 16))
        if (Mesh) ReferenceContainerMeshes.Add(Mesh);

    UE_LOG(LogTemp, Display, TEXT("DEADBRICK reference prop pools: %d doors, %d windows, %d containers"),
        ReferenceDoorMeshes.Num(), ReferenceWindowMeshes.Num(), ReferenceContainerMeshes.Num());
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

        BuildShell(FIntVector(X0, Y0, 0), FIntVector(X1, Y1, Z1), FloorV, WallMaterial, Stream);

        if (ReferenceContainerMeshes.Num() > 0 && Stream.FRand() < 0.75f)
        {
            UStaticMesh* ContainerMesh = PickReferenceMesh(ReferenceContainerMeshes, Stream);
            const FVector ContainerLocation = VoxelWorld->VoxelToWorld(FIntVector(X0 + 3, FMath::Max(1, Y0 - 5), 3));
            SpawnReferenceProp(
                ContainerMesh,
                ContainerLocation,
                FRotator(0.0f, Stream.FRandRange(-20.0f, 20.0f), 0.0f),
                FVector(180.0f, 100.0f, 110.0f),
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

void AProceduralCityGenerator::SpawnReferenceProp(
    UStaticMesh* Mesh,
    const FVector& WorldLocation,
    const FRotator& Rotation,
    const FVector& TargetDimensionsCm,
    EDeadbrickVoxelMaterial BreakMaterial,
    float Health,
    EDeadbrickReferencePropRole Role)
{
    if (!Mesh || !GetWorld() || !VoxelWorld) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AReferenceDestructibleProp* Prop = GetWorld()->SpawnActor<AReferenceDestructibleProp>(
        AReferenceDestructibleProp::StaticClass(), WorldLocation, Rotation, SpawnParams);
    if (Prop)
    {
        Prop->InitializeFromReference(Mesh, VoxelWorld, BreakMaterial, TargetDimensionsCm, Health, Role);
    }
}
