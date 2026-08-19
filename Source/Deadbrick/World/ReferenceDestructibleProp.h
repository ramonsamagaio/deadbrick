#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/DeadbrickVoxelTypes.h"
#include "ReferenceDestructibleProp.generated.h"

class ADestructibleVoxelWorld;
class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class DEADBRICK_API AReferenceDestructibleProp : public AActor
{
    GENERATED_BODY()

public:
    AReferenceDestructibleProp();

    virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    void InitializeFromReference(
        UStaticMesh* InMesh,
        ADestructibleVoxelWorld* InVoxelWorld,
        EDeadbrickVoxelMaterial InBreakMaterial,
        const FVector& TargetDimensionsCm,
        float InHealth = 80.0f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Reference Prop")
    TObjectPtr<UBoxComponent> CollisionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Reference Prop")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

private:
    UPROPERTY(Transient)
    TObjectPtr<ADestructibleVoxelWorld> VoxelWorld;

    EDeadbrickVoxelMaterial BreakMaterial = EDeadbrickVoxelMaterial::Wood;
    float Health = 80.0f;

    void BreakIntoVoxels();
};
