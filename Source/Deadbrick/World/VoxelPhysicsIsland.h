#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelPhysicsIsland.generated.h"

class ADestructibleVoxelWorld;
class UProceduralMeshComponent;

UCLASS()
class DEADBRICK_API AVoxelPhysicsIsland : public AActor
{
    GENERATED_BODY()

public:
    AVoxelPhysicsIsland();

    UFUNCTION()
    void InitializeFromVoxels(ADestructibleVoxelWorld* SourceWorld, const TArray<FIntVector>& Voxels);

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> MeshComponent;
};
