#include "Combat/FirearmComponent.h"
#include "World/DestructibleVoxelWorld.h"
#include "AI/ZombieDirectorSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UFirearmComponent::UFirearmComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UFirearmComponent::FireFromCamera(const FVector& Origin, const FVector& Direction)
{
    if (!GetWorld() || (!bInfiniteAmmo && AmmoInMagazine <= 0)) return false;

    const double Now = GetWorld()->GetTimeSeconds();
    const double SecondsPerShot = 60.0 / FMath::Max(1.0f, Stats.RoundsPerMinute);
    if (Now - LastShotTime < SecondsPerShot) return false;

    LastShotTime = Now;
    if (!bInfiniteAmmo) --AmmoInMagazine;

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
