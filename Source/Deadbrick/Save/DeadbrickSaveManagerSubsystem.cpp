#include "Save/DeadbrickSaveManagerSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DeadbrickCharacter.h"
#include "Save/DeadbrickSaveGame.h"
#include "World/DestructibleVoxelWorld.h"

bool UDeadbrickSaveManagerSubsystem::HasSave() const
{
    return UGameplayStatics::DoesSaveGameExist(DefaultSlotName, 0);
}

bool UDeadbrickSaveManagerSubsystem::SaveCurrentWorld()
{
    UWorld* World = GetWorld();
    if (!World) return false;

    UDeadbrickSaveGame* Save = Cast<UDeadbrickSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UDeadbrickSaveGame::StaticClass()));
    if (!Save) return false;

    for (TActorIterator<ADestructibleVoxelWorld> It(World); It; ++It)
    {
        It->ExportRuntimeEdits(Save->VoxelEdits);
        break;
    }

    if (ADeadbrickCharacter* Player = Cast<ADeadbrickCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)))
    {
        Save->PlayerLocation = Player->GetActorLocation();
        Save->PlayerRotation = Player->GetActorRotation();
        Save->PlayerHealth = Player->Health;
        Save->Inventory.Reset();
        for (const TPair<EDeadbrickItemType, int32>& Pair : Player->Inventory)
        {
            if (Pair.Value <= 0) continue;
            FDeadbrickInventorySaveRecord Record;
            Record.ItemType = Pair.Key;
            Record.Quantity = Pair.Value;
            Save->Inventory.Add(Record);
        }
    }

    const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, DefaultSlotName, 0);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, bSaved ? FColor::Green : FColor::Red,
            bSaved ? TEXT("DEADBRICK saved") : TEXT("DEADBRICK save failed"));
    }
    return bSaved;
}

bool UDeadbrickSaveManagerSubsystem::LoadCurrentWorld()
{
    UWorld* World = GetWorld();
    if (!World) return false;

    UDeadbrickSaveGame* Save = Cast<UDeadbrickSaveGame>(UGameplayStatics::LoadGameFromSlot(DefaultSlotName, 0));
    if (!Save) return false;

    for (TActorIterator<ADestructibleVoxelWorld> It(World); It; ++It)
    {
        It->ApplyRuntimeEdits(Save->VoxelEdits);
        break;
    }

    if (ADeadbrickCharacter* Player = Cast<ADeadbrickCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)))
    {
        Player->Inventory.Reset();
        for (const FDeadbrickInventorySaveRecord& Record : Save->Inventory)
            Player->AddInventoryItem(Record.ItemType, Record.Quantity);

        Player->Health = FMath::Clamp(Save->PlayerHealth, 1.0f, Player->MaxHealth);
        Player->GetCharacterMovement()->StopMovementImmediately();
        Player->SetActorLocationAndRotation(Save->PlayerLocation, Save->PlayerRotation, false, nullptr, ETeleportType::TeleportPhysics);
    }

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("DEADBRICK loaded"));
    return true;
}
