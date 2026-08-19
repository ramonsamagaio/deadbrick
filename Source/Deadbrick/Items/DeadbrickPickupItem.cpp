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
    EDeadbrickItemType Type = EDeadbrickItemType::ConcreteRubble;
    switch (Material)
    {
        case EDeadbrickVoxelMaterial::Wood: Type = EDeadbrickItemType::WoodScrap; break;
        case EDeadbrickVoxelMaterial::Brick: Type = EDeadbrickItemType::BrickFragment; break;
        case EDeadbrickVoxelMaterial::Concrete: Type = EDeadbrickItemType::ConcreteRubble; break;
        case EDeadbrickVoxelMaterial::Glass: Type = EDeadbrickItemType::GlassShard; break;
        case EDeadbrickVoxelMaterial::Metal: Type = EDeadbrickItemType::MetalScrap; break;
        case EDeadbrickVoxelMaterial::Asphalt: Type = EDeadbrickItemType::AsphaltChunk; break;
        case EDeadbrickVoxelMaterial::Soil: Type = EDeadbrickItemType::SoilClump; break;
        default: break;
    }
    InitializeItem(Type, InQuantity);
}

void ADeadbrickPickupItem::InitializeItem(EDeadbrickItemType InType, int32 InQuantity)
{
    ItemType = InType;
    Quantity = FMath::Max(1, InQuantity);
    ApplyVisualForItemType();

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

void ADeadbrickPickupItem::ApplyVisualForItemType()
{
    TArray<FString> Keywords;
    switch (ItemType)
    {
        case EDeadbrickItemType::WoodScrap: Keywords = {TEXT("wood"), TEXT("log"), TEXT("plank")}; break;
        case EDeadbrickItemType::BrickFragment: Keywords = {TEXT("brick"), TEXT("stone"), TEXT("rock")}; break;
        case EDeadbrickItemType::ConcreteRubble: Keywords = {TEXT("rock"), TEXT("stone"), TEXT("rubble")}; break;
        case EDeadbrickItemType::GlassShard: Keywords = {TEXT("glass"), TEXT("crystal"), TEXT("shard")}; break;
        case EDeadbrickItemType::MetalScrap: Keywords = {TEXT("metal"), TEXT("ore"), TEXT("ingot")}; break;
        case EDeadbrickItemType::AsphaltChunk: Keywords = {TEXT("stone"), TEXT("rock")}; break;
        case EDeadbrickItemType::SoilClump: Keywords = {TEXT("dirt"), TEXT("soil"), TEXT("earth")}; break;
        case EDeadbrickItemType::Cloth: Keywords = {TEXT("cloth"), TEXT("fabric"), TEXT("fiber")}; break;
        case EDeadbrickItemType::Electronics: Keywords = {TEXT("component"), TEXT("device"), TEXT("crystal")}; break;
        case EDeadbrickItemType::Plastic: Keywords = {TEXT("container"), TEXT("bottle")}; break;
        case EDeadbrickItemType::Wire: Keywords = {TEXT("wire"), TEXT("rope"), TEXT("cable")}; break;
        case EDeadbrickItemType::Nails: Keywords = {TEXT("nail"), TEXT("metal")}; break;
        case EDeadbrickItemType::Ammo9mm: Keywords = {TEXT("ammo"), TEXT("bullet"), TEXT("arrow")}; break;
        case EDeadbrickItemType::RifleAmmo: Keywords = {TEXT("ammo"), TEXT("bullet"), TEXT("arrow")}; break;
        case EDeadbrickItemType::ShotgunShells: Keywords = {TEXT("ammo"), TEXT("bullet")}; break;
        case EDeadbrickItemType::CannedFood: Keywords = {TEXT("food"), TEXT("meal"), TEXT("container")}; break;
        case EDeadbrickItemType::WaterBottle: Keywords = {TEXT("bottle"), TEXT("water"), TEXT("flask")}; break;
        case EDeadbrickItemType::PurifiedWater: Keywords = {TEXT("bottle"), TEXT("water"), TEXT("flask")}; break;
        case EDeadbrickItemType::MedicalSupplies: Keywords = {TEXT("potion"), TEXT("medical"), TEXT("bandage")}; break;
        case EDeadbrickItemType::Bandage: Keywords = {TEXT("cloth"), TEXT("bandage")}; break;
        case EDeadbrickItemType::Battery: Keywords = {TEXT("battery"), TEXT("crystal"), TEXT("component")}; break;
        case EDeadbrickItemType::Fuel: Keywords = {TEXT("fuel"), TEXT("oil"), TEXT("bottle")}; break;
        case EDeadbrickItemType::MechanicalParts: Keywords = {TEXT("gear"), TEXT("component"), TEXT("metal")}; break;
        case EDeadbrickItemType::MetalPlate: Keywords = {TEXT("metal"), TEXT("plate"), TEXT("ingot")}; break;
        case EDeadbrickItemType::RepairKit: Keywords = {TEXT("tool"), TEXT("kit"), TEXT("box")}; break;
        case EDeadbrickItemType::Molotov: Keywords = {TEXT("bottle"), TEXT("flask")}; break;
        case EDeadbrickItemType::WoodenBarricade: Keywords = {TEXT("wood"), TEXT("plank"), TEXT("wall")}; break;
        default: break;
    }

    if (Keywords.Num() == 0) return;
    if (UStaticMesh* ReferenceMesh = DeadbrickReferenceAssets::FindStaticMesh(Keywords))
    {
        MeshComponent->SetStaticMesh(ReferenceMesh);
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
