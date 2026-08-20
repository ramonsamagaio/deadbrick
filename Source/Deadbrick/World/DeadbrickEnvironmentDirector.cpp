#include "World/DeadbrickEnvironmentDirector.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/LocalFogVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Reference/ReferenceAssetResolver.h"
#include "UObject/ConstructorHelpers.h"
#include "World/DestructibleVoxelWorld.h"
#include "World/ProceduralCityGenerator.h"

namespace
{
    UMaterialInstanceDynamic* MakeTintedMaterial(
        UObject* Outer,
        UMaterialInterface* BaseMaterial,
        const FLinearColor& Color,
        float Roughness)
    {
        if (!Outer || !BaseMaterial) return nullptr;
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Outer);
        if (!Material) return nullptr;
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
        return Material;
    }
}

ADeadbrickEnvironmentDirector::ADeadbrickEnvironmentDirector()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("EnvironmentRoot"));
    SetRootComponent(SceneRoot);

    SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
    SunLight->SetupAttachment(SceneRoot);
    SunLight->SetMobility(EComponentMobility::Movable);

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(SceneRoot);
    SkyLight->SetMobility(EComponentMobility::Movable);

    SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    SkyAtmosphere->SetupAttachment(SceneRoot);

    HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
    HeightFog->SetupAttachment(SceneRoot);

    PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
    PostProcess->SetupAttachment(SceneRoot);
    PostProcess->bUnbound = true;
    PostProcess->BlendWeight = 1.0f;
    PostProcess->Priority = 50.0f;

    auto ConfigureInstances = [&](UHierarchicalInstancedStaticMeshComponent* Component)
    {
        Component->SetupAttachment(SceneRoot);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCanEverAffectNavigation(false);
        Component->SetMobility(EComponentMobility::Static);
        Component->bCastDynamicShadow = true;
    };

    WeedInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("WeedInstances"));
    ConfigureInstances(WeedInstances);

    ShrubInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ShrubInstances"));
    ConfigureInstances(ShrubInstances);

    TrashInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TrashInstances"));
    ConfigureInstances(TrashInstances);

    PoleInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PoleInstances"));
    ConfigureInstances(PoleInstances);

    LampHeadInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("LampHeadInstances"));
    ConfigureInstances(LampHeadInstances);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    CubeMesh = Cube.Succeeded() ? Cube.Object : nullptr;
    CylinderMesh = Cylinder.Succeeded() ? Cylinder.Object : nullptr;
    SphereMesh = Sphere.Succeeded() ? Sphere.Object : nullptr;
    ConeMesh = Cone.Succeeded() ? Cone.Object : nullptr;
    BasicShapeMaterial = ShapeMaterial.Succeeded() ? ShapeMaterial.Object : nullptr;

    WeedInstances->SetStaticMesh(ConeMesh);
    ShrubInstances->SetStaticMesh(SphereMesh);
    TrashInstances->SetStaticMesh(CubeMesh);
    PoleInstances->SetStaticMesh(CylinderMesh);
    LampHeadInstances->SetStaticMesh(CubeMesh);
}

void ADeadbrickEnvironmentDirector::InitializeForCity(
    AProceduralCityGenerator* CityGenerator,
    ADestructibleVoxelWorld* VoxelWorld)
{
    if (bInitialized || !CityGenerator || !VoxelWorld || !GetWorld()) return;
    bInitialized = true;

    ConfigureLightingAndGrade();
    ConfigureInstanceMaterials();
    PopulateStreetDressing(CityGenerator, VoxelWorld);
    SpawnLocalAtmospherePockets(CityGenerator, VoxelWorld);

    UE_LOG(LogTemp, Display,
        TEXT("DEADBRICK ENVIRONMENT READY | sky atmosphere + overcast sun + volumetric height fog + local haze + global grade + instanced street life"));
}

