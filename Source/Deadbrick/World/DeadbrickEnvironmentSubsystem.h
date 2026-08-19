#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeadbrickEnvironmentSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FDeadbrickEnvironmentCell
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float Water = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Gas = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Fuel = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Fire = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Smoke = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Temperature = 20.0f;

    bool IsActive() const
    {
        return Water > 0.005f || Gas > 0.005f || Fuel > 0.005f || Fire > 0.005f || Smoke > 0.005f || Temperature > 25.0f;
    }
};

UCLASS()
class DEADBRICK_API UDeadbrickEnvironmentSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    UFUNCTION(BlueprintCallable, Category="Environment")
    void AddWaterAtWorld(const FVector& WorldLocation, float Amount = 1.0f);

    UFUNCTION(BlueprintCallable, Category="Environment")
    void AddGasAtWorld(const FVector& WorldLocation, float Amount = 1.0f);

    UFUNCTION(BlueprintCallable, Category="Environment")
    void AddFuelAtWorld(const FVector& WorldLocation, float Amount = 1.0f);

    UFUNCTION(BlueprintCallable, Category="Environment")
    void IgniteAtWorld(const FVector& WorldLocation, float Intensity = 1.0f);

    UFUNCTION(BlueprintPure, Category="Environment")
    FDeadbrickEnvironmentCell GetCellAtWorld(const FVector& WorldLocation) const;

private:
    TMap<FIntVector, FDeadbrickEnvironmentCell> Cells;
    FTimerHandle SimulationTimer;
    int32 MaxCellsPerStep = 4096;

    void StepSimulation();
    class ADestructibleVoxelWorld* FindVoxelWorld() const;
    bool IsOpenCell(class ADestructibleVoxelWorld* VoxelWorld, const FIntVector& Coord) const;
};
