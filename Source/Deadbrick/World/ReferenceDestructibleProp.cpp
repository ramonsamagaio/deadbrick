#include "World/ReferenceDestructibleProp.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Items/DeadbrickPickupItem.h"
#include "Player/DeadbrickCharacter.h"
#include "World/DestructibleVoxelWorld.h"

AReferenceDestructibleProp::AReferenceDestructibleProp()
{
    PrimaryActorTick.bCanEverTick = false;

    CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("GameplayCollision"));
    SetRootComponent(CollisionComponent);
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
    CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReferenceMesh"));
    MeshComponent->SetupAttachment(CollisionComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetMobility(EComponentMobility::Movable);
}

void AReferenceDestructibleProp::InitializeFromReference(
    UStaticMesh* InMesh,
    ADestructibleVoxelWorld* InVoxelWorld,
    EDeadbrickVoxelMaterial InBreakMaterial,
    const FVector& TargetDimensionsCm,
    float InHealth,
    EDeadbrickReferencePropRole InRole)
{
    VoxelWorld = InVoxelWorld;
    BreakMaterial = InBreakMaterial;
    Health = FMath::Max(1.0f, InHealth);
    Role = InRole;
    ClosedRotation = GetActorRotation();

    const FVector SafeDimensions(
        FMath::Max(4.0f, TargetDimensionsCm.X),
        FMath::Max(4.0f, TargetDimensionsCm.Y),
        FMath::Max(4.0f, TargetDimensionsCm.Z));
    CollisionComponent->SetBoxExtent(SafeDimensions * 0.5f);

    if (!InMesh) return;

    MeshComponent->SetStaticMesh(InMesh);
    const FVector SourceSize = InMesh->GetBounds().BoxExtent * 2.0f;
    FVector Scale(1.0f);
    Scale.X = SourceSize.X > 1.0f ? SafeDimensions.X / SourceSize.X : 1.0f;
    Scale.Y = SourceSize.Y > 1.0f ? SafeDimensions.Y / SourceSize.Y : 1.0f;
    Scale.Z = SourceSize.Z > 1.0f ? SafeDimensions.Z / SourceSize.Z : 1.0f;
    MeshComponent->SetRelativeScale3D(Scale);
}

bool AReferenceDestructibleProp::Interact(ADeadbrickCharacter* Player)
{
    if (!Player) return false;

    if (Role == EDeadbrickReferencePropRole::Door)
    {
        bOpen = !bOpen;
        const FRotator Target = bOpen
            ? ClosedRotation + FRotator(0.0f, 90.0f, 0.0f)
            : ClosedRotation;
        SetActorRotation(Target);
        return true;
    }

    if (Role == EDeadbrickReferencePropRole::Container)
    {
        if (!bLooted)
        {
            SpawnContainerLoot();
            bLooted = true;
        }
        return true;
    }

    return false;
}

float AReferenceDestructibleProp::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    Health -= Applied;
    if (Health <= 0.0f)
    {
        if (Role == EDeadbrickReferencePropRole::Container && !bLooted)
        {
            SpawnContainerLoot();
            bLooted = true;
        }
        BreakIntoVoxels();
        Destroy();
    }
    return Applied;
}

void AReferenceDestructibleProp::SpawnContainerLoot()
{
    if (!GetWorld()) return;

    static const EDeadbrickItemType CommonLoot[] =
    {
        EDeadbrickItemType::CannedFood,
        EDeadbrickItemType::WaterBottle,
        EDeadbrickItemType::Cloth,
        EDeadbrickItemType::Plastic,
        EDeadbrickItemType::MetalScrap,
        EDeadbrickItemType::Wire,
        EDeadbrickItemType::Nails,
        EDeadbrickItemType::Ammo9mm,
        EDeadbrickItemType::RifleAmmo,
        EDeadbrickItemType::ShotgunShells,
        EDeadbrickItemType::MedicalSupplies,
        EDeadbrickItemType::Battery,
        EDeadbrickItemType::Electronics,
        EDeadbrickItemType::MechanicalParts
    };

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const int32 Drops = FMath::RandRange(2, 5);
    for (int32 Index = 0; Index < Drops; ++Index)
    {
        const EDeadbrickItemType Type = CommonLoot[FMath::RandRange(0, UE_ARRAY_COUNT(CommonLoot) - 1)];
        int32 Quantity = 1;
        if (Type == EDeadbrickItemType::Ammo9mm || Type == EDeadbrickItemType::RifleAmmo) Quantity = FMath::RandRange(3, 12);
        else if (Type == EDeadbrickItemType::ShotgunShells) Quantity = FMath::RandRange(2, 6);
        else if (Type == EDeadbrickItemType::Nails || Type == EDeadbrickItemType::Wire) Quantity = FMath::RandRange(1, 5);

        const FVector Offset(
            FMath::FRandRange(-35.0f, 35.0f),
            FMath::FRandRange(-35.0f, 35.0f),
            FMath::FRandRange(30.0f, 70.0f));

        if (ADeadbrickPickupItem* Pickup = GetWorld()->SpawnActor<ADeadbrickPickupItem>(
            ADeadbrickPickupItem::StaticClass(), GetActorLocation() + Offset, FRotator::ZeroRotator, SpawnParams))
        {
            Pickup->InitializeItem(Type, Quantity);
        }
    }
}

void AReferenceDestructibleProp::BreakIntoVoxels()
{
    if (!VoxelWorld || !CollisionComponent) return;

    const FBox Bounds = CollisionComponent->Bounds.GetBox();
    FIntVector MinVoxel = VoxelWorld->WorldToVoxel(Bounds.Min);
    FIntVector MaxVoxel = VoxelWorld->WorldToVoxel(Bounds.Max);

    const int32 MaxAxis = 18;
    MaxVoxel.X = FMath::Min(MaxVoxel.X, MinVoxel.X + MaxAxis);
    MaxVoxel.Y = FMath::Min(MaxVoxel.Y, MinVoxel.Y + MaxAxis);
    MaxVoxel.Z = FMath::Min(MaxVoxel.Z, MinVoxel.Z + MaxAxis);

    VoxelWorld->FillBox(MinVoxel, MaxVoxel, BreakMaterial);
    VoxelWorld->ApplySphereDamage(Bounds.GetCenter(), FMath::Max(35.0f, VoxelWorld->VoxelSizeCm * 2.5f), 320.0f);
}