void ADeadbrickEnvironmentDirector::ConfigureLightingAndGrade()
{
    // Cloudy late-day lighting: readable enough for scavenging, but with long cool shadows and warm
    // practical lights. The directional light uses lux in UE5's physical-lighting workflow.
    SetActorRotation(FRotator(-34.0f, -28.0f, 0.0f));

    if (SunLight)
    {
        SunLight->SetIntensity(18000.0f);
        SunLight->SetLightColor(FLinearColor(0.82f, 0.88f, 0.92f));
        SunLight->SetIndirectLightingIntensity(0.85f);
        SunLight->SetAtmosphereSunLight(true);
        SunLight->SetAtmosphereSunLightIndex(0);
        SunLight->bEnableLightShaftOcclusion = true;
        SunLight->bCastShadowsOnAtmosphere = true;
    }

    if (SkyLight)
    {
        SkyLight->SetIntensity(0.70f);
        SkyLight->SetIndirectLightingIntensity(1.10f);
        SkyLight->SetLightColor(FLinearColor(0.72f, 0.80f, 0.88f));
        SkyLight->SetLowerHemisphereColor(FLinearColor(0.035f, 0.045f, 0.04f));
        SkyLight->SetRealTimeCapture(true);
    }

    if (HeightFog)
    {
        HeightFog->SetFogDensity(0.011f);
        HeightFog->SetFogHeightFalloff(0.18f);
        HeightFog->SetFogInscatteringColor(FLinearColor(0.16f, 0.22f, 0.24f));
        HeightFog->SetFogMaxOpacity(0.72f);
        HeightFog->SetStartDistance(650.0f);
        HeightFog->SetSecondFogDensity(0.0045f);
        HeightFog->SetSecondFogHeightFalloff(0.08f);
        HeightFog->SetSecondFogHeightOffset(-90.0f);

        HeightFog->SetVolumetricFog(true);
        HeightFog->SetVolumetricFogScatteringDistribution(0.18f);
        HeightFog->SetVolumetricFogAlbedo(FColor(205, 218, 214));
        HeightFog->SetVolumetricFogExtinctionScale(0.72f);
        HeightFog->SetVolumetricFogDistance(8500.0f);
        HeightFog->SetVolumetricFogStartDistance(150.0f);
        HeightFog->SetVolumetricFogNearFadeInDistance(300.0f);
    }

    if (PostProcess)
    {
        FPostProcessSettings& Settings = PostProcess->Settings;

        Settings.bOverride_AutoExposureBias = true;
        Settings.AutoExposureBias = -0.35f;

        Settings.bOverride_ColorSaturation = true;
        Settings.ColorSaturation = FVector4(0.82f, 0.87f, 0.83f, 1.0f);

        Settings.bOverride_ColorContrast = true;
        Settings.ColorContrast = FVector4(1.07f, 1.06f, 1.05f, 1.0f);

        Settings.bOverride_ColorOffsetShadows = true;
        Settings.ColorOffsetShadows = FVector4(-0.008f, 0.004f, 0.012f, 0.0f);

        Settings.bOverride_ColorGainHighlights = true;
        Settings.ColorGainHighlights = FVector4(1.03f, 1.015f, 0.97f, 1.0f);

        Settings.bOverride_BloomIntensity = true;
        Settings.BloomIntensity = 0.28f;

        Settings.bOverride_VignetteIntensity = true;
        Settings.VignetteIntensity = 0.19f;

        Settings.bOverride_MotionBlurAmount = true;
        Settings.MotionBlurAmount = 0.08f;

        Settings.bOverride_FilmSlope = true;
        Settings.FilmSlope = 0.92f;
        Settings.bOverride_FilmToe = true;
        Settings.FilmToe = 0.48f;
        Settings.bOverride_FilmShoulder = true;
        Settings.FilmShoulder = 0.23f;
    }
}

void ADeadbrickEnvironmentDirector::ConfigureInstanceMaterials()
{
    if (!BasicShapeMaterial) return;

    if (UMaterialInstanceDynamic* WeedMaterial = MakeTintedMaterial(
        this, BasicShapeMaterial, FLinearColor(0.105f, 0.16f, 0.055f), 0.96f))
        WeedInstances->SetMaterial(0, WeedMaterial);

    if (UMaterialInstanceDynamic* ShrubMaterial = MakeTintedMaterial(
        this, BasicShapeMaterial, FLinearColor(0.065f, 0.105f, 0.042f), 0.93f))
        ShrubInstances->SetMaterial(0, ShrubMaterial);

    if (UMaterialInstanceDynamic* TrashMaterial = MakeTintedMaterial(
        this, BasicShapeMaterial, FLinearColor(0.115f, 0.105f, 0.09f), 0.82f))
        TrashInstances->SetMaterial(0, TrashMaterial);

    if (UMaterialInstanceDynamic* PoleMaterial = MakeTintedMaterial(
        this, BasicShapeMaterial, FLinearColor(0.045f, 0.052f, 0.05f), 0.72f))
        PoleInstances->SetMaterial(0, PoleMaterial);

    if (UMaterialInstanceDynamic* LampMaterial = MakeTintedMaterial(
        this, BasicShapeMaterial, FLinearColor(0.18f, 0.15f, 0.09f), 0.58f))
        LampHeadInstances->SetMaterial(0, LampMaterial);
}

