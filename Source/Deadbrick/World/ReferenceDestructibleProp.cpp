#include "World/ReferenceDestructibleProp.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
    float InHealth)
{
    VoxelWorld = InVoxelWorld;
    BreakMaterial = InBreakMaterial;
    Health = FMath::Max(1.0f, InHealth);

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

float AReferenceDestructibleProp::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    Health -= Applied;
    if (Health <= 0.0f)
    {
        BreakIntoVoxels();
        Destroy();
    }
    return Applied;
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

    // Reused meshes are only the visual skin. On destruction their physical volume becomes actual DEADBRICK
    // voxel matter, then receives an impulse-like damage pass that breaks it into salvage/detached pieces.
    VoxelWorld->FillBox(MinVoxel, MaxVoxel, BreakMaterial);
    VoxelWorld->ApplySphereDamage(Bounds.GetCenter(), FMath::Max(35.0f, VoxelWorld->VoxelSizeCm * 2.5f), 320.0f);
}
