#include "UI/DeadbrickHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Player/DeadbrickCharacter.h"

namespace
{
    struct FInventoryEntry
    {
        EDeadbrickItemType Type = EDeadbrickItemType::ConcreteRubble;
        int32 Quantity = 0;
    };
}

void ADeadbrickHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !PlayerOwner) return;

    ADeadbrickCharacter* Player = Cast<ADeadbrickCharacter>(PlayerOwner->GetPawn());
    if (!Player) return;

    const float W = Canvas->ClipX;
    const float H = Canvas->ClipY;

    // Compact crosshair.
    const float CX = W * 0.5f;
    const float CY = H * 0.5f;
    DrawRect(FLinearColor(0.92f, 0.92f, 0.92f, 0.9f), CX - 1.0f, CY - 8.0f, 2.0f, 16.0f);
    DrawRect(FLinearColor(0.92f, 0.92f, 0.92f, 0.9f), CX - 8.0f, CY - 1.0f, 16.0f, 2.0f);

    // Health is always readable without filling the screen with debug text.
    const float HealthRatio = Player->MaxHealth > 0.0f ? FMath::Clamp(Player->Health / Player->MaxHealth, 0.0f, 1.0f) : 0.0f;
    const float HealthX = 28.0f;
    const float HealthY = H - 118.0f;
    DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.72f), HealthX, HealthY, 210.0f, 18.0f);
    DrawRect(FLinearColor(0.62f, 0.08f, 0.06f, 0.96f), HealthX + 2.0f, HealthY + 2.0f, 206.0f * HealthRatio, 14.0f);
    DrawText(FString::Printf(TEXT("HP %.0f"), Player->Health), FLinearColor::White, HealthX + 6.0f, HealthY - 19.0f, GEngine ? GEngine->GetSmallFont() : nullptr, 0.9f, false);

    TArray<FInventoryEntry> Entries;
    Entries.Reserve(Player->Inventory.Num());
    for (const TPair<EDeadbrickItemType, int32>& Pair : Player->Inventory)
    {
        if (Pair.Value <= 0) continue;
        FInventoryEntry Entry;
        Entry.Type = Pair.Key;
        Entry.Quantity = Pair.Value;
        Entries.Add(Entry);
    }

    Entries.Sort([](const FInventoryEntry& A, const FInventoryEntry& B)
    {
        return (uint8)A.Type < (uint8)B.Type;
    });

    constexpr int32 SlotsPerRow = 8;
    constexpr int32 TotalSlots = 16;
    const float Slot = 46.0f;
    const float Gap = 5.0f;
    const float PanelWidth = SlotsPerRow * Slot + (SlotsPerRow - 1) * Gap + 18.0f;
    const float PanelHeight = 2.0f * Slot + Gap + 42.0f;
    const float PanelX = 24.0f;
    const float PanelY = H - PanelHeight - 142.0f;

    DrawRect(FLinearColor(0.015f, 0.018f, 0.022f, 0.76f), PanelX, PanelY, PanelWidth, PanelHeight);
    DrawText(TEXT("GATHERED RESOURCES"), FLinearColor(0.88f, 0.92f, 0.94f, 1.0f), PanelX + 10.0f, PanelY + 8.0f, GEngine ? GEngine->GetSmallFont() : nullptr, 0.86f, false);

    for (int32 SlotIndex = 0; SlotIndex < TotalSlots; ++SlotIndex)
    {
        const int32 Row = SlotIndex / SlotsPerRow;
        const int32 Col = SlotIndex % SlotsPerRow;
        const float X = PanelX + 10.0f + Col * (Slot + Gap);
        const float Y = PanelY + 31.0f + Row * (Slot + Gap);

        DrawRect(FLinearColor(0.06f, 0.065f, 0.075f, 0.92f), X, Y, Slot, Slot);

        if (!Entries.IsValidIndex(SlotIndex)) continue;
        const FInventoryEntry& Entry = Entries[SlotIndex];
        const FLinearColor Color = ItemColor(Entry.Type);

        // The colored inner cube is the provisional icon. It makes resource classes readable instantly
        // while the asset pipeline is still being connected.
        DrawRect(Color, X + 6.0f, Y + 6.0f, 24.0f, 24.0f);
        DrawText(ItemShortName(Entry.Type), FLinearColor::White, X + 5.0f, Y + 31.0f, GEngine ? GEngine->GetTinyFont() : nullptr, 0.66f, false);
        DrawText(FString::Printf(TEXT("x%d"), Entry.Quantity), FLinearColor::White, X + 29.0f, Y + 6.0f, GEngine ? GEngine->GetTinyFont() : nullptr, 0.72f, false);
    }
}

