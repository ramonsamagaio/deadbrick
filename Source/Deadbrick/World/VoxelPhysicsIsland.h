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
    void InitializeFromVoxels(ADestructibleVoxelWorld* SourceWorld, const TArray<FIntVector>& Voxels, bool bStartSimulating = true);

    UFUNCTION()
    void ActivatePhysics();

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    float PreparedMassKg = 1.0f;
    bool bPreparedForPhysics = false;
};
