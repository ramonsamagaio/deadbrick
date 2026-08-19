#include "DeadbrickGameModeBase.h"
#include "Player/DeadbrickCharacter.h"
#include "World/DestructibleVoxelWorld.h"
#include "World/ProceduralCityGenerator.h"
#include "AI/ZombieCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

ADeadbrickGameModeBase::ADeadbrickGameModeBase()
{
    DefaultPawnClass = ADeadbrickCharacter::StaticClass();
    bStartPlayersAsSpectators = false;
}

void ADeadbrickGameModeBase::StartPlay()
{
    Super::StartPlay();
    BuildPrototypeWorld();

    if (GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            PositionPlayer(It->Get());
        }
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Green,
            TEXT("DEADBRICK runtime prototype: voxel block generated. WASD + mouse, LMB shoots, R reloads."));
    }
}

void ADeadbrickGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
    BuildPrototypeWorld();
    PositionPlayer(NewPlayer);
}

void ADeadbrickGameModeBase::BuildPrototypeWorld()
{
    if (RuntimeVoxelWorld || !GetWorld())
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    RuntimeVoxelWorld = GetWorld()->SpawnActor<ADestructibleVoxelWorld>(
        ADestructibleVoxelWorld::StaticClass(), PrototypeOrigin, FRotator::ZeroRotator, SpawnParams);

    if (!RuntimeVoxelWorld)
    {
        return;
    }

    // Keep the first automatic slice deliberately small while the mesher is still face-per-voxel.
    // The production target remains finer micro-voxels with greedy meshing/streaming.
    RuntimeVoxelWorld->VoxelSizeCm = 20.0f;
    RuntimeVoxelWorld->ChunkSize = 32;

    RuntimeCityGenerator = GetWorld()->SpawnActor<AProceduralCityGenerator>(
        AProceduralCityGenerator::StaticClass(), PrototypeOrigin, FRotator::ZeroRotator, SpawnParams);

    if (!RuntimeCityGenerator)
    {
        return;
    }

    RuntimeCityGenerator->VoxelWorld = RuntimeVoxelWorld;
    RuntimeCityGenerator->Seed = 1337;
    RuntimeCityGenerator->BlocksPerAxis = 1;
    RuntimeCityGenerator->BlockSizeMeters = 18.0f;
    RuntimeCityGenerator->StreetWidthMeters = 8.0f;
    RuntimeCityGenerator->FloorHeightMeters = 3.0f;
    RuntimeCityGenerator->GenerateCity();

    SpawnPrototypeZombies();
}

void ADeadbrickGameModeBase::PositionPlayer(APlayerController* PlayerController) const
{
    if (!PlayerController)
    {
        return;
    }

    APawn* Pawn = PlayerController->GetPawn();
    if (!Pawn)
    {
        return;
    }

    // Spawn over the first street intersection of the generated block.
    Pawn->SetActorLocation(PrototypeOrigin + FVector(400.0f, 400.0f, 220.0f), false, nullptr, ETeleportType::TeleportPhysics);
    Pawn->SetActorRotation(FRotator(0.0f, 45.0f, 0.0f), ETeleportType::TeleportPhysics);
}

void ADeadbrickGameModeBase::SpawnPrototypeZombies()
{
    if (!GetWorld())
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    const FVector SpawnOffsets[] =
    {
        FVector(500.0f, 1450.0f, 180.0f),
        FVector(1450.0f, 500.0f, 180.0f),
        FVector(2800.0f, 450.0f, 180.0f),
        FVector(450.0f, 2800.0f, 180.0f)
    };

    for (const FVector& Offset : SpawnOffsets)
    {
        GetWorld()->SpawnActor<AZombieCharacter>(
            AZombieCharacter::StaticClass(), PrototypeOrigin + Offset, FRotator::ZeroRotator, SpawnParams);
    }
}
