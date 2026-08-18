#include "Player/DeadbrickCharacter.h"
#include "Camera/CameraComponent.h"
#include "Combat/FirearmComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADeadbrickCharacter::ADeadbrickCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(-10.0f, 0.0f, 64.0f));
    FirstPersonCamera->bUsePawnControlRotation = true;

    Firearm = CreateDefaultSubobject<UFirearmComponent>(TEXT("Firearm"));
    GetCharacterMovement()->MaxWalkSpeed = 500.0f;
}

void ADeadbrickCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis("MoveForward", this, &ADeadbrickCharacter::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this, &ADeadbrickCharacter::MoveRight);
    PlayerInputComponent->BindAxis("Turn", this, &ADeadbrickCharacter::LookYaw);
    PlayerInputComponent->BindAxis("LookUp", this, &ADeadbrickCharacter::LookPitch);
    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
    PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
    PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ADeadbrickCharacter::Fire);
    PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &ADeadbrickCharacter::Reload);
}

void ADeadbrickCharacter::MoveForward(float Value)
{
    if (!FMath::IsNearlyZero(Value)) AddMovementInput(GetActorForwardVector(), Value);
}

void ADeadbrickCharacter::MoveRight(float Value)
{
    if (!FMath::IsNearlyZero(Value)) AddMovementInput(GetActorRightVector(), Value);
}

void ADeadbrickCharacter::LookYaw(float Value) { AddControllerYawInput(Value); }
void ADeadbrickCharacter::LookPitch(float Value) { AddControllerPitchInput(Value); }

void ADeadbrickCharacter::Fire()
{
    if (FirstPersonCamera && Firearm)
    {
        Firearm->FireFromCamera(FirstPersonCamera->GetComponentLocation(), FirstPersonCamera->GetForwardVector());
    }
}

void ADeadbrickCharacter::Reload()
{
    if (Firearm) Firearm->Reload();
}
