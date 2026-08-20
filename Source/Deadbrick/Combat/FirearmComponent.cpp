#include "Combat/FirearmComponent.h"

#include "AI/ZombieDirectorSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/DeadbrickCharacter.h"
#include "World/DestructibleVoxelWorld.h"

UFirearmComponent::UFirearmComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UFirearmComponent::BeginPlay()
{
    Super::BeginPlay();
    BuildFallbackWeaponPresentation();
}

void UFirearmComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateWeaponPresentation(DeltaTime);
}

void UFirearmComponent::BuildFallbackWeaponPresentation()
{
    if (bPresentationBuilt) return;

    ADeadbrickCharacter* Character = Cast<ADeadbrickCharacter>(GetOwner());
    if (!Character || !Character->ViewModelRoot || !Character->ViewWeapon) return;

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!Cube || !Cylinder) return;

    BaseViewModelLocation = Character->ViewModelRoot->GetRelativeLocation();
    BaseViewModelRotation = Character->ViewModelRoot->GetRelativeRotation();

    // Compact improvised rifle silhouette. Reference weapon meshes can still replace the receiver,
    // while these pieces keep the first-person view readable before imported art is available.
    Character->ViewWeapon->SetStaticMesh(Cube);
    Character->ViewWeapon->SetRelativeLocation(FVector(49.0f, 13.0f, -21.0f));
    Character->ViewWeapon->SetRelativeScale3D(FVector(0.27f, 0.055f, 0.075f));

    auto MakePart = [&](const TCHAR* Name, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FRotator& Rotation)
    {
        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Character, Name);
        if (!Part) return (UStaticMeshComponent*)nullptr;
        Character->AddInstanceComponent(Part);
        Part->SetStaticMesh(Mesh);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetOnlyOwnerSee(true);
        Part->SetCastShadow(false);
        Part->SetupAttachment(Character->ViewModelRoot);
        Part->RegisterComponent();
        Part->SetRelativeLocation(Location);
        Part->SetRelativeScale3D(Scale);
        Part->SetRelativeRotation(Rotation);
        return Part;
    };

    WeaponStock = MakePart(TEXT("FallbackWeaponStock"), Cube, FVector(29.0f, 13.0f, -21.0f), FVector(0.17f, 0.05f, 0.09f), FRotator::ZeroRotator);
    WeaponBarrel = MakePart(TEXT("FallbackWeaponBarrel"), Cylinder, FVector(76.0f, 13.0f, -20.0f), FVector(0.045f, 0.045f, 0.30f), FRotator(0.0f, 90.0f, 0.0f));
    WeaponGrip = MakePart(TEXT("FallbackWeaponGrip"), Cube, FVector(44.0f, 13.0f, -31.0f), FVector(0.045f, 0.045f, 0.12f), FRotator(-16.0f, 0.0f, 0.0f));
    WeaponMagazine = MakePart(TEXT("FallbackWeaponMagazine"), Cube, FVector(56.0f, 13.0f, -30.0f), FVector(0.05f, 0.045f, 0.13f), FRotator(12.0f, 0.0f, 0.0f));
    WeaponSight = MakePart(TEXT("FallbackWeaponSight"), Cube, FVector(51.0f, 13.0f, -13.0f), FVector(0.055f, 0.028f, 0.025f), FRotator::ZeroRotator);

    if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        auto MakeMaterial = [&](const TCHAR* Name, const FLinearColor& Color, float Roughness)
        {
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Character, Name);
            if (Material)
            {
                Material->SetVectorParameterValue(TEXT("Color"), Color);
                Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
            }
            return Material;
        };

        UMaterialInstanceDynamic* Gunmetal = MakeMaterial(TEXT("Gunmetal"), FLinearColor(0.045f, 0.052f, 0.055f), 0.48f);
        UMaterialInstanceDynamic* DarkMetal = MakeMaterial(TEXT("DarkGunMetal"), FLinearColor(0.018f, 0.022f, 0.024f), 0.38f);
        UMaterialInstanceDynamic* StockMaterial = MakeMaterial(TEXT("WeaponStockMaterial"), FLinearColor(0.16f, 0.085f, 0.035f), 0.76f);

        if (Gunmetal) Character->ViewWeapon->SetMaterial(0, Gunmetal);
        if (StockMaterial && WeaponStock) WeaponStock->SetMaterial(0, StockMaterial);
        if (DarkMetal && WeaponBarrel) WeaponBarrel->SetMaterial(0, DarkMetal);
        if (DarkMetal && WeaponMagazine) WeaponMagazine->SetMaterial(0, DarkMetal);
        if (Gunmetal && WeaponGrip) WeaponGrip->SetMaterial(0, Gunmetal);
        if (DarkMetal && WeaponSight) WeaponSight->SetMaterial(0, DarkMetal);
    }

    MuzzleFlashLight = NewObject<UPointLightComponent>(Character, TEXT("FallbackMuzzleFlash"));
    if (MuzzleFlashLight)
    {
        Character->AddInstanceComponent(MuzzleFlashLight);
        MuzzleFlashLight->SetupAttachment(Character->ViewModelRoot);
        MuzzleFlashLight->SetMobility(EComponentMobility::Movable);
        MuzzleFlashLight->SetIntensity(0.0f);
        MuzzleFlashLight->SetAttenuationRadius(260.0f);
        MuzzleFlashLight->SetLightColor(FLinearColor(1.0f, 0.40f, 0.075f));
        MuzzleFlashLight->SetCastShadows(false);
        MuzzleFlashLight->VolumetricScatteringIntensity = 2.2f;
        MuzzleFlashLight->RegisterComponent();
        MuzzleFlashLight->SetRelativeLocation(FVector(94.0f, 13.0f, -20.0f));
    }

    bPresentationBuilt = true;
}

