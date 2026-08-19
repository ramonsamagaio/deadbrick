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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void InitializeFromVoxels(ADestructibleVoxelWorld* SourceWorld, const TArray<FIntVector>& Voxels, bool bStartSimulating = true);

    UFUNCTION()
    void ActivatePhysics();

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    float PreparedMassKg = 1.0f;
    FVector PreparedLocalCenter = FVector::ZeroVector;
    FVector PreparedHalfExtents = FVector(10.0f);
    int64 PhysXBodyHandle = INDEX_NONE;
    bool bPreparedForPhysics = false;
    bool bUsingPhysX = false;
};
