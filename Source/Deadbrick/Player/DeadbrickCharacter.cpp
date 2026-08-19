#include "Player/DeadbrickCharacter.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Combat/FirearmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Items/DeadbrickPickupItem.h"
#include "Reference/ReferenceAssetResolver.h"

ADeadbrickCharacter::ADeadbrickCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(-10.0f, 0.0f, 64.0f));
    FirstPersonCamera->bUsePawnControlRotation = true;
    FirstPersonCamera->SetFieldOfView(90.0f);

    Firearm = CreateDefaultSubobject<UFirearmComponent>(TEXT("Firearm"));

    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    bUseControllerRotationYaw = true;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = false;
    Movement->MaxWalkSpeed = WalkSpeed;
    Movement->MaxAcceleration = 2600.0f;
    Movement->BrakingDecelerationWalking = 2400.0f;
    Movement->GroundFriction = 8.0f;
    Movement->BrakingFrictionFactor = 2.0f;
    Movement->AirControl = 0.25f;
    Movement->GravityScale = 1.0f;
}

void ADeadbrickCharacter::BeginPlay()
{
    Super::BeginPlay();
    SafeSpawnLocation = GetActorLocation();
    SafeSpawnRotation = GetActorRotation();
    bHasSafeSpawn = true;
    TryApplyReferenceVisuals();
}

void ADeadbrickCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RecoverFromFall();
    UpdateReferenceAnimation();
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
    PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ADeadbrickCharacter::Interact);
    PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &ADeadbrickCharacter::SprintPressed);
    PlayerInputComponent->BindAction("Sprint", IE_Released, this, &ADeadbrickCharacter::SprintReleased);
}

void ADeadbrickCharacter::MoveForward(float Value)
{
    if (FMath::IsNearlyZero(Value) || !Controller) return;
    const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
    AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
}

void ADeadbrickCharacter::MoveRight(float Value)
{
    if (FMath::IsNearlyZero(Value) || !Controller) return;
    const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
    AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
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

void ADeadbrickCharacter::SprintPressed()
{
    if (GetCharacterMovement()) GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void ADeadbrickCharacter::SprintReleased()
{
    if (GetCharacterMovement()) GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void ADeadbrickCharacter::Interact()
{
    if (!GetWorld() || !FirstPersonCamera) return;

    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End = Start + FirstPersonCamera->GetForwardVector() * 320.0f;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DeadbrickInteract), false, this);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) return;

    ADeadbrickPickupItem* Pickup = Cast<ADeadbrickPickupItem>(Hit.GetActor());
    if (!Pickup) return;

    EDeadbrickItemType Type;
    const int32 Quantity = Pickup->Collect(Type);
    if (Quantity <= 0) return;

    Inventory.FindOrAdd(Type) += Quantity;
    if (GEngine)
    {
        const UEnum* Enum = StaticEnum<EDeadbrickItemType>();
        const FString Name = Enum ? Enum->GetNameStringByValue((int64)Type) : TEXT("Item");
        GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, FString::Printf(TEXT("Picked up %d x %s"), Quantity, *Name));
    }
}

int32 ADeadbrickCharacter::GetInventoryQuantity(EDeadbrickItemType ItemType) const
{
    if (const int32* Found = Inventory.Find(ItemType)) return *Found;
    return 0;
}

void ADeadbrickCharacter::TryApplyReferenceVisuals()
{
    FString MeshPath;
    USkeletalMesh* ReferenceMesh = DeadbrickReferenceAssets::FindSkeletalMesh(
        {TEXT("player"), TEXT("maincharacter"), TEXT("character"), TEXT("human"), TEXT("male"), TEXT("female")},
        &MeshPath);

    if (!ReferenceMesh || !GetMesh())
    {
        UE_LOG(LogTemp, Display, TEXT("DEADBRICK player: no compatible cooked reference mesh found; using FPS capsule until reference import succeeds."));
        return;
    }

    GetMesh()->SetSkeletalMesh(ReferenceMesh);
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    GetMesh()->SetOwnerNoSee(true);
    GetMesh()->SetCastHiddenShadow(true);

    USkeleton* Skeleton = ReferenceMesh->GetSkeleton();
    IdleAnimation = DeadbrickReferenceAssets::FindAnimationForSkeleton(
        Skeleton, {TEXT("idle"), TEXT("stand"), TEXT("breath")});
    WalkAnimation = DeadbrickReferenceAssets::FindAnimationForSkeleton(
        Skeleton, {TEXT("walk"), TEXT("run"), TEXT("locomotion")});

    UE_LOG(LogTemp, Display, TEXT("DEADBRICK player reference visual bound: %s"), *MeshPath);
}

void ADeadbrickCharacter::UpdateReferenceAnimation()
{
    if (!GetMesh()) return;

    UAnimSequence* Desired = GetVelocity().SizeSquared2D() > FMath::Square(15.0f) ? WalkAnimation : IdleAnimation;
    if (!Desired || Desired == CurrentAnimation) return;

    CurrentAnimation = Desired;
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    GetMesh()->SetAnimation(Desired);
    GetMesh()->Play(true);
}

void ADeadbrickCharacter::RecoverFromFall()
{
    if (!bHasSafeSpawn) return;

    if (GetActorLocation().Z < SafeSpawnLocation.Z - 2200.0f)
    {
        GetCharacterMovement()->StopMovementImmediately();
        SetActorLocationAndRotation(SafeSpawnLocation, SafeSpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
    }
}
