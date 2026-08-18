#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZombieDirectorSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FDeadbrickNoiseEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float RadiusCm = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Intensity = 0.0f;
    UPROPERTY(BlueprintReadOnly) double ExpireAt = 0.0;
};

UCLASS()
class DEADBRICK_API UDeadbrickZombieDirectorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Zombie Director")
    void ReportNoise(const FVector& Location, float RadiusCm, float Intensity = 1.0f, float LifetimeSeconds = 4.0f);

    UFUNCTION(BlueprintCallable, Category="Zombie Director")
    bool FindStrongestNoise(const FVector& ListenerLocation, float MaxListenRadiusCm, FVector& OutLocation, float& OutScore);

private:
    UPROPERTY()
    TArray<FDeadbrickNoiseEvent> NoiseEvents;

    void PruneExpired();
};
