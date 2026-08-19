#include "Items/DeadbrickCraftingSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Items/DeadbrickPickupItem.h"
#include "Player/DeadbrickCharacter.h"

void UDeadbrickCraftingSubsystem::GetRecipe(
    EDeadbrickRecipe Recipe,
    TMap<EDeadbrickItemType, int32>& OutIngredients,
    EDeadbrickItemType& OutProduct,
    int32& OutQuantity) const
{
    OutIngredients.Reset();
    OutQuantity = 1;

    switch (Recipe)
    {
        case EDeadbrickRecipe::Bandage:
            OutIngredients.Add(EDeadbrickItemType::Cloth, 2);
            OutIngredients.Add(EDeadbrickItemType::MedicalSupplies, 1);
            OutProduct = EDeadbrickItemType::Bandage;
            break;
        case EDeadbrickRecipe::MetalPlate:
            OutIngredients.Add(EDeadbrickItemType::MetalScrap, 4);
            OutProduct = EDeadbrickItemType::MetalPlate;
            break;
        case EDeadbrickRecipe::RepairKit:
            OutIngredients.Add(EDeadbrickItemType::MetalScrap, 2);
            OutIngredients.Add(EDeadbrickItemType::MechanicalParts, 2);
            OutIngredients.Add(EDeadbrickItemType::Electronics, 1);
            OutProduct = EDeadbrickItemType::RepairKit;
            break;
        case EDeadbrickRecipe::Molotov:
            OutIngredients.Add(EDeadbrickItemType::Fuel, 1);
            OutIngredients.Add(EDeadbrickItemType::Cloth, 1);
            OutIngredients.Add(EDeadbrickItemType::GlassShard, 1);
            OutProduct = EDeadbrickItemType::Molotov;
            break;
        case EDeadbrickRecipe::WoodenBarricade:
            OutIngredients.Add(EDeadbrickItemType::WoodScrap, 6);
            OutIngredients.Add(EDeadbrickItemType::Nails, 4);
            OutProduct = EDeadbrickItemType::WoodenBarricade;
            break;
        case EDeadbrickRecipe::PurifiedWater:
            OutIngredients.Add(EDeadbrickItemType::WaterBottle, 1);
            OutProduct = EDeadbrickItemType::PurifiedWater;
            break;
        default:
            OutProduct = EDeadbrickItemType::WoodScrap;
            break;
    }
}

void UDeadbrickCraftingSubsystem::CollectNearbyPhysicalMaterials(ADeadbrickCharacter* Player, float RadiusCm) const
{
    if (!Player || !GetWorld()) return;

    TArray<ADeadbrickPickupItem*> Nearby;
    for (TActorIterator<ADeadbrickPickupItem> It(GetWorld()); It; ++It)
    {
        ADeadbrickPickupItem* Pickup = *It;
        if (Pickup && FVector::DistSquared(Pickup->GetActorLocation(), Player->GetActorLocation()) <= FMath::Square(RadiusCm))
        {
            Nearby.Add(Pickup);
        }
    }

    for (ADeadbrickPickupItem* Pickup : Nearby)
    {
        if (!IsValid(Pickup)) continue;
        EDeadbrickItemType Type;
        const int32 Quantity = Pickup->Collect(Type);
        if (Quantity > 0) Player->AddInventoryItem(Type, Quantity);
    }
}

bool UDeadbrickCraftingSubsystem::TryCraft(ADeadbrickCharacter* Player, EDeadbrickRecipe Recipe, bool bUseNearbyPhysicalItems)
{
    if (!Player || !GetWorld()) return false;

    if (bUseNearbyPhysicalItems)
    {
        CollectNearbyPhysicalMaterials(Player, 260.0f);
    }

    TMap<EDeadbrickItemType, int32> Ingredients;
    EDeadbrickItemType Product;
    int32 ProductQuantity = 1;
    GetRecipe(Recipe, Ingredients, Product, ProductQuantity);

    for (const TPair<EDeadbrickItemType, int32>& Ingredient : Ingredients)
    {
        if (Player->GetInventoryQuantity(Ingredient.Key) < Ingredient.Value)
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange,
                    FString::Printf(TEXT("Missing materials: %s"), *DescribeRecipe(Recipe)));
            }
            return false;
        }
    }

    for (const TPair<EDeadbrickItemType, int32>& Ingredient : Ingredients)
    {
        Player->ConsumeInventoryItem(Ingredient.Key, Ingredient.Value);
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector SpawnLocation = Player->GetActorLocation() + Player->GetActorForwardVector() * 110.0f + FVector(0.0f, 0.0f, 55.0f);
    if (ADeadbrickPickupItem* ProductPickup = GetWorld()->SpawnActor<ADeadbrickPickupItem>(
        ADeadbrickPickupItem::StaticClass(), SpawnLocation, FRotator::ZeroRotator, SpawnParams))
    {
        ProductPickup->InitializeItem(Product, ProductQuantity);
    }

    if (GEngine)
    {
        const UEnum* Enum = StaticEnum<EDeadbrickItemType>();
        const FString ProductName = Enum ? Enum->GetNameStringByValue((int64)Product) : TEXT("Item");
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("Crafted %s"), *ProductName));
    }
    return true;
}

FString UDeadbrickCraftingSubsystem::DescribeRecipe(EDeadbrickRecipe Recipe) const
{
    TMap<EDeadbrickItemType, int32> Ingredients;
    EDeadbrickItemType Product;
    int32 Quantity = 1;
    GetRecipe(Recipe, Ingredients, Product, Quantity);

    const UEnum* ItemEnum = StaticEnum<EDeadbrickItemType>();
    TArray<FString> Parts;
    for (const TPair<EDeadbrickItemType, int32>& Ingredient : Ingredients)
    {
        const FString Name = ItemEnum ? ItemEnum->GetNameStringByValue((int64)Ingredient.Key) : TEXT("Item");
        Parts.Add(FString::Printf(TEXT("%d %s"), Ingredient.Value, *Name));
    }
    return FString::Join(Parts, TEXT(" + "));
}
