#include "World/DeadbrickRuntimeBootstrapSubsystem.h"
#include "World/DeadbrickEnvironmentDirector.h"
#include "World/DestructibleVoxelWorld.h"
#include "World/ProceduralCityGenerator.h"
#include "Player/DeadbrickCharacter.h"
#include "AI/ZombieCharacter.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Physics/DeadbrickPhysXSubsystem.h"
#include "Reference/ReferenceAssetResolver.h"
#include "TimerManager.h"

bool UDeadbrickRuntimeBootstrapSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

void UDeadbrickRuntimeBootstrapSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    ShowStatus(TEXT("DEADBRICK: building expanded destructible urban district..."), FColor::Yellow);

    if (UDeadbrickPhysXSubsystem* PhysX = InWorld.GetSubsystem<UDeadbrickPhysXSubsystem>())
    {
        if (PhysX->IsPhysXReady())
            ShowStatus(TEXT("PHYSX 5.8 ACTIVE: detached voxel structures are simulated outside Chaos at fixed 60 Hz."), FColor::Green);
        else
            ShowStatus(TEXT("PHYSX 5.8 NOT ACTIVE: run REBUILD_AND_OPEN_UE58.bat so the pinned SDK is installed before compilation."), FColor::Red);
    }

    if (DeadbrickReferenceAssets::HasCookedReferenceAssets())
        ShowStatus(TEXT("LOTL reference content detected: skins, animations, props and materials are being auto-bound."), FColor::Cyan);
    else
        ShowStatus(TEXT("LOTL reference assets are not editor-valid yet: gameplay uses structured fallbacks while the import pipeline remains available."), FColor::Orange);

    BuildPrototypeWorld();

    PlayerSetupAttempts = 0;
    InWorld.GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateUObject(this, &UDeadbrickRuntimeBootstrapSubsystem::EnsurePlayer));
}

void UDeadbrickRuntimeBootstrapSubsystem::BuildPrototypeWorld()
{
    UWorld* World = GetWorld();
    if (!World || RuntimeVoxelWorld.IsValid()) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ADestructibleVoxelWorld* VoxelWorld = World->SpawnActor<ADestructibleVoxelWorld>(
        ADestructibleVoxelWorld::StaticClass(), PrototypeOrigin, FRotator::ZeroRotator, SpawnParams);

    if (!VoxelWorld)
    {
        ShowStatus(TEXT("DEADBRICK bootstrap FAILED: could not spawn voxel world."), FColor::Red);
        return;
    }

    RuntimeVoxelWorld = VoxelWorld;
    VoxelWorld->VoxelSizeCm = 20.0f;
    VoxelWorld->ChunkSize = 32;
    VoxelWorld->bEnableStructuralGravity = true;
    VoxelWorld->bSpawnSalvageDrops = true;
    VoxelWorld->MaxPhysicsIslandVoxels = 24;

    // Interactive destruction is intentionally amortized. Rebuilding a 32^3 procedural chunk and
    // scanning thousands of support voxels in the same shot frame was the main CPU hitch candidate.
    VoxelWorld->MaxPhysicsBodiesPerCollapse = 128;
    VoxelWorld->PhysicsIslandBuildBudgetPerFrame = 2;
    VoxelWorld->StructuralWorkBudgetPerFrame = 1024;
    VoxelWorld->ChunkRebuildBudgetPerFrame = 1;
    VoxelWorld->SupportCapacityPerGroundVoxel = 320.0f;

    AProceduralCityGenerator* CityGenerator = World->SpawnActor<AProceduralCityGenerator>(
        AProceduralCityGenerator::StaticClass(), PrototypeOrigin, FRotator::ZeroRotator, SpawnParams);

    if (!CityGenerator)
    {
        ShowStatus(TEXT("DEADBRICK bootstrap FAILED: could not spawn city generator."), FColor::Red);
        return;
    }

    RuntimeCityGenerator = CityGenerator;
    CityGenerator->VoxelWorld = VoxelWorld;
    CityGenerator->Seed = 1337;

    CityGenerator->BlocksPerAxis = 2;
    CityGenerator->BlockSizeMeters = 32.0f;
    CityGenerator->StreetWidthMeters = 10.0f;
    CityGenerator->FloorHeightMeters = 3.2f;
    CityGenerator->GenerateCity();
    CityGenerator->ImproveTraversalAndStreetLife();

    if (ADeadbrickEnvironmentDirector* Environment = World->SpawnActor<ADeadbrickEnvironmentDirector>(
        ADeadbrickEnvironmentDirector::StaticClass(), PrototypeOrigin, FRotator::ZeroRotator, SpawnParams))
    {
        Environment->InitializeForCity(CityGenerator, VoxelWorld);
    }

    const FIntVector SpawnVoxel = VoxelWorld->WorldToVoxel(PrototypeOrigin + FVector(400.0f, 400.0f, 240.0f));
    VoxelWorld->BeginBulkEdit();
    for (int32 Z = 1; Z <= 28; ++Z)
    for (int32 Y = -7; Y <= 7; ++Y)
    for (int32 X = -7; X <= 7; ++X)
        VoxelWorld->SetVoxel(FIntVector(SpawnVoxel.X + X, SpawnVoxel.Y + Y, Z), EDeadbrickVoxelMaterial::Air, 0);
    VoxelWorld->EndBulkEdit();

    VoxelWorld->StartRuntimePersistence();

    if (UDeadbrickPhysXSubsystem* PhysX = World->GetSubsystem<UDeadbrickPhysXSubsystem>())
        PhysX->SetGroundHeight(PrototypeOrigin.Z + VoxelWorld->VoxelSizeCm);

    SpawnPrototypeZombies();
    ShowStatus(TEXT("Expanded district ready: balanced atmosphere, destructible interiors, scavenging and amortized destruction are active."), FColor::Green);
}

