#include "Items/DeadbrickPickupItem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ADeadbrickPickupItem::ADeadbrickPickupItem()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
    SetRootComponent(MeshComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetEnableGravity(true);
    MeshComponent->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.12f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded()) MeshComponent->SetStaticMesh(CubeMesh.Object);
}

void ADeadbrickPickupItem::InitializeFromVoxelMaterial(EDeadbrickVoxelMaterial Material, int32 InQuantity)
{
    Quantity = FMath::Max(1, InQuantity);
    switch (Material)
    {
        case EDeadbrickVoxelMaterial::Wood: ItemType = EDeadbrickItemType::WoodScrap; break;
        case EDeadbrickVoxelMaterial::Brick: ItemType = EDeadbrickItemType::BrickFragment; break;
        case EDeadbrickVoxelMaterial::Concrete: ItemType = EDeadbrickItemType::ConcreteRubble; break;
        case EDeadbrickVoxelMaterial::Glass: ItemType = EDeadbrickItemType::GlassShard; break;
        case EDeadbrickVoxelMaterial::Metal: ItemType = EDeadbrickItemType::MetalScrap; break;
        case EDeadbrickVoxelMaterial::Asphalt: ItemType = EDeadbrickItemType::AsphaltChunk; break;
        case EDeadbrickVoxelMaterial::Soil: ItemType = EDeadbrickItemType::SoilClump; break;
        default: ItemType = EDeadbrickItemType::ConcreteRubble; break;
    }

    if (MeshComponent)
    {
        const float Scale = FMath::Clamp(0.08f + FMath::Sqrt((float)Quantity) * 0.025f, 0.10f, 0.28f);
        MeshComponent->SetRelativeScale3D(FVector(Scale));
        MeshComponent->AddImpulse(FVector(FMath::FRandRange(-60.0f, 60.0f), FMath::FRandRange(-60.0f, 60.0f), FMath::FRandRange(80.0f, 180.0f)), NAME_None, true);
    }
}

int32 ADeadbrickPickupItem::Collect(EDeadbrickItemType& OutType)
{
    OutType = ItemType;
    const int32 Result = Quantity;
    Quantity = 0;
    Destroy();
    return Result;
}
