#include "Player/DeadbrickCharacter.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Combat/FirearmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Items/DeadbrickCraftingSubsystem.h"
#include "Items/DeadbrickPickupItem.h"
#include "Reference/ReferenceAssetResolver.h"
#include "Save/DeadbrickSaveManagerSubsystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "World/ReferenceDestructibleProp.h"

ADeadbrickCharacter::ADeadbrickCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(-10.0f, 0.0f, 64.0f));
    FirstPersonCamera->bUsePawnControlRotation = true;
    FirstPersonCamera->SetFieldOfView(90.0f);

    ViewModelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ViewModelRoot"));
    ViewModelRoot->SetupAttachment(FirstPersonCamera);

    FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
    FirstPersonArms->SetupAttachment(ViewModelRoot);
    FirstPersonArms->SetOnlyOwnerSee(true);
    FirstPersonArms->SetCastShadow(false);
    FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ViewWeapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ViewWeapon"));
    ViewWeapon->SetupAttachment(ViewModelRoot);
    ViewWeapon->SetOnlyOwnerSee(true);
    ViewWeapon->SetCastShadow(false);
    ViewWeapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ViewWeapon->SetRelativeLocation(FVector(48.0f, 13.0f, -21.0f));
    ViewWeapon->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
    ViewWeapon->SetRelativeScale3D(FVector(0.42f, 0.065f, 0.07f));

    FallbackRightArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackRightArm"));
    FallbackRightArm->SetupAttachment(ViewModelRoot);
    FallbackRightArm->SetOnlyOwnerSee(true);
    FallbackRightArm->SetCastShadow(false);
    FallbackRightArm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FallbackRightArm->SetRelativeLocation(FVector(30.0f, 18.0f, -31.0f));
    FallbackRightArm->SetRelativeRotation(FRotator(-8.0f, -5.0f, 0.0f));
    FallbackRightArm->SetRelativeScale3D(FVector(0.28f, 0.055f, 0.065f));

    FallbackLeftArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallbackLeftArm"));
    FallbackLeftArm->SetupAttachment(ViewModelRoot);
    FallbackLeftArm->SetOnlyOwnerSee(true);
    FallbackLeftArm->SetCastShadow(false);
    FallbackLeftArm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FallbackLeftArm->SetRelativeLocation(FVector(31.0f, -10.0f, -29.0f));
    FallbackLeftArm->SetRelativeRotation(FRotator(-10.0f, 8.0f, 0.0f));
    FallbackLeftArm->SetRelativeScale3D(FVector(0.25f, 0.05f, 0.06f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        ViewWeapon->SetStaticMesh(CubeMesh.Object);
        FallbackRightArm->SetStaticMesh(CubeMesh.Object);
        FallbackLeftArm->SetStaticMesh(CubeMesh.Object);
    }

    Firearm = CreateDefaultSubobject<UFirearmComponent>(TEXT("Firearm"));

    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    bUseControllerRotationYaw = true;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = false;
    Movement->MaxWalkSpeed = WalkSpeed;
    Movement->MaxAcceleration = 4200.0f;
    Movement->BrakingDecelerationWalking = 5200.0f;
    Movement->GroundFriction = 12.0f;
    Movement->bUseSeparateBrakingFriction = true;
    Movement->BrakingFriction = 12.0f;
    Movement->BrakingFrictionFactor = 1.0f;
    Movement->BrakingSubStepTime = 1.0f / 60.0f;
    Movement->MinAnalogWalkSpeed = 0.0f;
    Movement->MaxStepHeight = 45.0f;
    Movement->bUseFlatBaseForFloorChecks = true;
    Movement->AirControl = 0.12f;
    Movement->FallingLateralFriction = 0.05f;
    Movement->GravityScale = 1.35f;
    Movement->JumpZVelocity = 430.0f;
}

void ADeadbrickCharacter::BeginPlay()
{
    Super::BeginPlay();
    Health = MaxHealth;
    SetSafeSpawnTransform(GetActorLocation(), GetActorRotation());
    TryApplyReferenceVisuals();
}

void ADeadbrickCharacter::SetSafeSpawnTransform(const FVector& Location, const FRotator& Rotation)
{
    SafeSpawnLocation = Location;
    SafeSpawnRotation = Rotation;
    bHasSafeSpawn = true;
}

void ADeadbrickCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateGroundBraking();
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
    PlayerInputComponent->BindAction("QuickCraft", IE_Pressed, this, &ADeadbrickCharacter::QuickCraft);
    PlayerInputComponent->BindAction("QuickSave", IE_Pressed, this, &ADeadbrickCharacter::QuickSave);
    PlayerInputComponent->BindAction("QuickLoad", IE_Pressed, this, &ADeadbrickCharacter::QuickLoad);
}

float ADeadbrickCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (bDead) return 0.0f;

    const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    Health = FMath::Clamp(Health - Applied, 0.0f, MaxHealth);

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red, FString::Printf(TEXT("Health %.0f / %.0f"), Health, MaxHealth));

    if (Health <= 0.0f)
    {
        bDead = true;
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("YOU DIED"));

        if (GetWorld())
        {
            FTimerHandle RespawnTimer;
            GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &ADeadbrickCharacter::RespawnAtSafeLocation, 2.0f, false);
        }
    }
    return Applied;
}

void ADeadbrickCharacter::MoveForward(float Value)
{
    ForwardInput = FMath::Abs(Value) < 0.01f ? 0.0f : Value;
    if (bDead || ForwardInput == 0.0f || !Controller) return;
    const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
    AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), ForwardInput);
}

void ADeadbrickCharacter::MoveRight(float Value)
{
    RightInput = FMath::Abs(Value) < 0.01f ? 0.0f : Value;
    if (bDead || RightInput == 0.0f || !Controller) return;
    const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
    AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), RightInput);
}

void ADeadbrickCharacter::LookYaw(float Value) { AddControllerYawInput(Value); }
void ADeadbrickCharacter::LookPitch(float Value) { AddControllerPitchInput(Value); }

void ADeadbrickCharacter::Fire()
{
    if (bDead) return;
    if (FirstPersonCamera && Firearm)
        Firearm->FireFromCamera(FirstPersonCamera->GetComponentLocation(), FirstPersonCamera->GetForwardVector());
}

void ADeadbrickCharacter::Reload()
{
    if (!bDead && Firearm) Firearm->Reload();
}

void ADeadbrickCharacter::SprintPressed()
{
    if (!bDead && GetCharacterMovement()) GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void ADeadbrickCharacter::SprintReleased()
{
    if (GetCharacterMovement()) GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void ADeadbrickCharacter::Interact()
{
    if (bDead || !GetWorld() || !FirstPersonCamera) return;

    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End = Start + FirstPersonCamera->GetForwardVector() * 320.0f;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DeadbrickInteract), false, this);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) return;

    if (ADeadbrickPickupItem* Pickup = Cast<ADeadbrickPickupItem>(Hit.GetActor()))
    {
        EDeadbrickItemType Type;
        const int32 Quantity = Pickup->Collect(Type);
        if (Quantity > 0)
        {
            AddInventoryItem(Type, Quantity);
            if (GEngine)
            {
                const UEnum* Enum = StaticEnum<EDeadbrickItemType>();
                const FString Name = Enum ? Enum->GetNameStringByValue((int64)Type) : TEXT("Item");
                GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, FString::Printf(TEXT("Picked up %d x %s"), Quantity, *Name));
            }
        }
        return;
    }

    if (AReferenceDestructibleProp* Prop = Cast<AReferenceDestructibleProp>(Hit.GetActor()))
        Prop->Interact(this);
}

void ADeadbrickCharacter::QuickCraft()
{
    if (bDead || !GetWorld()) return;
    if (UDeadbrickCraftingSubsystem* Crafting = GetWorld()->GetSubsystem<UDeadbrickCraftingSubsystem>())
        Crafting->TryCraft(this, EDeadbrickRecipe::MetalPlate, true);
}

void ADeadbrickCharacter::QuickSave()
{
    if (GetWorld())
        if (UDeadbrickSaveManagerSubsystem* SaveManager = GetWorld()->GetSubsystem<UDeadbrickSaveManagerSubsystem>()) SaveManager->SaveCurrentWorld();
}

void ADeadbrickCharacter::QuickLoad()
{
    if (GetWorld())
        if (UDeadbrickSaveManagerSubsystem* SaveManager = GetWorld()->GetSubsystem<UDeadbrickSaveManagerSubsystem>()) SaveManager->LoadCurrentWorld();
}

int32 ADeadbrickCharacter::GetInventoryQuantity(EDeadbrickItemType ItemType) const
{
    if (const int32* Found = Inventory.Find(ItemType)) return *Found;
    return 0;
}

void ADeadbrickCharacter::AddInventoryItem(EDeadbrickItemType ItemType, int32 Quantity)
{
    if (Quantity > 0) Inventory.FindOrAdd(ItemType) += Quantity;
}

