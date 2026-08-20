#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeadbrickEnvironmentDirector.generated.h"

class ADestructibleVoxelWorld;
class AProceduralCityGenerator;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UPostProcessComponent;
class USceneComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS()
class DEADBRICK_API ADeadbrickEnvironmentDirector : public AActor
{
    GENERATED_BODY()

public:
    ADeadbrickEnvironmentDirector();

    UFUNCTION(BlueprintCallable, Category="Environment")
    void InitializeForCity(AProceduralCityGenerator* CityGenerator, ADestructibleVoxelWorld* VoxelWorld);

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> SunLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkyLightComponent> SkyLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UExponentialHeightFogComponent> HeightFog;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> PostProcess;

    UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> WeedInstances;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShrubInstances;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TrashInstances;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PoleInstances;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> LampHeadInstances;

    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BasicShapeMaterial;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> CylinderMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> SphereMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> ConeMesh;

    bool bInitialized = false;

    void DisablePreexistingEnvironment();
    void ConfigureLightingAndGrade();
    void ConfigureInstanceMaterials();
    void PopulateStreetDressing(AProceduralCityGenerator* CityGenerator, ADestructibleVoxelWorld* VoxelWorld);
    void SpawnLocalAtmospherePockets(AProceduralCityGenerator* CityGenerator, ADestructibleVoxelWorld* VoxelWorld);
    void SpawnStreetLamp(const FVector& WorldLocation, float YawDegrees);
};
