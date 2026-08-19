#include "Items/DeadbrickPickupItem.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Reference/ReferenceAssetResolver.h"
#include "UObject/ConstructorHelpers.h"

ADeadbrickPickupItem::ADeadbrickPickupItem()
{
    PrimaryActorTick.bCanEverTick = false;

    PhysicsBody = CreateDefaultSubobject<USphereComponent>(TEXT("PickupPhysics"));
    SetRootComponent(PhysicsBody);
    PhysicsBody->InitSphereRadius(12.0f);
    PhysicsBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PhysicsBody->SetCollisionObjectType(ECC_PhysicsBody);
    PhysicsBody->SetCollisionResponseToAllChannels(ECR_Block);
    PhysicsBody->SetSimulatePhysics(true);
    PhysicsBody->SetEnableGravity(true);

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
    MeshComponent->SetupAttachment(PhysicsBody);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetRelativeScale3D(FVector(0.12f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded()) MeshComponent->SetStaticMesh(CubeMesh.Object);
}

void ADeadbrickPickupItem::InitializeFromVoxelMaterial(EDeadbrickVoxelMaterial Material, int32 InQuantity)
{
    Quantity = FMath::Max(1, InQuantity);
    TArray<FString> VisualKeywords;

    switch (Material)
    {
        case EDeadbrickVoxelMaterial::Wood:
            ItemType = EDeadbrickItemType::WoodScrap;
            VisualKeywords = {TEXT("wood"), TEXT("log"), TEXT("plank")};
            break;
        case EDeadbrickVoxelMaterial::Brick:
            ItemType = EDeadbrickItemType::BrickFragment;
            VisualKeywords = {TEXT("brick"), TEXT("stone"), TEXT("rock")};
            break;
        case EDeadbrickVoxelMaterial::Concrete:
            ItemType = EDeadbrickItemType::ConcreteRubble;
            VisualKeywords = {TEXT("rock"), TEXT("stone"), TEXT("rubble")};
            break;
        case EDeadbrickVoxelMaterial::Glass:
            ItemType = EDeadbrickItemType::GlassShard;
            VisualKeywords = {TEXT("glass"), TEXT("crystal"), TEXT("shard")};
            break;
        case EDeadbrickVoxelMaterial::Metal:
            ItemType = EDeadbrickItemType::MetalScrap;
            VisualKeywords = {TEXT("metal"), TEXT("ore"), TEXT("ingot")};
            break;
        case EDeadbrickVoxelMaterial::Asphalt:
            ItemType = EDeadbrickItemType::AsphaltChunk;
            VisualKeywords = {TEXT("stone"), TEXT("rock")};
            break;
        case EDeadbrickVoxelMaterial::Soil:
            ItemType = EDeadbrickItemType::SoilClump;
            VisualKeywords = {TEXT("dirt"), TEXT("soil"), TEXT("earth")};
            break;
        default:
            ItemType = EDeadbrickItemType::ConcreteRubble;
            break;
    }

    if (VisualKeywords.Num() > 0)
    {
        if (UStaticMesh* ReferenceMesh = DeadbrickReferenceAssets::FindStaticMesh(VisualKeywords))
        {
            MeshComponent->SetStaticMesh(ReferenceMesh);
        }
    }

    const float DesiredSizeCm = FMath::Clamp(10.0f + FMath::Sqrt((float)Quantity) * 3.0f, 12.0f, 34.0f);
    if (UStaticMesh* CurrentMesh = MeshComponent->GetStaticMesh())
    {
        const FVector SourceSize = CurrentMesh->GetBounds().BoxExtent * 2.0f;
        const float Longest = FMath::Max3(SourceSize.X, SourceSize.Y, SourceSize.Z);
        const float UniformScale = Longest > 1.0f ? DesiredSizeCm / Longest : 0.12f;
        MeshComponent->SetRelativeScale3D(FVector(UniformScale));
    }

    PhysicsBody->SetSphereRadius(FMath::Max(6.0f, DesiredSizeCm * 0.42f), true);
    PhysicsBody->AddImpulse(FVector(
        FMath::FRandRange(-60.0f, 60.0f),
        FMath::FRandRange(-60.0f, 60.0f),
        FMath::FRandRange(80.0f, 180.0f)), NAME_None, true);
}

int32 ADeadbrickPickupItem::Collect(EDeadbrickItemType& OutType)
{
    OutType = ItemType;
    const int32 Result = Quantity;
    Quantity = 0;
    Destroy();
    return Result;
}
