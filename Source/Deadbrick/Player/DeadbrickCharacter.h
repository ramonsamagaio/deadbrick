#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DeadbrickCharacter.generated.h"

class UCameraComponent;
class UFirearmComponent;

UCLASS()
class DEADBRICK_API ADeadbrickCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ADeadbrickCharacter();
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> FirstPersonCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UFirearmComponent> Firearm;

private:
    void MoveForward(float Value);
    void MoveRight(float Value);
    void LookYaw(float Value);
    void LookPitch(float Value);
    void Fire();
    void Reload();
};
