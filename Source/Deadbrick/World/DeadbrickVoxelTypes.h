#pragma once

#include "CoreMinimal.h"
#include "DeadbrickVoxelTypes.generated.h"

UENUM(BlueprintType)
enum class EDeadbrickVoxelMaterial : uint8
{
    Air = 0,
    Asphalt,
    Concrete,
    Brick,
    Glass,
    Wood,
    Metal,
    Soil
};

USTRUCT(BlueprintType)
struct FDeadbrickVoxel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDeadbrickVoxelMaterial Material = EDeadbrickVoxelMaterial::Air;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint8 Integrity = 0;

    bool IsSolid() const { return Material != EDeadbrickVoxelMaterial::Air; }
};

USTRUCT()
struct FDeadbrickVoxelChunk
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FDeadbrickVoxel> Voxels;
};
