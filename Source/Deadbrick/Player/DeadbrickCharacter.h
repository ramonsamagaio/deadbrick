#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
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

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> IdleAnimation;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> WalkAnimation;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> CurrentAnimation;

    FVector SafeSpawnLocation = FVector::ZeroVector;
    FRotator SafeSpawnRotation = FRotator::ZeroRotator;
    bool bHasSafeSpawn = false;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void LookYaw(float Value);
    void LookPitch(float Value);
    void Fire();
    void Reload();
    void TryApplyReferenceVisuals();
    void UpdateReferenceAnimation();
    void RecoverFromFall();
};
