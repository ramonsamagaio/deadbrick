#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeadbrickVoxelTypes.h"
#include "DestructibleVoxelWorld.generated.h"

class UMaterialInterface;
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Physics")
    bool bEnableStructuralGravity = true;

    // Structural checks happen only after meaningful destruction, so we can afford a much larger
    // connectivity scan than the first prototype. This prevents whole upper floors being ignored.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Physics", meta=(ClampMin="4096", ClampMax="500000"))
    int32 MaxStructuralScanVoxels = 160000;

    // Detached structures are split spatially into several Chaos bodies instead of creating one
    // enormous convex hull or refusing to fall because the component is too large.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Physics", meta=(ClampMin="64", ClampMax="8192"))
    int32 MaxPhysicsIslandVoxels = 1024;

    // Approximate load capacity of one grounded contact voxel. An intact footprint has hundreds of
    // contacts; a skyscraper hanging from a tiny leftover pillar will fail under its own mass.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Physics", meta=(ClampMin="8.0", ClampMax="512.0"))
    float SupportCapacityPerGroundVoxel = 96.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Items")
    bool bSpawnSalvageDrops = true;

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

    UFUNCTION(BlueprintCallable, Category="Voxel|Save")
    void StartRuntimePersistence();

    void ExportRuntimeEdits(TArray<FDeadbrickVoxelEditRecord>& OutEdits) const;
    void ApplyRuntimeEdits(const TArray<FDeadbrickVoxelEditRecord>& Edits);

    bool GetVoxel(const FIntVector& Voxel, FDeadbrickVoxel& OutVoxel) const;
    void BeginBulkEdit();
    void EndBulkEdit();

private:
    TMap<FIntVector, FDeadbrickVoxelChunk> Chunks;
    TMap<FIntVector, TObjectPtr<UProceduralMeshComponent>> ChunkMeshes;
    TMap<EDeadbrickVoxelMaterial, TObjectPtr<UMaterialInterface>> MaterialCache;
    TSet<EDeadbrickVoxelMaterial> MaterialResolutionAttempted;
    TMap<FIntVector, FDeadbrickVoxel> RuntimeEdits;
    bool bRecordRuntimeEdits = false;

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
    UMaterialInterface* ResolveSurfaceMaterial(EDeadbrickVoxelMaterial Material);
    void ResolveStructuralGravityNear(const FVector& WorldCenter, float RadiusCm);
    void SpawnDetachedComponentAsPhysics(const TArray<FIntVector>& Component);
    void SpawnSalvageDrops(const FVector& WorldCenter, const TMap<EDeadbrickVoxelMaterial, int32>& DestroyedByMaterial);
};
