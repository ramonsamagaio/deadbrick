#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Items/DeadbrickItemTypes.h"
#include "DeadbrickCraftingSubsystem.generated.h"

class ADeadbrickCharacter;

UENUM(BlueprintType)
enum class EDeadbrickRecipe : uint8
{
    Bandage,
    MetalPlate,
    RepairKit,
    Molotov,
    WoodenBarricade,
    PurifiedWater
};

UCLASS()
class DEADBRICK_API UDeadbrickCraftingSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Crafting")
    bool TryCraft(ADeadbrickCharacter* Player, EDeadbrickRecipe Recipe, bool bUseNearbyPhysicalItems = true);

    UFUNCTION(BlueprintPure, Category="Crafting")
    FString DescribeRecipe(EDeadbrickRecipe Recipe) const;

private:
    void GetRecipe(EDeadbrickRecipe Recipe, TMap<EDeadbrickItemType, int32>& OutIngredients, EDeadbrickItemType& OutProduct, int32& OutQuantity) const;
    void CollectNearbyPhysicalMaterials(ADeadbrickCharacter* Player, float RadiusCm) const;
};
