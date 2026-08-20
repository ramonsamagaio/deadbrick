#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/DeadbrickVoxelTypes.h"
#include "VoxelPhysicsIsland.generated.h"

class ADestructibleVoxelWorld;
class UPrimitiveComponent;
class UProceduralMeshComponent;

UCLASS()
class DEADBRICK_API AVoxelPhysicsIsland : public AActor
{
    GENERATED_BODY()

public:
    AVoxelPhysicsIsland();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    virtual void NotifyHit(
        UPrimitiveComponent* MyComp,
        AActor* Other,
        UPrimitiveComponent* OtherComp,
        bool bSelfMoved,
        FVector HitLocation,
        FVector HitNormal,
        FVector NormalImpulse,
        const FHitResult& Hit) override;

    UFUNCTION()
    void InitializeFromVoxels(ADestructibleVoxelWorld* SourceWorld, const TArray<FIntVector>& Voxels, bool bStartSimulating = true);

    UFUNCTION()
    void ActivatePhysics();

    UFUNCTION(BlueprintCallable, Category="Voxel|Physics")
    void PushFromGameplay(const FVector& Impulse);

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    UPROPERTY()
    TWeakObjectPtr<ADestructibleVoxelWorld> SourceVoxelWorld;

    float PreparedMassKg = 1.0f;
    FVector PreparedLocalCenter = FVector::ZeroVector;
    FVector PreparedHalfExtents = FVector(10.0f);
    int64 PhysXBodyHandle = INDEX_NONE;
    bool bPreparedForPhysics = false;
    bool bUsingPhysX = false;

    int32 SourceVoxelCount = 0;
    float RubbleDurability = 100.0f;
    EDeadbrickVoxelMaterial DominantMaterial = EDeadbrickVoxelMaterial::Concrete;

    void ApplyGameplayImpulse(const FVector& Impulse);
    void BreakIntoSalvage();
};
