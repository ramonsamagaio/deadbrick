#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/DeadbrickVoxelTypes.h"
#include "ReferenceDestructibleProp.generated.h"

class ADeadbrickCharacter;
class ADestructibleVoxelWorld;
class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EDeadbrickReferencePropRole : uint8
{
    Generic,
    Door,
    Window,
    Container
};

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
        float InHealth = 80.0f,
        EDeadbrickReferencePropRole InRole = EDeadbrickReferencePropRole::Generic);

    UFUNCTION(BlueprintCallable, Category="Reference Prop")
    bool Interact(ADeadbrickCharacter* Player);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Reference Prop")
    TObjectPtr<UBoxComponent> CollisionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Reference Prop")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(BlueprintReadOnly, Category="Reference Prop")
    EDeadbrickReferencePropRole ReferencePropRole = EDeadbrickReferencePropRole::Generic;

private:
    UPROPERTY(Transient)
    TObjectPtr<ADestructibleVoxelWorld> VoxelWorld;

    EDeadbrickVoxelMaterial BreakMaterial = EDeadbrickVoxelMaterial::Wood;
    float Health = 80.0f;
    bool bOpen = false;
    bool bLooted = false;
    FRotator ClosedRotation = FRotator::ZeroRotator;

    void BreakIntoVoxels();
    void SpawnContainerLoot();
};
