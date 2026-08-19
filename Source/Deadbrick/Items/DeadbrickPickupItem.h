#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/DeadbrickItemTypes.h"
#include "World/DeadbrickVoxelTypes.h"
#include "DeadbrickPickupItem.generated.h"

class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class DEADBRICK_API ADeadbrickPickupItem : public AActor
{
    GENERATED_BODY()

public:
    ADeadbrickPickupItem();

    UPROPERTY(BlueprintReadOnly, Category="Item")
    EDeadbrickItemType ItemType = EDeadbrickItemType::ConcreteRubble;

    UPROPERTY(BlueprintReadOnly, Category="Item")
    int32 Quantity = 1;

    UFUNCTION(BlueprintCallable, Category="Item")
    void InitializeFromVoxelMaterial(EDeadbrickVoxelMaterial Material, int32 InQuantity);

    UFUNCTION(BlueprintCallable, Category="Item")
    void InitializeItem(EDeadbrickItemType InType, int32 InQuantity);

    UFUNCTION(BlueprintCallable, Category="Item")
    int32 Collect(EDeadbrickItemType& OutType);

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USphereComponent> PhysicsBody;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    void ApplyVisualForItemType();
};
