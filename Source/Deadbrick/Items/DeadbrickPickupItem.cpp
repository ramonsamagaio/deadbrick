#include "Items/DeadbrickPickupItem.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Reference/ReferenceAssetResolver.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    UStaticMesh* BasicCube()
    {
        static TWeakObjectPtr<UStaticMesh> Cached;
        if (!Cached.IsValid()) Cached = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        return Cached.Get();
    }

    UStaticMesh* BasicCylinder()
    {
        static TWeakObjectPtr<UStaticMesh> Cached;
        if (!Cached.IsValid()) Cached = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
        return Cached.Get();
    }

    UStaticMesh* BasicSphere()
    {
        static TWeakObjectPtr<UStaticMesh> Cached;
        if (!Cached.IsValid()) Cached = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        return Cached.Get();
    }

    UStaticMesh* BasicCone()
    {
        static TWeakObjectPtr<UStaticMesh> Cached;
        if (!Cached.IsValid()) Cached = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
        return Cached.Get();
    }
}

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
    PhysicsBody->SetLinearDamping(0.35f);
    PhysicsBody->SetAngularDamping(0.45f);

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
    MeshComponent->SetupAttachment(PhysicsBody);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    DetailMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupDetail"));
    DetailMeshComponent->SetupAttachment(PhysicsBody);
    DetailMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    AccentMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupAccent"));
    AccentMeshComponent->SetupAttachment(PhysicsBody);
    AccentMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded()) MeshComponent->SetStaticMesh(CubeMesh.Object);
    DetailMeshComponent->SetVisibility(false, true);
    AccentMeshComponent->SetVisibility(false, true);
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
    if (bUsingReferenceVisual)
    {
        if (UStaticMesh* CurrentMesh = MeshComponent->GetStaticMesh())
        {
            const FVector SourceSize = CurrentMesh->GetBounds().BoxExtent * 2.0f;
            const float Longest = FMath::Max3(SourceSize.X, SourceSize.Y, SourceSize.Z);
            const float UniformScale = Longest > 1.0f ? DesiredSizeCm / Longest : 0.12f;
            MeshComponent->SetRelativeScale3D(FVector(UniformScale));
        }
    }
    else
    {
        ConfigureFallbackVisual(DesiredSizeCm);
    }

    PhysicsBody->SetSphereRadius(FMath::Max(6.0f, DesiredSizeCm * 0.48f), true);
    PhysicsBody->AddImpulse(FVector(
        FMath::FRandRange(-55.0f, 55.0f),
        FMath::FRandRange(-55.0f, 55.0f),
        FMath::FRandRange(70.0f, 150.0f)), NAME_None, true);
}

void ADeadbrickPickupItem::ApplyVisualForItemType()
{
    bUsingReferenceVisual = false;
    DetailMeshComponent->SetVisibility(false, true);
    AccentMeshComponent->SetVisibility(false, true);
    MeshComponent->SetRelativeLocation(FVector::ZeroVector);
    MeshComponent->SetRelativeRotation(FRotator::ZeroRotator);

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
        case EDeadbrickItemType::Electronics: Keywords = {TEXT("component"), TEXT("device"), TEXT("electronics")}; break;
        case EDeadbrickItemType::Plastic: Keywords = {TEXT("container"), TEXT("bottle"), TEXT("plastic")}; break;
        case EDeadbrickItemType::Wire: Keywords = {TEXT("wire"), TEXT("rope"), TEXT("cable")}; break;
        case EDeadbrickItemType::Nails: Keywords = {TEXT("nail"), TEXT("metal")}; break;
        case EDeadbrickItemType::Ammo9mm: Keywords = {TEXT("ammo"), TEXT("bullet"), TEXT("cartridge")}; break;
        case EDeadbrickItemType::RifleAmmo: Keywords = {TEXT("ammo"), TEXT("bullet"), TEXT("cartridge")}; break;
        case EDeadbrickItemType::ShotgunShells: Keywords = {TEXT("ammo"), TEXT("shell"), TEXT("bullet")}; break;
        case EDeadbrickItemType::CannedFood: Keywords = {TEXT("food"), TEXT("can"), TEXT("container")}; break;
        case EDeadbrickItemType::WaterBottle: Keywords = {TEXT("bottle"), TEXT("water"), TEXT("flask")}; break;
        case EDeadbrickItemType::PurifiedWater: Keywords = {TEXT("bottle"), TEXT("water"), TEXT("flask")}; break;
        case EDeadbrickItemType::MedicalSupplies: Keywords = {TEXT("medical"), TEXT("medkit"), TEXT("bandage")}; break;
        case EDeadbrickItemType::Bandage: Keywords = {TEXT("cloth"), TEXT("bandage")}; break;
        case EDeadbrickItemType::Battery: Keywords = {TEXT("battery"), TEXT("component")}; break;
        case EDeadbrickItemType::Fuel: Keywords = {TEXT("fuel"), TEXT("oil"), TEXT("canister")}; break;
        case EDeadbrickItemType::MechanicalParts: Keywords = {TEXT("gear"), TEXT("component"), TEXT("mechanical")}; break;
        case EDeadbrickItemType::MetalPlate: Keywords = {TEXT("metal"), TEXT("plate"), TEXT("sheet")}; break;
        case EDeadbrickItemType::RepairKit: Keywords = {TEXT("tool"), TEXT("kit"), TEXT("toolbox")}; break;
        case EDeadbrickItemType::Molotov: Keywords = {TEXT("bottle"), TEXT("flask")}; break;
        case EDeadbrickItemType::WoodenBarricade: Keywords = {TEXT("wood"), TEXT("plank"), TEXT("barricade")}; break;
        default: break;
    }

    if (Keywords.Num() > 0)
    {
        if (UStaticMesh* ReferenceMesh = DeadbrickReferenceAssets::FindStaticMesh(Keywords))
        {
            MeshComponent->SetStaticMesh(ReferenceMesh);
            bUsingReferenceVisual = true;
            return;
        }
    }

    MeshComponent->SetStaticMesh(BasicCube());
}

