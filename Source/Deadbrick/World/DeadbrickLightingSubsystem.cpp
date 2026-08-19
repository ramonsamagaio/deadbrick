#include "World/DeadbrickLightingSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool UDeadbrickLightingSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

void UDeadbrickLightingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    EnsureLighting(InWorld);
}

void UDeadbrickLightingSubsystem::EnsureLighting(UWorld& World)
{
    ADirectionalLight* Sun = nullptr;
    for (TActorIterator<ADirectionalLight> It(&World); It; ++It)
    {
        Sun = *It;
        break;
    }

    if (!Sun)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Sun = World.SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector::ZeroVector, FRotator(-48.0f, -35.0f, 0.0f), SpawnParams);
    }

    if (Sun)
    {
        Sun->SetActorRotation(FRotator(-48.0f, -35.0f, 0.0f));
        if (UDirectionalLightComponent* SunComponent = Sun->GetComponent())
        {
            SunComponent->SetMobility(EComponentMobility::Movable);
            SunComponent->SetIntensity(8.0f);
            SunComponent->SetLightColor(FLinearColor(1.0f, 0.93f, 0.82f));
            SunComponent->SetAtmosphereSunLight(true);
            SunComponent->SetAtmosphereSunLightIndex(0);
            SunComponent->SetLightSourceAngle(0.5357f);
            SunComponent->SetShadowAmount(1.0f);
        }
    }

    ASkyLight* Sky = nullptr;
    for (TActorIterator<ASkyLight> It(&World); It; ++It)
    {
        Sky = *It;
        break;
    }

    if (!Sky)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Sky = World.SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    }

    if (Sky && Sky->GetLightComponent())
    {
        USkyLightComponent* SkyComponent = Sky->GetLightComponent();
        SkyComponent->SetMobility(EComponentMobility::Movable);
        SkyComponent->SetIntensity(1.25f);
        SkyComponent->SetRealTimeCapture(true);
        SkyComponent->RecaptureSky();
    }

    UE_LOG(LogTemp, Display, TEXT("DEADBRICK lighting ready: movable atmosphere sun + realtime skylight + Lumen project GI."));
}
