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
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
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
    SunLight->SetForwardShadingPriority(10);

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

    DisablePreexistingEnvironment();
    ConfigureLightingAndGrade();
    ConfigureInstanceMaterials();
    PopulateStreetDressing(CityGenerator, VoxelWorld);
    SpawnLocalAtmospherePockets(CityGenerator, VoxelWorld);

    UE_LOG(LogTemp, Display,
        TEXT("DEADBRICK ENVIRONMENT READY | legacy lights suppressed | balanced sun | light volumetric fog | local haze | instanced dressing"));
}

void ADeadbrickEnvironmentDirector::DisablePreexistingEnvironment()
{
    if (!GetWorld()) return;

    TArray<AActor*> Actors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), Actors);

    int32 DisabledDirectional = 0;
    int32 DisabledSky = 0;
    int32 DisabledFog = 0;
    int32 DisabledAtmosphere = 0;
    int32 DisabledPost = 0;

    for (AActor* Actor : Actors)
    {
        if (!Actor || Actor == this) continue;

        if (UDirectionalLightComponent* Directional = Actor->FindComponentByClass<UDirectionalLightComponent>())
        {
            Directional->SetVisibility(false, true);
            Directional->SetIntensity(0.0f);
            Directional->SetForwardShadingPriority(0);
            ++DisabledDirectional;
        }

        if (USkyLightComponent* ExistingSky = Actor->FindComponentByClass<USkyLightComponent>())
        {
            ExistingSky->SetVisibility(false, true);
            ExistingSky->SetIntensity(0.0f);
            ++DisabledSky;
        }

        if (UExponentialHeightFogComponent* ExistingFog = Actor->FindComponentByClass<UExponentialHeightFogComponent>())
        {
            ExistingFog->SetVisibility(false, true);
            ExistingFog->SetFogDensity(0.0f);
            ExistingFog->SetVolumetricFog(false);
            ++DisabledFog;
        }

        if (USkyAtmosphereComponent* ExistingAtmosphere = Actor->FindComponentByClass<USkyAtmosphereComponent>())
        {
            ExistingAtmosphere->SetVisibility(false, true);
            ++DisabledAtmosphere;
        }

        if (UPostProcessComponent* ExistingPost = Actor->FindComponentByClass<UPostProcessComponent>())
        {
            ExistingPost->BlendWeight = 0.0f;
            ExistingPost->Deactivate();
            ++DisabledPost;
        }
    }

    UE_LOG(LogTemp, Display,
        TEXT("DEADBRICK environment takeover | directional=%d sky=%d fog=%d atmosphere=%d post=%d disabled"),
        DisabledDirectional, DisabledSky, DisabledFog, DisabledAtmosphere, DisabledPost);
}

void ADeadbrickEnvironmentDirector::ConfigureLightingAndGrade()
{
    SetActorRotation(FRotator(-32.0f, -30.0f, 0.0f));

    if (SunLight)
    {
        // The previous 18,000 intensity was physically plausible only with a fully calibrated exposure
        // pipeline. In the template map it stacked with an existing sun and blew the image to white.
        SunLight->SetIntensity(7.5f);
        SunLight->SetLightColor(FLinearColor(0.74f, 0.80f, 0.86f));
        SunLight->SetIndirectLightingIntensity(0.70f);
        SunLight->SetAtmosphereSunLight(true);
        SunLight->SetAtmosphereSunLightIndex(0);
        SunLight->SetForwardShadingPriority(10);
        SunLight->SetVolumetricScatteringIntensity(0.65f);
        SunLight->bEnableLightShaftOcclusion = true;
        SunLight->bCastShadowsOnAtmosphere = true;
    }

    if (SkyLight)
    {
        SkyLight->SetIntensity(0.42f);
        SkyLight->SetIndirectLightingIntensity(0.80f);
        SkyLight->SetLightColor(FLinearColor(0.61f, 0.69f, 0.76f));
        SkyLight->SetLowerHemisphereColor(FLinearColor(0.022f, 0.027f, 0.025f));
        SkyLight->SetRealTimeCapture(true);
    }

    if (HeightFog)
    {
        HeightFog->SetFogDensity(0.0042f);
        HeightFog->SetFogHeightFalloff(0.22f);
        HeightFog->SetFogInscatteringColor(FLinearColor(0.10f, 0.14f, 0.15f));
        HeightFog->SetFogMaxOpacity(0.48f);
        HeightFog->SetStartDistance(1050.0f);
        HeightFog->SetSecondFogDensity(0.0018f);
        HeightFog->SetSecondFogHeightFalloff(0.10f);
        HeightFog->SetSecondFogHeightOffset(-120.0f);

        HeightFog->SetVolumetricFog(true);
        HeightFog->SetVolumetricFogScatteringDistribution(0.12f);
        HeightFog->SetVolumetricFogAlbedo(FColor(188, 201, 197));
        HeightFog->SetVolumetricFogExtinctionScale(0.34f);
        HeightFog->SetVolumetricFogDistance(5200.0f);
        HeightFog->SetVolumetricFogStartDistance(450.0f);
        HeightFog->SetVolumetricFogNearFadeInDistance(450.0f);
    }

    if (PostProcess)
    {
        FPostProcessSettings& Settings = PostProcess->Settings;

        Settings.bOverride_AutoExposureBias = true;
        Settings.AutoExposureBias = -0.65f;

        Settings.bOverride_ColorSaturation = true;
        Settings.ColorSaturation = FVector4(0.90f, 0.93f, 0.91f, 1.0f);

        Settings.bOverride_ColorContrast = true;
        Settings.ColorContrast = FVector4(1.04f, 1.04f, 1.03f, 1.0f);

        Settings.bOverride_ColorOffsetShadows = true;
        Settings.ColorOffsetShadows = FVector4(-0.004f, 0.002f, 0.006f, 0.0f);

        Settings.bOverride_ColorGainHighlights = true;
        Settings.ColorGainHighlights = FVector4(1.015f, 1.005f, 0.98f, 1.0f);

        Settings.bOverride_BloomIntensity = true;
        Settings.BloomIntensity = 0.12f;

        Settings.bOverride_VignetteIntensity = true;
        Settings.VignetteIntensity = 0.12f;

        Settings.bOverride_MotionBlurAmount = true;
        Settings.MotionBlurAmount = 0.03f;

        Settings.bOverride_FilmSlope = true;
        Settings.FilmSlope = 0.90f;
        Settings.bOverride_FilmToe = true;
        Settings.FilmToe = 0.46f;
        Settings.bOverride_FilmShoulder = true;
        Settings.FilmShoulder = 0.25f;
    }
}

