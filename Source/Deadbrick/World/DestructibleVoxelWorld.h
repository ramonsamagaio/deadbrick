#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeadbrickVoxelTypes.h"
#include "DestructibleVoxelWorld.generated.h"

class UProceduralMeshComponent;

UCLASS(BlueprintType)
class DEADBRICK_API ADestructibleVoxelWorld : public AActor
{
    GENERATED_BODY()

public:
    ADestructibleVoxelWorld();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel")
    float VoxelSizeCm = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel")
    int32 ChunkSize = 32;

    UFUNCTION(BlueprintCallable, Category="Voxel")
    FIntVector WorldToVoxel(const FVector& WorldPosition) const;

    UFUNCTION(BlueprintCallable, Category="Voxel")
    FVector VoxelToWorld(const FIntVector& Voxel) const;

    UFUNCTION(BlueprintCallable, Category="Voxel")
    void SetVoxel(const FIntVector& Voxel, EDeadbrickVoxelMaterial Material, uint8 Integrity = 255);

    UFUNCTION(BlueprintCallable, Category="Voxel")
    void FillBox(const FIntVector& MinVoxel, const FIntVector& MaxVoxel, EDeadbrickVoxelMaterial Material, uint8 Integrity = 255);

    UFUNCTION(BlueprintCallable, Category="Voxel")
    int32 ApplySphereDamage(const FVector& WorldCenter, float RadiusCm, float Damage);

    bool GetVoxel(const FIntVector& Voxel, FDeadbrickVoxel& OutVoxel) const;
    void BeginBulkEdit();
    void EndBulkEdit();

private:
    // These maps intentionally stay out of UPROPERTY reflection. They are runtime caches keyed by FIntVector.
    TMap<FIntVector, FDeadbrickVoxelChunk> Chunks;
    TMap<FIntVector, TObjectPtr<UProceduralMeshComponent>> ChunkMeshes;

    TSet<FIntVector> DirtyChunks;
    int32 BulkEditDepth = 0;

    static int32 FloorDiv(int32 Value, int32 Divisor);
    static int32 PositiveMod(int32 Value, int32 Divisor);
    FIntVector ToChunkCoord(const FIntVector& Voxel) const;
    FIntVector ToLocalCoord(const FIntVector& Voxel) const;
    int32 ToIndex(const FIntVector& Local) const;
    FDeadbrickVoxelChunk& FindOrCreateChunk(const FIntVector& ChunkCoord);
    UProceduralMeshComponent* FindOrCreateChunkMesh(const FIntVector& ChunkCoord);
    void MarkDirty(const FIntVector& ChunkCoord);
    void RebuildChunk(const FIntVector& ChunkCoord);
    uint8 DefaultIntegrityFor(EDeadbrickVoxelMaterial Material) const;
};