void ADeadbrickPickupItem::ConfigureFallbackVisual(float DesiredSizeCm)
{
    const float S = DesiredSizeCm / 100.0f;

    auto SetPart = [](UStaticMeshComponent* Part, UStaticMesh* Mesh, const FVector& Scale, const FVector& Location, const FRotator& Rotation)
    {
        if (!Part) return;
        Part->SetStaticMesh(Mesh);
        Part->SetRelativeScale3D(Scale);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeRotation(Rotation);
        Part->SetVisibility(Mesh != nullptr, true);
    };

    SetPart(DetailMeshComponent, nullptr, FVector::OneVector, FVector::ZeroVector, FRotator::ZeroRotator);
    SetPart(AccentMeshComponent, nullptr, FVector::OneVector, FVector::ZeroVector, FRotator::ZeroRotator);

    const float Jitter = FMath::FRandRange(-12.0f, 12.0f);

    switch (ItemType)
    {
        case EDeadbrickItemType::WoodScrap:
        case EDeadbrickItemType::WoodenBarricade:
            SetPart(MeshComponent, BasicCube(), FVector(S * 1.25f, S * 0.24f, S * 0.18f), FVector(0,0,0), FRotator(Jitter, 12.0f, 5.0f));
            SetPart(DetailMeshComponent, BasicCube(), FVector(S * 0.95f, S * 0.18f, S * 0.14f), FVector(0, 5, 5), FRotator(-8.0f, -18.0f, 15.0f));
            break;

        case EDeadbrickItemType::BrickFragment:
        case EDeadbrickItemType::ConcreteRubble:
        case EDeadbrickItemType::AsphaltChunk:
        case EDeadbrickItemType::SoilClump:
            SetPart(MeshComponent, BasicCube(), FVector(S * 0.72f, S * 0.58f, S * 0.46f), FVector::ZeroVector, FRotator(13.0f + Jitter, 27.0f, 8.0f));
            SetPart(DetailMeshComponent, BasicCube(), FVector(S * 0.38f, S * 0.32f, S * 0.27f), FVector(5, -4, 6), FRotator(-22.0f, 11.0f, 34.0f));
            break;

        case EDeadbrickItemType::GlassShard:
            SetPart(MeshComponent, BasicCone(), FVector(S * 0.34f, S * 0.13f, S * 0.85f), FVector::ZeroVector, FRotator(72.0f, 12.0f, 18.0f));
            SetPart(DetailMeshComponent, BasicCone(), FVector(S * 0.20f, S * 0.08f, S * 0.55f), FVector(3, 3, 1), FRotator(44.0f, -28.0f, 11.0f));
            break;

        case EDeadbrickItemType::MetalScrap:
        case EDeadbrickItemType::MetalPlate:
            SetPart(MeshComponent, BasicCube(), FVector(S * 0.85f, S * 0.62f, S * 0.08f), FVector::ZeroVector, FRotator(8.0f, 22.0f, Jitter));
            SetPart(DetailMeshComponent, BasicCylinder(), FVector(S * 0.13f, S * 0.13f, S * 0.45f), FVector(4, -2, 4), FRotator(0, 90.0f, 0));
            break;

        case EDeadbrickItemType::Cloth:
        case EDeadbrickItemType::Bandage:
            SetPart(MeshComponent, BasicCube(), FVector(S * 0.75f, S * 0.48f, S * 0.12f), FVector::ZeroVector, FRotator(4.0f, Jitter, 8.0f));
            SetPart(DetailMeshComponent, BasicCube(), FVector(S * 0.46f, S * 0.20f, S * 0.08f), FVector(2, 0, 5), FRotator(0, -12.0f, 0));
            break;

        case EDeadbrickItemType::Wire:
        case EDeadbrickItemType::Nails:
            SetPart(MeshComponent, BasicCylinder(), FVector(S * 0.10f, S * 0.10f, S * 0.85f), FVector::ZeroVector, FRotator(0, 90.0f, 18.0f));
            SetPart(DetailMeshComponent, BasicCylinder(), FVector(S * 0.08f, S * 0.08f, S * 0.68f), FVector(0, 3, 3), FRotator(74.0f, 12.0f, 0));
            break;

        case EDeadbrickItemType::Ammo9mm:
        case EDeadbrickItemType::RifleAmmo:
        case EDeadbrickItemType::ShotgunShells:
            SetPart(MeshComponent, BasicCylinder(), FVector(S * 0.18f, S * 0.18f, S * 0.62f), FVector(-3, 0, 0), FRotator(0, 35.0f, 90.0f));
            SetPart(DetailMeshComponent, BasicCylinder(), FVector(S * 0.17f, S * 0.17f, S * 0.58f), FVector(3, 2, 1), FRotator(0, 48.0f, 90.0f));
            SetPart(AccentMeshComponent, BasicCylinder(), FVector(S * 0.15f, S * 0.15f, S * 0.52f), FVector(1, -3, 2), FRotator(0, 22.0f, 90.0f));
            break;

        case EDeadbrickItemType::CannedFood:
            SetPart(MeshComponent, BasicCylinder(), FVector(S * 0.43f, S * 0.43f, S * 0.58f), FVector::ZeroVector, FRotator(Jitter, 0, 0));
            SetPart(DetailMeshComponent, BasicCylinder(), FVector(S * 0.46f, S * 0.46f, S * 0.035f), FVector(0,0,DesiredSizeCm * 0.30f), FRotator::ZeroRotator);
            break;

        case EDeadbrickItemType::WaterBottle:
        case EDeadbrickItemType::PurifiedWater:
        case EDeadbrickItemType::Molotov:
            SetPart(MeshComponent, BasicCylinder(), FVector(S * 0.31f, S * 0.31f, S * 0.82f), FVector::ZeroVector, FRotator(Jitter, 0, 0));
            SetPart(DetailMeshComponent, BasicCylinder(), FVector(S * 0.15f, S * 0.15f, S * 0.22f), FVector(0,0,DesiredSizeCm * 0.50f), FRotator::ZeroRotator);
            if (ItemType == EDeadbrickItemType::Molotov)
                SetPart(AccentMeshComponent, BasicCube(), FVector(S * 0.08f, S * 0.08f, S * 0.42f), FVector(0,0,DesiredSizeCm * 0.68f), FRotator(0, 12.0f, 8.0f));
            break;

        case EDeadbrickItemType::Fuel:
            SetPart(MeshComponent, BasicCube(), FVector(S * 0.55f, S * 0.32f, S * 0.72f), FVector::ZeroVector, FRotator(0, Jitter, 0));
            SetPart(DetailMeshComponent, BasicCylinder(), FVector(S * 0.13f, S * 0.13f, S * 0.18f), FVector(0,0,DesiredSizeCm * 0.45f), FRotator::ZeroRotator);
            break;

        case EDeadbrickItemType::MedicalSupplies:
        case EDeadbrickItemType::RepairKit:
            SetPart(MeshComponent, BasicCube(), FVector(S * 0.72f, S * 0.52f, S * 0.42f), FVector::ZeroVector, FRotator(0, Jitter, 0));
            SetPart(DetailMeshComponent, BasicCube(), FVector(S * 0.10f, S * 0.42f, S * 0.06f), FVector(0,-DesiredSizeCm * 0.27f,DesiredSizeCm * 0.15f), FRotator::ZeroRotator);
            SetPart(AccentMeshComponent, BasicCube(), FVector(S * 0.32f, S * 0.08f, S * 0.06f), FVector(0,-DesiredSizeCm * 0.29f,DesiredSizeCm * 0.15f), FRotator::ZeroRotator);
            break;

        case EDeadbrickItemType::Electronics:
        case EDeadbrickItemType::Battery:
            SetPart(MeshComponent, BasicCube(), FVector(S * 0.62f, S * 0.38f, S * 0.48f), FVector::ZeroVector, FRotator(4, Jitter, 0));
            SetPart(DetailMeshComponent, BasicCylinder(), FVector(S * 0.10f, S * 0.10f, S * 0.18f), FVector(-4, -2, DesiredSizeCm * 0.30f), FRotator::ZeroRotator);
            SetPart(AccentMeshComponent, BasicCylinder(), FVector(S * 0.10f, S * 0.10f, S * 0.18f), FVector(4, -2, DesiredSizeCm * 0.30f), FRotator::ZeroRotator);
            break;

        case EDeadbrickItemType::MechanicalParts:
            SetPart(MeshComponent, BasicCylinder(), FVector(S * 0.48f, S * 0.48f, S * 0.18f), FVector::ZeroVector, FRotator(90.0f, Jitter, 0));
            SetPart(DetailMeshComponent, BasicCylinder(), FVector(S * 0.18f, S * 0.18f, S * 0.52f), FVector(2,0,0), FRotator(0, 90.0f, 0));
            break;

        case EDeadbrickItemType::Plastic:
            SetPart(MeshComponent, BasicSphere(), FVector(S * 0.48f, S * 0.34f, S * 0.52f), FVector::ZeroVector, FRotator(Jitter, 0, 0));
            break;

        default:
            SetPart(MeshComponent, BasicCube(), FVector(S * 0.55f), FVector::ZeroVector, FRotator(Jitter, 18.0f, 7.0f));
            break;
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
