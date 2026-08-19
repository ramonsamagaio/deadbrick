#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DeadbrickGameModeBase.generated.h"

class ADestructibleVoxelWorld;
class AProceduralCityGenerator;
class APlayerController;

UCLASS()
class DEADBRICK_API ADeadbrickGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    ADeadbrickGameModeBase();
    virtual void StartPlay() override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<ADestructibleVoxelWorld> RuntimeVoxelWorld;

    UPROPERTY(Transient)
    TObjectPtr<AProceduralCityGenerator> RuntimeCityGenerator;

    FVector PrototypeOrigin = FVector(0.0, 0.0, 12000.0);

    void BuildPrototypeWorld();
    void PositionPlayer(APlayerController* PlayerController) const;
    void SpawnPrototypeZombies();
};
