#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Items/DeadbrickItemTypes.h"
#include "DeadbrickHUD.generated.h"

UCLASS()
class DEADBRICK_API ADeadbrickHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    FLinearColor ItemColor(EDeadbrickItemType Type) const;
    FString ItemShortName(EDeadbrickItemType Type) const;
};