void UDeadbrickRuntimeBootstrapSubsystem::EnsurePlayer()
{
    UWorld* World = GetWorld();
    if (!World) return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    if (!PC)
    {
        if (++PlayerSetupAttempts < 120)
        {
            World->GetTimerManager().SetTimerForNextTick(
                FTimerDelegate::CreateUObject(this, &UDeadbrickRuntimeBootstrapSubsystem::EnsurePlayer));
        }
        else
        {
            ShowStatus(TEXT("DEADBRICK bootstrap FAILED: no PlayerController after 120 frames."), FColor::Red);
        }
        return;
    }

    ADeadbrickCharacter* DeadbrickPawn = Cast<ADeadbrickCharacter>(PC->GetPawn());
    if (!DeadbrickPawn)
    {
        APawn* OldPawn = PC->GetPawn();
        if (OldPawn)
        {
            PC->UnPossess();
            OldPawn->Destroy();
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = PC;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        DeadbrickPawn = World->SpawnActor<ADeadbrickCharacter>(
            ADeadbrickCharacter::StaticClass(),
            PrototypeOrigin + FVector(400.0f, 400.0f, 600.0f),
            FRotator::ZeroRotator,
            SpawnParams);

        if (!DeadbrickPawn)
        {
            ShowStatus(TEXT("DEADBRICK bootstrap FAILED: could not spawn FPS pawn."), FColor::Red);
            return;
        }

        PC->Possess(DeadbrickPawn);
    }

    const FRotator SpawnRotation(0.0f, 45.0f, 0.0f);
    FVector SpawnLocation = PrototypeOrigin + FVector(400.0f, 400.0f, 220.0f);

    FHitResult GroundHit;
    FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(DeadbrickSpawnGround), false, DeadbrickPawn);
    const FVector TraceStart = PrototypeOrigin + FVector(400.0f, 400.0f, 2000.0f);
    const FVector TraceEnd = PrototypeOrigin + FVector(400.0f, 400.0f, -1000.0f);
    if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, GroundParams))
        SpawnLocation.Z = GroundHit.ImpactPoint.Z + 110.0f;

    DeadbrickPawn->SetActorLocationAndRotation(
        SpawnLocation,
        SpawnRotation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    DeadbrickPawn->SetSafeSpawnTransform(SpawnLocation, SpawnRotation);

    PC->SetControlRotation(SpawnRotation);
    if (DeadbrickPawn->FirstPersonCamera)
        DeadbrickPawn->FirstPersonCamera->Activate(true);
    PC->SetViewTargetWithBlend(DeadbrickPawn, 0.0f);

    PC->SetInputMode(FInputModeGameOnly());
    PC->bShowMouseCursor = false;

    UE_LOG(LogTemp, Display, TEXT("DEADBRICK FPS CAMERA READY | Pawn=%s | Camera=%s | Location=%s | Rotation=%s"),
        *GetNameSafe(DeadbrickPawn),
        *GetNameSafe(DeadbrickPawn->FirstPersonCamera),
        *SpawnLocation.ToCompactString(),
        *SpawnRotation.ToCompactString());

    ShowStatus(TEXT("WASD move | Shift sprint | Space jump | LMB infinite fire | E loot/gather | C craft | F5 save | F9 load"), FColor::Green);
}

void UDeadbrickRuntimeBootstrapSubsystem::SpawnPrototypeZombies()
{
    UWorld* World = GetWorld();
    if (!World) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    const FVector SpawnOffsets[] =
    {
        FVector(500.0f, 1450.0f, 180.0f),
        FVector(1450.0f, 500.0f, 180.0f),
        FVector(2600.0f, 450.0f, 180.0f),
        FVector(450.0f, 2600.0f, 180.0f),
        FVector(3600.0f, 1600.0f, 180.0f),
        FVector(1700.0f, 3600.0f, 180.0f),
        FVector(4800.0f, 900.0f, 180.0f),
        FVector(900.0f, 4800.0f, 180.0f)
    };

    for (const FVector& Offset : SpawnOffsets)
    {
        World->SpawnActor<AZombieCharacter>(
            AZombieCharacter::StaticClass(), PrototypeOrigin + Offset, FRotator::ZeroRotator, SpawnParams);
    }
}

void UDeadbrickRuntimeBootstrapSubsystem::ShowStatus(const FString& Message, const FColor& Color) const
{
    UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 12.0f, Color, Message);
}