void ADeadbrickEnvironmentDirector::ConfigureInstanceMaterials()
{
    if (!BasicShapeMaterial) return;

    if (UMaterialInstanceDynamic* WeedMaterial = MakeTintedMaterial(
        this, BasicShapeMaterial, FLinearColor(0.085f, 0.13f, 0.045f), 0.96f))
        WeedInstances->SetMaterial(0, WeedMaterial);

    if (UMaterialInstanceDynamic* ShrubMaterial = MakeTintedMaterial(
        this, BasicShapeMaterial, FLinearColor(0.052f, 0.085f, 0.034f), 0.94f))
        ShrubInstances->SetMaterial(0, ShrubMaterial);

    if (UMaterialInstanceDynamic* TrashMaterial = MakeTintedMaterial(
        this, BasicShapeMaterial, FLinearColor(0.095f, 0.085f, 0.075f), 0.84f))
        TrashInstances->SetMaterial(0, TrashMaterial);

    if (UMaterialInstanceDynamic* PoleMaterial = MakeTintedMaterial(
        this, BasicShapeMaterial, FLinearColor(0.035f, 0.040f, 0.039f), 0.75f))
        PoleInstances->SetMaterial(0, PoleMaterial);

    if (UMaterialInstanceDynamic* LampMaterial = MakeTintedMaterial(
        this, BasicShapeMaterial, FLinearColor(0.14f, 0.11f, 0.065f), 0.62f))
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

    const FVector WorldOrigin = VoxelWorld->GetActorLocation();
    const float BlockCm = CityGenerator->BlockSizeMeters * 100.0f;
    const float StreetCm = CityGenerator->StreetWidthMeters * 100.0f;
    const float StepCm = BlockCm + StreetCm;
    const float GroundZ = WorldOrigin.Z + VoxelWorld->VoxelSizeCm * 1.55f;
    FRandomStream Stream(CityGenerator->Seed ^ 0x5A17C3);

    for (int32 BY = 0; BY < CityGenerator->BlocksPerAxis; ++BY)
    for (int32 BX = 0; BX < CityGenerator->BlocksPerAxis; ++BX)
    {
        const float BlockX0 = WorldOrigin.X + StreetCm + BX * StepCm;
        const float BlockY0 = WorldOrigin.Y + StreetCm + BY * StepCm;
        const float BlockX1 = BlockX0 + BlockCm;
        const float BlockY1 = BlockY0 + BlockCm;

        for (float Along = 180.0f; Along < BlockCm - 180.0f; Along += Stream.FRandRange(280.0f, 450.0f))
        {
            const float Jitter = Stream.FRandRange(-45.0f, 45.0f);
            const float SideInset = Stream.FRandRange(70.0f, 120.0f);
            const float WeedScale = Stream.FRandRange(0.10f, 0.24f);
            const float WeedHeight = Stream.FRandRange(0.28f, 0.55f);

            const FVector Positions[4] =
            {
                FVector(BlockX0 + Along, BlockY0 + SideInset + Jitter, GroundZ),
                FVector(BlockX0 + Along, BlockY1 - SideInset + Jitter, GroundZ),
                FVector(BlockX0 + SideInset + Jitter, BlockY0 + Along, GroundZ),
                FVector(BlockX1 - SideInset + Jitter, BlockY0 + Along, GroundZ)
            };

            for (const FVector& Position : Positions)
            {
                WeedInstances->AddInstance(
                    FTransform(
                        FRotator(Stream.FRandRange(-7.0f, 7.0f), Stream.FRandRange(-180.0f, 180.0f), Stream.FRandRange(-7.0f, 7.0f)),
                        Position,
                        FVector(WeedScale, WeedScale, WeedHeight)),
                    true);

                if (Stream.FRand() < 0.14f)
                {
                    TrashInstances->AddInstance(
                        FTransform(
                            FRotator(Stream.FRandRange(-25.0f, 25.0f), Stream.FRandRange(-180.0f, 180.0f), Stream.FRandRange(-25.0f, 25.0f)),
                            Position + FVector(Stream.FRandRange(-50.0f, 50.0f), Stream.FRandRange(-50.0f, 50.0f), 5.0f),
                            FVector(Stream.FRandRange(0.07f, 0.16f), Stream.FRandRange(0.05f, 0.14f), Stream.FRandRange(0.025f, 0.07f))),
                        true);
                }
            }
        }

        const int32 ShrubsPerBlock = Stream.RandRange(2, 5);
        for (int32 Index = 0; Index < ShrubsPerBlock; ++Index)
        {
            const bool bXSide = Stream.FRand() < 0.5f;
            const float X = bXSide
                ? (Stream.FRand() < 0.5f ? BlockX0 + 135.0f : BlockX1 - 135.0f)
                : Stream.FRandRange(BlockX0 + 170.0f, BlockX1 - 170.0f);
            const float Y = !bXSide
                ? (Stream.FRand() < 0.5f ? BlockY0 + 135.0f : BlockY1 - 135.0f)
                : Stream.FRandRange(BlockY0 + 170.0f, BlockY1 - 170.0f);
            const float Scale = Stream.FRandRange(0.15f, 0.30f);
            ShrubInstances->AddInstance(
                FTransform(
                    FRotator(0.0f, Stream.FRandRange(-180.0f, 180.0f), 0.0f),
                    FVector(X, Y, GroundZ + 8.0f),
                    FVector(Scale, Scale * Stream.FRandRange(0.76f, 1.16f), Scale * Stream.FRandRange(0.65f, 1.0f))),
                true);
        }

        // One real dynamic light per block is enough for depth. A previous two-per-block setup with
        // strong volumetric scattering was needlessly expensive for a prototype district.
        const bool bAlternate = ((BX + BY) & 1) != 0;
        const FVector LampBase = bAlternate
            ? FVector(BlockX1 - 115.0f, BlockY1 - 115.0f, GroundZ)
            : FVector(BlockX0 + 115.0f, BlockY0 + 115.0f, GroundZ);
        SpawnStreetLamp(LampBase, bAlternate ? -135.0f : 45.0f);
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
    Practical->SetIntensity(720.0f);
    Practical->SetAttenuationRadius(720.0f);
    Practical->SetLightColor(FLinearColor(1.0f, 0.50f, 0.17f));
    Practical->SetCastShadows(false);
    Practical->VolumetricScatteringIntensity = 0.15f;
    Practical->RegisterComponent();
    Practical->SetWorldLocation(LampLocation - FVector(0.0f, 0.0f, 18.0f));
}

void ADeadbrickEnvironmentDirector::SpawnLocalAtmospherePockets(
    AProceduralCityGenerator* CityGenerator,
    ADestructibleVoxelWorld* VoxelWorld)
{
    if (!GetWorld() || !CityGenerator || !VoxelWorld) return;

    const FVector WorldOrigin = VoxelWorld->GetActorLocation();
    const float BlockCm = CityGenerator->BlockSizeMeters * 100.0f;
    const float StreetCm = CityGenerator->StreetWidthMeters * 100.0f;
    const float StepCm = BlockCm + StreetCm;
    const float BaseZ = WorldOrigin.Z + 105.0f;

    for (int32 BY = 0; BY < CityGenerator->BlocksPerAxis; ++BY)
    for (int32 BX = 0; BX < CityGenerator->BlocksPerAxis; ++BX)
    {
        const FVector BlockCenter(
            WorldOrigin.X + StreetCm + BX * StepCm + BlockCm * 0.5f,
            WorldOrigin.Y + StreetCm + BY * StepCm + BlockCm * 0.5f,
            BaseZ);

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ALocalFogVolume* LocalFog = GetWorld()->SpawnActor<ALocalFogVolume>(
            ALocalFogVolume::StaticClass(), BlockCenter, FRotator::ZeroRotator, Params);
        if (!LocalFog) continue;

        LocalFog->SetActorScale3D(FVector(7.5f));
        if (ULocalFogVolumeComponent* Component = LocalFog->GetComponent())
        {
            Component->SetRadialFogExtinction(0.0040f);
            Component->SetHeightFogExtinction(0.0022f);
            Component->SetHeightFogFalloff(1050.0f);
            Component->SetHeightFogOffset(-0.18f);
            Component->SetFogPhaseG(0.12f);
            Component->SetFogAlbedo(FLinearColor(0.63f, 0.69f, 0.66f));
            Component->SetFogEmissive(FLinearColor::Black);
        }
    }
}
