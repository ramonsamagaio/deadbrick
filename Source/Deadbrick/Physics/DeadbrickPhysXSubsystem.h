#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeadbrickPhysXSubsystem.generated.h"

UCLASS()
class DEADBRICK_API UDeadbrickPhysXSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

    bool IsPhysXReady() const { return bPhysXReady; }

    // Creates a PhysX-owned dynamic box. VisualActor is kept non-simulating in Chaos and is
    // synchronized from the PhysX pose every fixed step. LocalCenter is relative to VisualActor.
    int64 CreateDynamicBox(
        AActor* VisualActor,
        const FVector& LocalCenter,
        const FVector& HalfExtentsCm,
        float MassKg,
        float LinearDamping = 0.08f,
        float AngularDamping = 0.25f);

    void DestroyBody(int64 Handle);
    void SetGroundHeight(float GroundHeightCm);

private:
    void* PhysXState = nullptr;
    bool bPhysXReady = false;
};
