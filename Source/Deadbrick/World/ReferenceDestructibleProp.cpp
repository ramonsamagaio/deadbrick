#include "World/ReferenceDestructibleProp.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "World/DestructibleVoxelWorld.h"

AReferenceDestructibleProp::AReferenceDestructibleProp()
{
    PrimaryActorTick.bCanEverTick = false;
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReferenceMesh"));
    SetRootComponent(MeshComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
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

    if (!InMesh)
    {
        return;
    }

    MeshComponent->SetStaticMesh(InMesh);
    const FVector SourceSize = InMesh->GetBounds().BoxExtent * 2.0f;
    FVector Scale(1.0f);
    Scale.X = SourceSize.X > 1.0f ? TargetDimensionsCm.X / SourceSize.X : 1.0f;
    Scale.Y = SourceSize.Y > 1.0f ? TargetDimensionsCm.Y / SourceSize.Y : 1.0f;
    Scale.Z = SourceSize.Z > 1.0f ? TargetDimensionsCm.Z / SourceSize.Z : 1.0f;
    SetActorScale3D(Scale);
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
    if (!VoxelWorld || !MeshComponent || !MeshComponent->GetStaticMesh())
    {
        return;
    }

    const FBox Bounds = MeshComponent->Bounds.GetBox();
    FIntVector MinVoxel = VoxelWorld->WorldToVoxel(Bounds.Min);
    FIntVector MaxVoxel = VoxelWorld->WorldToVoxel(Bounds.Max);

    // Keep conversion bounded so a badly scaled reference mesh cannot flood a whole chunk set.
    const int32 MaxAxis = 18;
    MaxVoxel.X = FMath::Min(MaxVoxel.X, MinVoxel.X + MaxAxis);
    MaxVoxel.Y = FMath::Min(MaxVoxel.Y, MinVoxel.Y + MaxAxis);
    MaxVoxel.Z = FMath::Min(MaxVoxel.Z, MinVoxel.Z + MaxAxis);

    VoxelWorld->FillBox(MinVoxel, MaxVoxel, BreakMaterial);
    VoxelWorld->ApplySphereDamage(Bounds.GetCenter(), FMath::Max(35.0f, VoxelWorld->VoxelSizeCm * 2.5f), 320.0f);
}
