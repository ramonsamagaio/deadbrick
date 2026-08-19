#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeadbrickLightingSubsystem.generated.h"

UCLASS()
class DEADBRICK_API UDeadbrickLightingSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
    void EnsureLighting(UWorld& World);
};
