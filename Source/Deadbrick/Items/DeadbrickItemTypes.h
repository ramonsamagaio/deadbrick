#pragma once

#include "CoreMinimal.h"
#include "DeadbrickItemTypes.generated.h"

UENUM(BlueprintType)
enum class EDeadbrickItemType : uint8
{
    WoodScrap,
    BrickFragment,
    ConcreteRubble,
    GlassShard,
    MetalScrap,
    AsphaltChunk,
    SoilClump,
    Cloth,
    Electronics,
    Plastic,
    Wire,
    Nails,
    Ammo9mm,
    RifleAmmo,
    ShotgunShells,
    CannedFood,
    WaterBottle,
    PurifiedWater,
    MedicalSupplies,
    Bandage,
    Battery,
    Fuel,
    MechanicalParts,
    MetalPlate,
    RepairKit,
    Molotov,
    WoodenBarricade
};
