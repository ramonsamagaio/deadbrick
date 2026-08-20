#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeadbrickVoxelTypes.h"
#include "DestructibleVoxelWorld.generated.h"

class AVoxelPhysicsIsland;
class UMaterialInterface;
class UProceduralMeshComponent;

UCLASS(BlueprintType)
class DEADBRICK_API ADestructibleVoxelWorld : public AActor
{
    GENERATED_BODY()

public:
    ADestructibleVoxelWorld();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel")
    float VoxelSizeCm = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Voxel")
    int32 ChunkSize = 32;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Physics")
    bool bEnableStructuralGravity = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Physics", meta=(ClampMin="4096", ClampMax="524288"))
    int32 MaxStructuralScanVoxels = 262144;

    // Preferred amount of source voxel material represented by one falling rubble piece. Large
    // structures may raise the effective target just enough to respect the body safety ceiling.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Physics", meta=(ClampMin="8", ClampMax="512"))
    int32 MaxPhysicsIslandVoxels = 48;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Physics", meta=(ClampMin="4096", ClampMax="524288"))
    int32 MaxDetachedComponentVoxels = 262144;

    // Simple PhysX boxes are cheap enough to carry many local rubble pieces. A higher ceiling keeps
    // the visual result granular while preparation remains amortized over several render frames.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Physics", meta=(ClampMin="1", ClampMax="256"))
    int32 MaxPhysicsBodiesPerCollapse = 128;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Performance", meta=(ClampMin="1", ClampMax="8"))
    int32 PhysicsIslandBuildBudgetPerFrame = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Performance", meta=(ClampMin="256", ClampMax="32768"))
    int32 StructuralWorkBudgetPerFrame = 4096;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Performance", meta=(ClampMin="1", ClampMax="16"))
    int32 ChunkRebuildBudgetPerFrame = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxel|Physics")
    float SupportCapacityPerGroundVoxel = 384.0f;

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

    UFUNCTION(BlueprintCallable, Category="Voxel|Physics")
    void EvaluateStructuralGravity(const FVector& WorldCenter, float RadiusCm = 650.0f);

    UFUNCTION(BlueprintCallable, Category="Voxel|Save")
    void StartRuntimePersistence();

    void ExportRuntimeEdits(TArray<FDeadbrickVoxelEditRecord>& OutEdits) const;
    void ApplyRuntimeEdits(const TArray<FDeadbrickVoxelEditRecord>& Edits);

    bool GetVoxel(const FIntVector& Voxel, FDeadbrickVoxel& OutVoxel) const;
    UMaterialInterface* GetSurfaceMaterialForVoxel(EDeadbrickVoxelMaterial Material) { return ResolveSurfaceMaterial(Material); }
    void BeginBulkEdit();
    void EndBulkEdit();

private:
    struct FStructuralQueryState
    {
        TArray<FIntVector> Seeds;
        int32 SeedIndex = 0;
        TSet<FIntVector> Visited;
        TArray<FIntVector> Queue;
        int32 QueueReadIndex = 0;
        TArray<FIntVector> Component;
        bool bComponentActive = false;
        bool bGrounded = false;
        bool bHitScanLimit = false;

        void ResetComponent()
        {
            Queue.Reset();
            QueueReadIndex = 0;
            Component.Reset();
            bComponentActive = false;
            bGrounded = false;
            bHitScanLimit = false;
        }
    };

    struct FPendingCollapseState
    {
        TArray<FIntVector> Cells;
        TArray<TArray<FIntVector>> Groups;
        TArray<TWeakObjectPtr<AVoxelPhysicsIsland>> PreparedIslands;
        int32 NextGroupIndex = 0;
    };

    TMap<FIntVector, FDeadbrickVoxelChunk> Chunks;
    TMap<FIntVector, TObjectPtr<UProceduralMeshComponent>> ChunkMeshes;
    TMap<EDeadbrickVoxelMaterial, TObjectPtr<UMaterialInterface>> MaterialCache;
    TSet<EDeadbrickVoxelMaterial> MaterialResolutionAttempted;
    TMap<FIntVector, FDeadbrickVoxel> RuntimeEdits;
    bool bRecordRuntimeEdits = false;
    bool bDeferRuntimeChunkRebuilds = false;
    bool bAsyncChunkCookingConfigured = false;

    TSet<FIntVector> DirtyChunks;
    int32 BulkEditDepth = 0;

    TSet<FIntVector> PendingStructuralSeeds;
    TArray<FStructuralQueryState> StructuralQueries;

    TSet<FIntVector> PendingCollapseCells;
    TArray<FPendingCollapseState> PendingCollapses;

    static int32 FloorDiv(int32 Value, int32 Divisor);
    static int32 PositiveMod(int32 Value, int32 Divisor);
    FIntVector ToChunkCoord(const FIntVector& Voxel) const;
    FIntVector ToLocalCoord(const FIntVector& Voxel) const;
    int32 ToIndex(const FIntVector& Local) const;
    FDeadbrickVoxelChunk& FindOrCreateChunk(const FIntVector& ChunkCoord);
    UProceduralMeshComponent* FindOrCreateChunkMesh(const FIntVector& ChunkCoord);
    void MarkDirty(const FIntVector& ChunkCoord);
    void FlushDirtyChunkBudget();
    void RebuildChunk(const FIntVector& ChunkCoord);
    uint8 DefaultIntegrityFor(EDeadbrickVoxelMaterial Material) const;
    UMaterialInterface* ResolveSurfaceMaterial(EDeadbrickVoxelMaterial Material);
    void ResolveStructuralGravityNear(const FVector& WorldCenter, float RadiusCm);
    void QueueStructuralCheckFromDestroyed(const TArray<FIntVector>& DestroyedCells);
    void StartNextStructuralQueryIfNeeded();
    void ProcessStructuralQueries();
    void QueueDetachedComponentForPhysics(const TArray<FIntVector>& Component);
    void ProcessPendingCollapses();
    void DetachCellsFromStaticWorld(const TArray<FIntVector>& Cells);
    void SpawnDetachedComponentAsPhysics(const TArray<FIntVector>& Component);
    void SpawnSalvageDrops(const FVector& WorldCenter, const TMap<EDeadbrickVoxelMaterial, int32>& DestroyedByMaterial);
};
