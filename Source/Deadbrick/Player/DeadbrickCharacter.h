#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Items/DeadbrickItemTypes.h"
#include "DeadbrickCharacter.generated.h"

class UAnimSequence;
class UCameraComponent;
class UFirearmComponent;

UCLASS()
class DEADBRICK_API ADeadbrickCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ADeadbrickCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> FirstPersonCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UFirearmComponent> Firearm;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    TMap<EDeadbrickItemType, int32> Inventory;

    UFUNCTION(BlueprintPure, Category="Inventory")
    int32 GetInventoryQuantity(EDeadbrickItemType ItemType) const;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> IdleAnimation;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> WalkAnimation;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> CurrentAnimation;

    FVector SafeSpawnLocation = FVector::ZeroVector;
    FRotator SafeSpawnRotation = FRotator::ZeroRotator;
    bool bHasSafeSpawn = false;

    float WalkSpeed = 500.0f;
    float SprintSpeed = 760.0f;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void LookYaw(float Value);
    void LookPitch(float Value);
    void Fire();
    void Reload();
    void Interact();
    void SprintPressed();
    void SprintReleased();
    void TryApplyReferenceVisuals();
    void UpdateReferenceAnimation();
    void RecoverFromFall();
};
