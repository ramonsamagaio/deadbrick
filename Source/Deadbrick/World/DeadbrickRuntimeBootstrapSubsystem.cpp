#include "World/DeadbrickRuntimeBootstrapSubsystem.h"
#include "World/DestructibleVoxelWorld.h"
#include "World/ProceduralCityGenerator.h"
#include "Player/DeadbrickCharacter.h"
#include "AI/ZombieCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Reference/ReferenceAssetResolver.h"
#include "TimerManager.h"

bool UDeadbrickRuntimeBootstrapSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

void UDeadbrickRuntimeBootstrapSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    ShowStatus(TEXT("DEADBRICK: building procedural destructible urban test district..."), FColor::Yellow);
    if (DeadbrickReferenceAssets::HasCookedReferenceAssets())
        ShowStatus(TEXT("LOTL reference content detected: skins, animations, props and materials are being auto-bound."), FColor::Cyan);
    else
        ShowStatus(TEXT("LOTL reference packages are not readable yet: run IMPORT_LOTL_REFERENCE_UE58.bat."), FColor::Orange);

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
    CityGenerator->BlocksPerAxis = 1;
    CityGenerator->BlockSizeMeters = 24.0f;
    CityGenerator->StreetWidthMeters = 8.0f;
    CityGenerator->FloorHeightMeters = 3.0f;
    CityGenerator->GenerateCity();

    // Everything before this point is deterministic baseline generation. Only gameplay changes after it
    // become save deltas, so saves stay tiny even when the city eventually spans many streamed districts.
    VoxelWorld->StartRuntimePersistence();

    SpawnPrototypeZombies();
    ShowStatus(TEXT("Urban voxel district ready: roads, soil, floors, walls, rooms and stairs are destructible cells."), FColor::Green);
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
            PrototypeOrigin + FVector(400.0f, 400.0f, 240.0f),
            FRotator(0.0f, 45.0f, 0.0f),
            SpawnParams);

        if (!DeadbrickPawn)
        {
            ShowStatus(TEXT("DEADBRICK bootstrap FAILED: could not spawn FPS pawn."), FColor::Red);
            return;
        }

        PC->Possess(DeadbrickPawn);
    }

    DeadbrickPawn->SetActorLocation(
        PrototypeOrigin + FVector(400.0f, 400.0f, 240.0f),
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    DeadbrickPawn->SetActorRotation(FRotator(0.0f, 45.0f, 0.0f), ETeleportType::TeleportPhysics);

    PC->SetInputMode(FInputModeGameOnly());
    PC->bShowMouseCursor = false;

    ShowStatus(TEXT("WASD move | Shift sprint | Space jump | LMB shoot | R reload | E interact/collect | C craft | F5 save | F9 load"), FColor::Green);
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
        FVector(450.0f, 2600.0f, 180.0f)
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
