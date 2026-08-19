#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ReferenceCityDecorationSubsystem.generated.h"

UCLASS()
class DEADBRICK_API UDeadbrickReferenceCityDecorationSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
    FTimerHandle DecorationTimer;
    void DecorateReferenceProps();
};
