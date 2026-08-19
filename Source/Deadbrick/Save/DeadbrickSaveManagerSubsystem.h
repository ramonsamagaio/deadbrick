#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeadbrickSaveManagerSubsystem.generated.h"

UCLASS()
class DEADBRICK_API UDeadbrickSaveManagerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Save")
    FString DefaultSlotName = TEXT("DEADBRICK_Autosave");

    UFUNCTION(BlueprintCallable, Category="Save")
    bool SaveCurrentWorld(const FString& SlotName = TEXT(""));

    UFUNCTION(BlueprintCallable, Category="Save")
    bool LoadCurrentWorld(const FString& SlotName = TEXT(""));

    UFUNCTION(BlueprintPure, Category="Save")
    bool HasSave(const FString& SlotName = TEXT("")) const;

private:
    FString ResolveSlot(const FString& SlotName) const;
};
