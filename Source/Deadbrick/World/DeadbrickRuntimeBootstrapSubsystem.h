#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeadbrickRuntimeBootstrapSubsystem.generated.h"

class ADestructibleVoxelWorld;
class AProceduralCityGenerator;
class APlayerController;

UCLASS()
class DEADBRICK_API UDeadbrickRuntimeBootstrapSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
    TWeakObjectPtr<ADestructibleVoxelWorld> RuntimeVoxelWorld;
    TWeakObjectPtr<AProceduralCityGenerator> RuntimeCityGenerator;
    FVector PrototypeOrigin = FVector(0.0, 0.0, 12000.0);
    int32 PlayerSetupAttempts = 0;

    void BuildPrototypeWorld();
    void EnsurePlayer();
    void SpawnPrototypeZombies();
    void ShowStatus(const FString& Message, const FColor& Color) const;
};
