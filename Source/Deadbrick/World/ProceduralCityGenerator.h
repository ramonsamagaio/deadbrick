#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/DeadbrickVoxelTypes.h"
#include "ProceduralCityGenerator.generated.h"

class ADestructibleVoxelWorld;
class UStaticMesh;
enum class EDeadbrickReferencePropRole : uint8;

UENUM(BlueprintType)
enum class EDeadbrickDistrictType : uint8
{
    Residential,
    Downtown,
    Commercial,
    Industrial,
    Civic,
    Medical,
    University,
    Suburban,
    LowIncome,
    Military
};

USTRUCT(BlueprintType)
struct FDeadbrickBuildingSpec
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FIntPoint Block = FIntPoint::ZeroValue;
    UPROPERTY(BlueprintReadOnly) FIntPoint Lot = FIntPoint::ZeroValue;
    UPROPERTY(BlueprintReadOnly) int32 Floors = 1;
    UPROPERTY(BlueprintReadOnly) EDeadbrickDistrictType District = EDeadbrickDistrictType::Residential;
};

UCLASS()
class DEADBRICK_API AProceduralCityGenerator : public AActor
{
    GENERATED_BODY()

public:
    AProceduralCityGenerator();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City") int32 Seed = 1337;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City", meta=(ClampMin="1", ClampMax="16")) int32 BlocksPerAxis = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City") float BlockSizeMeters = 72.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City") float StreetWidthMeters = 14.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City") float FloorHeightMeters = 3.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City") TObjectPtr<ADestructibleVoxelWorld> VoxelWorld;

    UPROPERTY(BlueprintReadOnly, Category="City") TArray<FDeadbrickBuildingSpec> Buildings;

    UFUNCTION(CallInEditor, BlueprintCallable, Category="City")
    void GenerateCity();

private:
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> ReferenceDoorMeshes;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> ReferenceWindowMeshes;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> ReferenceContainerMeshes;

    EDeadbrickDistrictType PickDistrict(FRandomStream& Stream, int32 BlockX, int32 BlockY) const;
    int32 PickFloors(EDeadbrickDistrictType District, FRandomStream& Stream) const;
    void LoadReferencePropMeshes();
    UStaticMesh* PickReferenceMesh(const TArray<TObjectPtr<UStaticMesh>>& Pool, FRandomStream& Stream) const;
    void BuildRoadGrid(FRandomStream& Stream);
    void BuildBlock(int32 BlockX, int32 BlockY, EDeadbrickDistrictType District, FRandomStream& Stream);
    void BuildShell(const FIntVector& Min, const FIntVector& Max, int32 FloorHeightVoxels, EDeadbrickVoxelMaterial WallMaterial, FRandomStream& Stream);
    void BuildInterior(const FIntVector& Min, const FIntVector& Max, int32 FloorHeightVoxels, FRandomStream& Stream);
    void BuildVoxelStairwell(const FIntVector& Min, const FIntVector& Max, int32 FloorHeightVoxels);
    void SpawnReferenceProp(UStaticMesh* Mesh, const FVector& WorldLocation, const FRotator& Rotation, const FVector& TargetDimensionsCm, EDeadbrickVoxelMaterial BreakMaterial, float Health, EDeadbrickReferencePropRole PropRole);
};
