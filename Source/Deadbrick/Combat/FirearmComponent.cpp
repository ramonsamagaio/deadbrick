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
    if (!GetWorld() || AmmoInMagazine <= 0) return false;

    const double Now = GetWorld()->GetTimeSeconds();
    const double SecondsPerShot = 60.0 / FMath::Max(1.0f, Stats.RoundsPerMinute);
    if (Now - LastShotTime < SecondsPerShot) return false;

    LastShotTime = Now;
    --AmmoInMagazine;

    if (UDeadbrickZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UDeadbrickZombieDirectorSubsystem>())
    {
        Director->ReportNoise(Origin, Stats.NoiseRadiusCm, Stats.NoiseIntensity, 4.0f);
    }

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DeadbrickFirearm), true, GetOwner());
    const FVector End = Origin + Direction.GetSafeNormal() * Stats.RangeCm;

    if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params)) return true;

    if (ADestructibleVoxelWorld* VoxelWorld = Cast<ADestructibleVoxelWorld>(Hit.GetActor()))
    {
        VoxelWorld->ApplySphereDamage(Hit.ImpactPoint - Hit.ImpactNormal * 2.0f, Stats.VoxelDamageRadiusCm, Stats.VoxelDamage);
    }
    else if (AActor* HitActor = Hit.GetActor())
    {
        UGameplayStatics::ApplyPointDamage(HitActor, Stats.Damage, Direction, Hit, GetOwner()->GetInstigatorController(), GetOwner(), nullptr);
    }

    return true;
}

void UFirearmComponent::Reload()
{
    const int32 Missing = FMath::Max(0, Stats.MagazineSize - AmmoInMagazine);
    const int32 ToLoad = FMath::Min(Missing, ReserveAmmo);
    AmmoInMagazine += ToLoad;
    ReserveAmmo -= ToLoad;
}
