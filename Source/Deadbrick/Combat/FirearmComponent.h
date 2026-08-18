#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FirearmComponent.generated.h"

USTRUCT(BlueprintType)
struct FDeadbrickFirearmStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 34.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float VoxelDamage = 55.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float VoxelDamageRadiusCm = 16.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RangeCm = 12000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RoundsPerMinute = 600.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MagazineSize = 30;
};

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class DEADBRICK_API UFirearmComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFirearmComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Firearm") FDeadbrickFirearmStats Stats;
    UPROPERTY(BlueprintReadOnly, Category="Firearm") int32 AmmoInMagazine = 30;
    UPROPERTY(BlueprintReadOnly, Category="Firearm") int32 ReserveAmmo = 90;

    UFUNCTION(BlueprintCallable, Category="Firearm") bool FireFromCamera(const FVector& Origin, const FVector& Direction);
    UFUNCTION(BlueprintCallable, Category="Firearm") void Reload();

private:
    double LastShotTime = -1000.0;
};
