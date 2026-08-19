#include "Reference/ReferenceCityDecorationSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Reference/ReferenceAssetResolver.h"
#include "TimerManager.h"
#include "World/DestructibleVoxelWorld.h"
#include "World/ReferenceDestructibleProp.h"

bool UDeadbrickReferenceCityDecorationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

void UDeadbrickReferenceCityDecorationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    InWorld.GetTimerManager().SetTimer(
        DecorationTimer,
        this,
        &UDeadbrickReferenceCityDecorationSubsystem::DecorateReferenceProps,
        0.75f,
        false);
}

void UDeadbrickReferenceCityDecorationSubsystem::DecorateReferenceProps()
{
    UWorld* World = GetWorld();
    if (!World) return;

    ADestructibleVoxelWorld* VoxelWorld = nullptr;
    for (TActorIterator<ADestructibleVoxelWorld> It(World); It; ++It)
    {
        VoxelWorld = *It;
        break;
    }
    if (!VoxelWorld) return;

    struct FReferencePool
    {
        TArray<UStaticMesh*> Meshes;
        EDeadbrickVoxelMaterial BreakMaterial = EDeadbrickVoxelMaterial::Wood;
        EDeadbrickReferencePropRole Role = EDeadbrickReferencePropRole::Generic;
        FVector SizeCm = FVector(80.0f, 80.0f, 100.0f);
        float Health = 70.0f;
    };

    TArray<FReferencePool> Pools;

    FReferencePool Containers;
    Containers.Meshes = DeadbrickReferenceAssets::FindStaticMeshes(
        {TEXT("container"), TEXT("crate"), TEXT("chest"), TEXT("box"), TEXT("barrel")}, 16);
    Containers.BreakMaterial = EDeadbrickVoxelMaterial::Metal;
    Containers.Role = EDeadbrickReferencePropRole::Container;
    Containers.SizeCm = FVector(120.0f, 90.0f, 110.0f);
    Containers.Health = 110.0f;
    if (Containers.Meshes.Num() > 0) Pools.Add(MoveTemp(Containers));

    FReferencePool Furniture;
    Furniture.Meshes = DeadbrickReferenceAssets::FindStaticMeshes(
        {TEXT("chair"), TEXT("table"), TEXT("bench"), TEXT("shelf"), TEXT("cabinet")}, 18);
    Furniture.BreakMaterial = EDeadbrickVoxelMaterial::Wood;
    Furniture.SizeCm = FVector(90.0f, 70.0f, 100.0f);
    Furniture.Health = 65.0f;
    if (Furniture.Meshes.Num() > 0) Pools.Add(MoveTemp(Furniture));

    FReferencePool Street;
    Street.Meshes = DeadbrickReferenceAssets::FindStaticMeshes(
        {TEXT("fence"), TEXT("lamp"), TEXT("post"), TEXT("sign"), TEXT("gate")}, 18);
    Street.BreakMaterial = EDeadbrickVoxelMaterial::Metal;
    Street.SizeCm = FVector(70.0f, 70.0f, 180.0f);
    Street.Health = 95.0f;
    if (Street.Meshes.Num() > 0) Pools.Add(MoveTemp(Street));

    if (Pools.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("DEADBRICK reference decorator: no loadable LOTL static meshes found."));
        return;
    }

    const FVector Origin = VoxelWorld->GetActorLocation();
    FRandomStream Stream(7331);
    int32 Spawned = 0;

    for (int32 Index = 0; Index < 22; ++Index)
    {
        const FReferencePool& Pool = Pools[Stream.RandRange(0, Pools.Num() - 1)];
        if (Pool.Meshes.Num() == 0) continue;
        UStaticMesh* Mesh = Pool.Meshes[Stream.RandRange(0, Pool.Meshes.Num() - 1)];
        if (!Mesh) continue;

        // Keep this first decorator pass on the broad outer streets of the 1-block prototype so
        // props do not spawn inside rooms while still making reference art immediately visible.
        const bool bVerticalStreet = Stream.RandRange(0, 1) == 0;
        const bool bFarSide = Stream.RandRange(0, 1) == 1;
        float LocalX = Stream.FRandRange(150.0f, 3850.0f);
        float LocalY = Stream.FRandRange(150.0f, 3850.0f);
        if (bVerticalStreet) LocalX = bFarSide ? Stream.FRandRange(3300.0f, 3850.0f) : Stream.FRandRange(150.0f, 700.0f);
        else LocalY = bFarSide ? Stream.FRandRange(3300.0f, 3850.0f) : Stream.FRandRange(150.0f, 700.0f);

        const FVector TraceStart = Origin + FVector(LocalX, LocalY, 900.0f);
        const FVector TraceEnd = Origin + FVector(LocalX, LocalY, -400.0f);
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(DeadbrickReferenceDecoration), false);
        if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params)) continue;
        if (Hit.GetActor() != VoxelWorld) continue;

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        const FRotator Rotation(0.0f, Stream.FRandRange(-180.0f, 180.0f), 0.0f);

        if (AReferenceDestructibleProp* Prop = World->SpawnActor<AReferenceDestructibleProp>(
            AReferenceDestructibleProp::StaticClass(),
            Hit.ImpactPoint + FVector(0.0f, 0.0f, Pool.SizeCm.Z * 0.5f + 2.0f),
            Rotation,
            SpawnParams))
        {
            Prop->InitializeFromReference(
                Mesh,
                VoxelWorld,
                Pool.BreakMaterial,
                Pool.SizeCm,
                Pool.Health,
                Pool.Role);
            ++Spawned;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("DEADBRICK reference decorator: spawned %d cooked LOTL props."), Spawned);
    if (Spawned > 0 && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
            FString::Printf(TEXT("LOTL GRAPHICS ACTIVE: %d reference props placed"), Spawned));
    }
}