void ADeadbrickEnvironmentDirector::PopulateStreetDressing(
    AProceduralCityGenerator* CityGenerator,
    ADestructibleVoxelWorld* VoxelWorld)
{
    if (!CityGenerator || !VoxelWorld) return;

    WeedInstances->ClearInstances();
    ShrubInstances->ClearInstances();
    TrashInstances->ClearInstances();
    PoleInstances->ClearInstances();
    LampHeadInstances->ClearInstances();

    const float BlockCm = CityGenerator->BlockSizeMeters * 100.0f;
    const float StreetCm = CityGenerator->StreetWidthMeters * 100.0f;
    const float StepCm = BlockCm + StreetCm;
    const float GroundZ = VoxelWorld->GetActorLocation().Z + VoxelWorld->VoxelSizeCm * 1.55f;
    FRandomStream Stream(CityGenerator->Seed ^ 0x5A17C3);

    // Sidewalk-edge weeds and debris are deliberately concentrated at curb seams and building
    // margins. This gives the streets parallax and scale cues without putting hundreds of Actors in
    // the world. HISM keeps all of these as a handful of draw calls.
    for (int32 BY = 0; BY < CityGenerator->BlocksPerAxis; ++BY)
    for (int32 BX = 0; BX < CityGenerator->BlocksPerAxis; ++BX)
    {
        const float BlockX0 = StreetCm + BX * StepCm;
        const float BlockY0 = StreetCm + BY * StepCm;
        const float BlockX1 = BlockX0 + BlockCm;
        const float BlockY1 = BlockY0 + BlockCm;

        for (float Along = 180.0f; Along < BlockCm - 180.0f; Along += Stream.FRandRange(240.0f, 410.0f))
        {
            const float Jitter = Stream.FRandRange(-55.0f, 55.0f);
            const float SideInset = Stream.FRandRange(65.0f, 125.0f);
            const float WeedScale = Stream.FRandRange(0.12f, 0.28f);
            const float WeedHeight = Stream.FRandRange(0.32f, 0.62f);

            const FVector Positions[4] =
            {
                FVector(BlockX0 + Along, BlockY0 + SideInset + Jitter, GroundZ),
                FVector(BlockX0 + Along, BlockY1 - SideInset + Jitter, GroundZ),
                FVector(BlockX0 + SideInset + Jitter, BlockY0 + Along, GroundZ),
                FVector(BlockX1 - SideInset + Jitter, BlockY0 + Along, GroundZ)
            };

            for (const FVector& Position : Positions)
            {
                const FRotator Rotation(
                    Stream.FRandRange(-8.0f, 8.0f),
                    Stream.FRandRange(-180.0f, 180.0f),
                    Stream.FRandRange(-8.0f, 8.0f));
                WeedInstances->AddInstance(
                    FTransform(Rotation, Position, FVector(WeedScale, WeedScale, WeedHeight)), true);

                if (Stream.FRand() < 0.17f)
                {
                    TrashInstances->AddInstance(
                        FTransform(
                            FRotator(Stream.FRandRange(-25.0f, 25.0f), Stream.FRandRange(-180.0f, 180.0f), Stream.FRandRange(-25.0f, 25.0f)),
                            Position + FVector(Stream.FRandRange(-55.0f, 55.0f), Stream.FRandRange(-55.0f, 55.0f), 5.0f),
                            FVector(Stream.FRandRange(0.08f, 0.18f), Stream.FRandRange(0.06f, 0.16f), Stream.FRandRange(0.025f, 0.08f))),
                        true);
                }
            }
        }

        // Sparse scrub in forgotten corners. The asymmetry is intentional so the procedural city
        // does not read as four identical clean quadrants.
        const int32 ShrubsPerBlock = Stream.RandRange(3, 7);
        for (int32 Index = 0; Index < ShrubsPerBlock; ++Index)
        {
            const bool bXSide = Stream.FRand() < 0.5f;
            const float X = bXSide
                ? (Stream.FRand() < 0.5f ? BlockX0 + 130.0f : BlockX1 - 130.0f)
                : Stream.FRandRange(BlockX0 + 160.0f, BlockX1 - 160.0f);
            const float Y = !bXSide
                ? (Stream.FRand() < 0.5f ? BlockY0 + 130.0f : BlockY1 - 130.0f)
                : Stream.FRandRange(BlockY0 + 160.0f, BlockY1 - 160.0f);
            const float Scale = Stream.FRandRange(0.17f, 0.34f);
            ShrubInstances->AddInstance(
                FTransform(
                    FRotator(0.0f, Stream.FRandRange(-180.0f, 180.0f), 0.0f),
                    FVector(X, Y, GroundZ + 10.0f),
                    FVector(Scale, Scale * Stream.FRandRange(0.72f, 1.20f), Scale * Stream.FRandRange(0.62f, 1.05f))),
                true);
        }

        // Two practical lights per block, offset from corners so silhouettes and fog have alternating
        // pools of warm light. They are intentionally sparse instead of turning the city into a runway.
        SpawnStreetLamp(FVector(BlockX0 + 115.0f, BlockY0 + 115.0f, GroundZ), 45.0f);
        SpawnStreetLamp(FVector(BlockX1 - 115.0f, BlockY1 - 115.0f, GroundZ), -135.0f);
    }

    UE_LOG(LogTemp, Display,
        TEXT("DEADBRICK street dressing: weeds=%d shrubs=%d trash=%d lampPoles=%d"),
        WeedInstances->GetInstanceCount(),
        ShrubInstances->GetInstanceCount(),
        TrashInstances->GetInstanceCount(),
        PoleInstances->GetInstanceCount());
}