bool ADeadbrickCharacter::ConsumeInventoryItem(EDeadbrickItemType ItemType, int32 Quantity)
{
    if (Quantity <= 0) return true;
    int32* Found = Inventory.Find(ItemType);
    if (!Found || *Found < Quantity) return false;
    *Found -= Quantity;
    if (*Found <= 0) Inventory.Remove(ItemType);
    return true;
}

void ADeadbrickCharacter::TryApplyReferenceVisuals()
{
    FString MeshPath;
    USkeletalMesh* ReferenceMesh = DeadbrickReferenceAssets::FindSkeletalMesh(
        {TEXT("player"), TEXT("maincharacter"), TEXT("character"), TEXT("human"), TEXT("male"), TEXT("female")}, &MeshPath);

    if (ReferenceMesh && GetMesh())
    {
        GetMesh()->SetSkeletalMesh(ReferenceMesh);
        GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
        GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        GetMesh()->SetOwnerNoSee(true);
        GetMesh()->SetCastHiddenShadow(true);

        USkeleton* Skeleton = ReferenceMesh->GetSkeleton();
        IdleAnimation = DeadbrickReferenceAssets::FindAnimationForSkeleton(Skeleton, {TEXT("idle"), TEXT("stand"), TEXT("breath")});
        WalkAnimation = DeadbrickReferenceAssets::FindAnimationForSkeleton(Skeleton, {TEXT("walk"), TEXT("run"), TEXT("locomotion")});
        UE_LOG(LogTemp, Display, TEXT("DEADBRICK player reference visual bound: %s"), *MeshPath);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("DEADBRICK player: no editor-valid reference body mesh yet; gameplay capsule remains authoritative."));
    }

    if (FirstPersonArms)
    {
        USkeletalMesh* ArmsMesh = DeadbrickReferenceAssets::FindSkeletalMesh(
            {TEXT("firstperson"), TEXT("first_person"), TEXT("arms"), TEXT("hands"), TEXT("arm"), TEXT("hand")});
        if (ArmsMesh)
        {
            FirstPersonArms->SetSkeletalMesh(ArmsMesh);
            FirstPersonArms->SetRelativeLocation(FVector(10.0f, 0.0f, -32.0f));
            FirstPersonArms->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
            FallbackLeftArm->SetVisibility(false, true);
            FallbackRightArm->SetVisibility(false, true);
            UE_LOG(LogTemp, Display, TEXT("DEADBRICK first-person arms bound: %s"), *ArmsMesh->GetPathName());
        }
    }

    if (ViewWeapon)
    {
        UStaticMesh* WeaponMesh = DeadbrickReferenceAssets::FindStaticMesh(
            {TEXT("rifle"), TEXT("pistol"), TEXT("shotgun"), TEXT("gun"), TEXT("firearm"), TEXT("weapon")});
        if (WeaponMesh)
        {
            ViewWeapon->SetStaticMesh(WeaponMesh);
            ViewWeapon->SetRelativeScale3D(FVector(1.0f));
            UE_LOG(LogTemp, Display, TEXT("DEADBRICK first-person weapon bound: %s"), *WeaponMesh->GetPathName());
        }
    }
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

void ADeadbrickCharacter::UpdateGroundBraking()
{
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!Movement || !Movement->IsMovingOnGround()) return;

    const bool bHasInput = FMath::Abs(ForwardInput) > 0.01f || FMath::Abs(RightInput) > 0.01f;
    if (bHasInput) return;

    FVector Velocity = Movement->Velocity;
    if (Velocity.SizeSquared2D() <= FMath::Square(85.0f))
    {
        Velocity.X = 0.0f;
        Velocity.Y = 0.0f;
        Movement->Velocity = Velocity;
    }
}

void ADeadbrickCharacter::RecoverFromFall()
{
    if (!bHasSafeSpawn || bDead) return;
    if (GetActorLocation().Z < SafeSpawnLocation.Z - 2200.0f)
    {
        GetCharacterMovement()->StopMovementImmediately();
        SetActorLocationAndRotation(SafeSpawnLocation, SafeSpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
    }
}

void ADeadbrickCharacter::RespawnAtSafeLocation()
{
    if (!bHasSafeSpawn) return;

    bDead = false;
    Health = MaxHealth;
    ForwardInput = 0.0f;
    RightInput = 0.0f;
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->StopMovementImmediately();
    SetActorLocationAndRotation(SafeSpawnLocation, SafeSpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
    if (Controller) Controller->SetControlRotation(SafeSpawnRotation);
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Respawned"));
}