void UFirearmComponent::PlayFirePresentation()
{
    if (!bPresentationBuilt) BuildFallbackWeaponPresentation();

    RecoilLocation += FVector(
        -FMath::FRandRange(3.2f, 4.5f),
        FMath::FRandRange(-0.35f, 0.35f),
        FMath::FRandRange(0.7f, 1.35f));
    RecoilLocation.X = FMath::Max(RecoilLocation.X, -8.5f);

    RecoilRotation.Pitch += FMath::FRandRange(-2.8f, -1.8f);
    RecoilRotation.Yaw += FMath::FRandRange(-0.45f, 0.45f);
    RecoilRotation.Roll += FMath::FRandRange(-0.55f, 0.55f);
    RecoilRotation.Pitch = FMath::Max(RecoilRotation.Pitch, -6.5f);

    MuzzleFlashAlpha = 1.0f;
}

void UFirearmComponent::UpdateWeaponPresentation(float DeltaTime)
{
    ADeadbrickCharacter* Character = Cast<ADeadbrickCharacter>(GetOwner());
    if (!Character || !Character->ViewModelRoot) return;
    if (!bPresentationBuilt) BuildFallbackWeaponPresentation();

    const float Speed = Character->GetVelocity().Size2D();
    const float MoveAlpha = FMath::Clamp(Speed / 650.0f, 0.0f, 1.0f);
    WalkBobTime += DeltaTime * FMath::Lerp(2.1f, 8.2f, MoveAlpha);

    const float SideWave = FMath::Sin(WalkBobTime);
    const float StepWave = FMath::Sin(WalkBobTime * 2.0f);
    const FVector BobLocation(
        -FMath::Abs(StepWave) * 0.28f * MoveAlpha,
        SideWave * 0.62f * MoveAlpha,
        -FMath::Abs(SideWave) * 0.55f * MoveAlpha);
    const FRotator BobRotation(
        StepWave * 0.23f * MoveAlpha,
        SideWave * 0.22f * MoveAlpha,
        SideWave * 0.75f * MoveAlpha);

    RecoilLocation = FMath::VInterpTo(RecoilLocation, FVector::ZeroVector, DeltaTime, 13.5f);
    RecoilRotation = FMath::RInterpTo(RecoilRotation, FRotator::ZeroRotator, DeltaTime, 15.5f);

    Character->ViewModelRoot->SetRelativeLocation(BaseViewModelLocation + BobLocation + RecoilLocation);
    Character->ViewModelRoot->SetRelativeRotation(BaseViewModelRotation + BobRotation + RecoilRotation);

    MuzzleFlashAlpha = FMath::FInterpTo(MuzzleFlashAlpha, 0.0f, DeltaTime, 42.0f);
    if (MuzzleFlashLight)
        MuzzleFlashLight->SetIntensity(11000.0f * MuzzleFlashAlpha);
}

bool UFirearmComponent::FireFromCamera(const FVector& Origin, const FVector& Direction)
{
    if (!GetWorld() || (!bInfiniteAmmo && AmmoInMagazine <= 0)) return false;

    const double Now = GetWorld()->GetTimeSeconds();
    const double SecondsPerShot = 60.0 / FMath::Max(1.0f, Stats.RoundsPerMinute);
    if (Now - LastShotTime < SecondsPerShot) return false;

    LastShotTime = Now;
    if (!bInfiniteAmmo) --AmmoInMagazine;
    PlayFirePresentation();

    if (UDeadbrickZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UDeadbrickZombieDirectorSubsystem>())
        Director->ReportNoise(Origin, Stats.NoiseRadiusCm, Stats.NoiseIntensity, 4.0f);

    const FVector ShotDirection = Direction.GetSafeNormal();
    const FVector End = Origin + ShotDirection * Stats.RangeCm;

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DeadbrickFirearm), true, GetOwner());
    Params.bReturnPhysicalMaterial = true;

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

    if (!GetWorld()->LineTraceSingleByObjectType(Hit, Origin, End, ObjectParams, Params)) return true;

    if (ADestructibleVoxelWorld* VoxelWorld = Cast<ADestructibleVoxelWorld>(Hit.GetActor()))
    {
        const FVector DamagePoint = Hit.ImpactPoint - Hit.ImpactNormal * 2.0f;
        VoxelWorld->ApplySphereDamage(DamagePoint, Stats.VoxelDamageRadiusCm, Stats.VoxelDamage);
    }
    else if (AActor* HitActor = Hit.GetActor())
    {
        UGameplayStatics::ApplyPointDamage(
            HitActor,
            Stats.Damage,
            ShotDirection,
            Hit,
            GetOwner() ? GetOwner()->GetInstigatorController() : nullptr,
            GetOwner(),
            nullptr);

        UE_LOG(LogTemp, Verbose, TEXT("DEADBRICK firearm hit %s at %s"), *GetNameSafe(HitActor), *Hit.ImpactPoint.ToCompactString());
    }

    return true;
}

void UFirearmComponent::Reload()
{
    if (bInfiniteAmmo)
    {
        AmmoInMagazine = Stats.MagazineSize;
        return;
    }

    const int32 Missing = FMath::Max(0, Stats.MagazineSize - AmmoInMagazine);
    const int32 ToLoad = FMath::Min(Missing, ReserveAmmo);
    AmmoInMagazine += ToLoad;
    ReserveAmmo -= ToLoad;
}
