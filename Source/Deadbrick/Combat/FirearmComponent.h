#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FirearmComponent.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FDeadbrickFirearmStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 45.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float VoxelDamage = 320.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float VoxelDamageRadiusCm = 45.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RangeCm = 12000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RoundsPerMinute = 600.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MagazineSize = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float NoiseRadiusCm = 7000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float NoiseIntensity = 1.0f;
};

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class DEADBRICK_API UFirearmComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFirearmComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Firearm") FDeadbrickFirearmStats Stats;

    // Prototype convenience: firing never consumes ammo and Reload becomes a no-op while enabled.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Firearm|Prototype") bool bInfiniteAmmo = true;

    UPROPERTY(BlueprintReadOnly, Category="Firearm") int32 AmmoInMagazine = 30;
    UPROPERTY(BlueprintReadOnly, Category="Firearm") int32 ReserveAmmo = 90;

    UFUNCTION(BlueprintCallable, Category="Firearm") bool FireFromCamera(const FVector& Origin, const FVector& Direction);
    UFUNCTION(BlueprintCallable, Category="Firearm") void Reload();

protected:
    virtual void BeginPlay() override;

private:
    double LastShotTime = -1000.0;

    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WeaponStock;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WeaponBarrel;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WeaponGrip;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WeaponMagazine;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WeaponSight;
    UPROPERTY(Transient) TObjectPtr<UPointLightComponent> MuzzleFlashLight;

    FTimerHandle FirePresentationTimer;
    FVector BaseViewModelLocation = FVector::ZeroVector;
    bool bPresentationBuilt = false;

    void BuildFallbackWeaponPresentation();
    void PlayFirePresentation();
    void ResetFirePresentation();
};