FLinearColor ADeadbrickHUD::ItemColor(EDeadbrickItemType Type) const
{
    switch (Type)
    {
        case EDeadbrickItemType::WoodScrap:
        case EDeadbrickItemType::WoodenBarricade: return FLinearColor(0.46f, 0.24f, 0.08f, 1.0f);
        case EDeadbrickItemType::BrickFragment: return FLinearColor(0.64f, 0.16f, 0.08f, 1.0f);
        case EDeadbrickItemType::ConcreteRubble: return FLinearColor(0.42f, 0.44f, 0.47f, 1.0f);
        case EDeadbrickItemType::GlassShard: return FLinearColor(0.25f, 0.78f, 0.90f, 1.0f);
        case EDeadbrickItemType::MetalScrap:
        case EDeadbrickItemType::MetalPlate: return FLinearColor(0.25f, 0.42f, 0.58f, 1.0f);
        case EDeadbrickItemType::AsphaltChunk: return FLinearColor(0.10f, 0.11f, 0.12f, 1.0f);
        case EDeadbrickItemType::SoilClump: return FLinearColor(0.35f, 0.22f, 0.10f, 1.0f);
        case EDeadbrickItemType::Cloth:
        case EDeadbrickItemType::Bandage: return FLinearColor(0.83f, 0.80f, 0.67f, 1.0f);
        case EDeadbrickItemType::Electronics:
        case EDeadbrickItemType::Battery: return FLinearColor(0.20f, 0.74f, 0.45f, 1.0f);
        case EDeadbrickItemType::Plastic: return FLinearColor(0.58f, 0.34f, 0.76f, 1.0f);
        case EDeadbrickItemType::Wire:
        case EDeadbrickItemType::Nails:
        case EDeadbrickItemType::MechanicalParts: return FLinearColor(0.45f, 0.52f, 0.58f, 1.0f);
        case EDeadbrickItemType::Ammo9mm:
        case EDeadbrickItemType::RifleAmmo:
        case EDeadbrickItemType::ShotgunShells: return FLinearColor(0.92f, 0.48f, 0.08f, 1.0f);
        case EDeadbrickItemType::CannedFood: return FLinearColor(0.72f, 0.50f, 0.12f, 1.0f);
        case EDeadbrickItemType::WaterBottle:
        case EDeadbrickItemType::PurifiedWater: return FLinearColor(0.12f, 0.48f, 0.88f, 1.0f);
        case EDeadbrickItemType::MedicalSupplies:
        case EDeadbrickItemType::RepairKit: return FLinearColor(0.14f, 0.72f, 0.24f, 1.0f);
        case EDeadbrickItemType::Fuel:
        case EDeadbrickItemType::Molotov: return FLinearColor(0.92f, 0.68f, 0.08f, 1.0f);
        default: return FLinearColor(0.65f, 0.65f, 0.68f, 1.0f);
    }
}

FString ADeadbrickHUD::ItemShortName(EDeadbrickItemType Type) const
{
    switch (Type)
    {
        case EDeadbrickItemType::WoodScrap: return TEXT("WOOD");
        case EDeadbrickItemType::BrickFragment: return TEXT("BRICK");
        case EDeadbrickItemType::ConcreteRubble: return TEXT("CONC");
        case EDeadbrickItemType::GlassShard: return TEXT("GLASS");
        case EDeadbrickItemType::MetalScrap: return TEXT("METAL");
        case EDeadbrickItemType::AsphaltChunk: return TEXT("ASPH");
        case EDeadbrickItemType::SoilClump: return TEXT("SOIL");
        case EDeadbrickItemType::Cloth: return TEXT("CLOTH");
        case EDeadbrickItemType::Electronics: return TEXT("ELEC");
        case EDeadbrickItemType::Plastic: return TEXT("PLAST");
        case EDeadbrickItemType::Wire: return TEXT("WIRE");
        case EDeadbrickItemType::Nails: return TEXT("NAILS");
        case EDeadbrickItemType::Ammo9mm: return TEXT("9MM");
        case EDeadbrickItemType::RifleAmmo: return TEXT("RIFLE");
        case EDeadbrickItemType::ShotgunShells: return TEXT("SHELL");
        case EDeadbrickItemType::CannedFood: return TEXT("FOOD");
        case EDeadbrickItemType::WaterBottle: return TEXT("WATER");
        case EDeadbrickItemType::PurifiedWater: return TEXT("PURE");
        case EDeadbrickItemType::MedicalSupplies: return TEXT("MED");
        case EDeadbrickItemType::Bandage: return TEXT("BAND");
        case EDeadbrickItemType::Battery: return TEXT("BATT");
        case EDeadbrickItemType::Fuel: return TEXT("FUEL");
        case EDeadbrickItemType::MechanicalParts: return TEXT("PARTS");
        case EDeadbrickItemType::MetalPlate: return TEXT("PLATE");
        case EDeadbrickItemType::RepairKit: return TEXT("REPAIR");
        case EDeadbrickItemType::Molotov: return TEXT("MOLOT");
        case EDeadbrickItemType::WoodenBarricade: return TEXT("BARR");
        default: return TEXT("ITEM");
    }
}
