#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Items/DeadbrickItemTypes.h"
#include "World/DeadbrickVoxelTypes.h"
#include "DeadbrickSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FDeadbrickInventorySaveRecord
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadWrite)
    EDeadbrickItemType ItemType = EDeadbrickItemType::WoodScrap;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 Quantity = 0;
};

UCLASS()
class DEADBRICK_API UDeadbrickSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 WorldSeed = 1337;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    FVector PlayerLocation = FVector::ZeroVector;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    FRotator PlayerRotation = FRotator::ZeroRotator;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    float PlayerHealth = 100.0f;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    TArray<FDeadbrickInventorySaveRecord> Inventory;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    TArray<FDeadbrickVoxelEditRecord> VoxelEdits;
};