void ADeadbrickEnvironmentDirector::SpawnStreetLamp(const FVector& WorldLocation, float YawDegrees)
{
    if (!GetWorld()) return;

    PoleInstances->AddInstance(
        FTransform(FRotator::ZeroRotator, WorldLocation + FVector(0.0f, 0.0f, 280.0f), FVector(0.035f, 0.035f, 2.8f)),
        true);

    const FVector ArmDirection = FRotator(0.0f, YawDegrees, 0.0f).Vector();
    const FVector LampLocation = WorldLocation + FVector(0.0f, 0.0f, 555.0f) + ArmDirection * 42.0f;
    LampHeadInstances->AddInstance(
        FTransform(FRotator(0.0f, YawDegrees, 0.0f), LampLocation, FVector(0.22f, 0.10f, 0.055f)),
        true);

    UPointLightComponent* Practical = NewObject<UPointLightComponent>(this);
    if (!Practical) return;

    AddInstanceComponent(Practical);
    Practical->SetupAttachment(SceneRoot);
    Practical->SetMobility(EComponentMobility::Movable);
    Practical->SetIntensity(1850.0f);
    Practical->SetAttenuationRadius(1050.0f);
    Practical->SetLightColor(FLinearColor(1.0f, 0.53f, 0.19f));
    Practical->SetCastShadows(false);
    Practical->VolumetricScatteringIntensity = 1.65f;
    Practical->RegisterComponent();
    Practical->SetWorldLocation(LampLocation - FVector(0.0f, 0.0f, 18.0f));
}

void ADeadbrickEnvironmentDirector::SpawnLocalAtmospherePockets(
    AProceduralCityGenerator* CityGenerator,
    ADestructibleVoxelWorld* VoxelWorld)
{
    if (!GetWorld() || !CityGenerator || !VoxelWorld) return;

    const float BlockCm = CityGenerator->BlockSizeMeters * 100.0f;
    const float StreetCm = CityGenerator->StreetWidthMeters * 100.0f;
    const float StepCm = BlockCm + StreetCm;
    const float BaseZ = VoxelWorld->GetActorLocation().Z + 115.0f;

    for (int32 BY = 0; BY < CityGenerator->BlocksPerAxis; ++BY)
    for (int32 BX = 0; BX < CityGenerator->BlocksPerAxis; ++BX)
    {
        const FVector BlockCenter(
            StreetCm + BX * StepCm + BlockCm * 0.5f,
            StreetCm + BY * StepCm + BlockCm * 0.5f,
            BaseZ);

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ALocalFogVolume* LocalFog = GetWorld()->SpawnActor<ALocalFogVolume>(
            ALocalFogVolume::StaticClass(), BlockCenter, FRotator::ZeroRotator, Params);
        if (!LocalFog) continue;

        LocalFog->SetActorScale3D(FVector(10.5f));
        if (ULocalFogVolumeComponent* Component = LocalFog->GetComponent())
        {
            Component->SetRadialFogExtinction(0.012f);
            Component->SetHeightFogExtinction(0.0065f);
            Component->SetHeightFogFalloff(950.0f);
            Component->SetHeightFogOffset(-0.22f);
            Component->SetFogPhaseG(0.18f);
            Component->SetFogAlbedo(FLinearColor(0.69f, 0.76f, 0.72f));
            Component->SetFogEmissive(FLinearColor(0.0f, 0.0f, 0.0f));
        }
    }
}
