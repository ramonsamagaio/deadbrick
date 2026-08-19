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
    Ammo9mm,
    RifleAmmo,
    ShotgunShells,
    CannedFood,
    WaterBottle,
    MedicalSupplies,
    Battery,
    Fuel,
    MechanicalParts
};
