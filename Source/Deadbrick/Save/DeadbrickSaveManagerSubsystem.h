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
    bool SaveCurrentWorld();

    UFUNCTION(BlueprintCallable, Category="Save")
    bool LoadCurrentWorld();

    UFUNCTION(BlueprintPure, Category="Save")
    bool HasSave() const;
};
