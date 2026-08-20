#include "Combat/FirearmComponent.h"

#include "AI/ZombieDirectorSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DeadbrickCharacter.h"
#include "TimerManager.h"
#include "World/DestructibleVoxelWorld.h"

UFirearmComponent::UFirearmComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UFirearmComponent::BeginPlay()
{
    Super::BeginPlay();
    BuildFallbackWeaponPresentation();
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

    // Re-purpose the old single cuboid as the receiver, then layer enough simple shapes around it to
    // read as an actual compact rifle even when the reference weapon mesh is unavailable.
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
        Part->RegisterComponent();
        Part->AttachToComponent(Character->ViewModelRoot, FAttachmentTransformRules::KeepRelativeTransform);
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

    MuzzleFlashLight = NewObject<UPointLightComponent>(Character, TEXT("FallbackMuzzleFlash"));
    if (MuzzleFlashLight)
    {
        Character->AddInstanceComponent(MuzzleFlashLight);
        MuzzleFlashLight->SetIntensity(0.0f);
        MuzzleFlashLight->SetAttenuationRadius(180.0f);
        MuzzleFlashLight->SetLightColor(FLinearColor(1.0f, 0.48f, 0.12f));
        MuzzleFlashLight->RegisterComponent();
        MuzzleFlashLight->AttachToComponent(Character->ViewModelRoot, FAttachmentTransformRules::KeepRelativeTransform);
        MuzzleFlashLight->SetRelativeLocation(FVector(93.0f, 13.0f, -20.0f));
    }

    bPresentationBuilt = true;
}

void UFirearmComponent::PlayFirePresentation()
{
    ADeadbrickCharacter* Character = Cast<ADeadbrickCharacter>(GetOwner());
    if (!Character || !Character->ViewModelRoot) return;

    if (!bPresentationBuilt) BuildFallbackWeaponPresentation();

    Character->ViewModelRoot->SetRelativeLocation(BaseViewModelLocation + FVector(-3.5f, 0.0f, 1.4f));
    if (MuzzleFlashLight) MuzzleFlashLight->SetIntensity(8500.0f);

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(FirePresentationTimer);
        GetWorld()->GetTimerManager().SetTimer(FirePresentationTimer, this, &UFirearmComponent::ResetFirePresentation, 0.055f, false);
    }
}

void UFirearmComponent::ResetFirePresentation()
{
    if (ADeadbrickCharacter* Character = Cast<ADeadbrickCharacter>(GetOwner()))
    {
        if (Character->ViewModelRoot)
            Character->ViewModelRoot->SetRelativeLocation(BaseViewModelLocation);
    }
    if (MuzzleFlashLight) MuzzleFlashLight->SetIntensity(0.0f);
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
    {
        Director->ReportNoise(Origin, Stats.NoiseRadiusCm, Stats.NoiseIntensity, 4.0f);
    }

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
