#include "AI/ZombieCharacter.h"

#include "AI/ZombieDirectorSubsystem.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Reference/ReferenceAssetResolver.h"
#include "UObject/ConstructorHelpers.h"

AZombieCharacter::AZombieCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->MaxWalkSpeed = 205.0f;
    Movement->MaxAcceleration = 720.0f;
    Movement->BrakingDecelerationWalking = 780.0f;
    Movement->bOrientRotationToMovement = true;
    Movement->RotationRate = FRotator(0.0f, 115.0f, 0.0f);
    bUseControllerRotationYaw = false;

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

    auto ConfigurePart = [&](UStaticMeshComponent* Part, const FVector& Location, const FVector& Scale)
    {
        Part->SetupAttachment(GetRootComponent());
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeScale3D(Scale);
        Part->SetCastShadow(true);
        if (CubeMesh.Succeeded()) Part->SetStaticMesh(CubeMesh.Object);
    };

    PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
    ConfigurePart(PlaceholderBody, FVector(0.0f, 0.0f, 13.0f), FVector(0.34f, 0.25f, 0.48f));

    PlaceholderHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderHead"));
    ConfigurePart(PlaceholderHead, FVector(1.0f, 0.0f, 68.0f), FVector(0.25f, 0.24f, 0.27f));

    PlaceholderLeftArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderLeftArm"));
    ConfigurePart(PlaceholderLeftArm, FVector(6.0f, -31.0f, 23.0f), FVector(0.115f, 0.10f, 0.43f));
    PlaceholderLeftArm->SetRelativeRotation(FRotator(-52.0f, 0.0f, -8.0f));

    PlaceholderRightArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderRightArm"));
    ConfigurePart(PlaceholderRightArm, FVector(6.0f, 31.0f, 23.0f), FVector(0.115f, 0.10f, 0.43f));
    PlaceholderRightArm->SetRelativeRotation(FRotator(-58.0f, 0.0f, 8.0f));

    PlaceholderLeftLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderLeftLeg"));
    ConfigurePart(PlaceholderLeftLeg, FVector(0.0f, -13.0f, -46.0f), FVector(0.14f, 0.13f, 0.45f));

    PlaceholderRightLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderRightLeg"));
    ConfigurePart(PlaceholderRightLeg, FVector(0.0f, 13.0f, -46.0f), FVector(0.14f, 0.13f, 0.45f));
}

void AZombieCharacter::BeginPlay()
{
    Super::BeginPlay();
    Health = MaxHealth;

    FRandomStream VariantStream((int32)(GetUniqueID() ^ 0x71B01D));
    GaitFrequency = VariantStream.FRandRange(4.4f, 6.8f);
    GaitStride = VariantStream.FRandRange(0.72f, 1.22f);
    GaitPhaseOffset = VariantStream.FRandRange(-PI, PI);
    LimpBias = VariantStream.FRandRange(-1.0f, 1.0f);
    ShoulderBias = VariantStream.FRandRange(-7.0f, 7.0f);
    GetCharacterMovement()->MaxWalkSpeed = VariantStream.FRandRange(165.0f, 235.0f);

    ApplyFallbackZombieMaterials();
    TryApplyReferenceVisuals();
}

void AZombieCharacter::AcquireTarget()
{
    CurrentTarget.Reset();
    bHasMoveTarget = false;

    ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
    if (Player && FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(VisualDetectionRadiusCm))
    {
        CurrentTarget = Player;
        MoveTarget = Player->GetActorLocation();
        bHasMoveTarget = true;
        return;
    }

    if (GetWorld())
    {
        if (UDeadbrickZombieDirectorSubsystem* Director = GetWorld()->GetSubsystem<UDeadbrickZombieDirectorSubsystem>())
        {
            float Score = 0.0f;
            FVector NoiseLocation = FVector::ZeroVector;
            if (Director->FindStrongestNoise(GetActorLocation(), HearingRadiusCm, NoiseLocation, Score))
            {
                MoveTarget = NoiseLocation;
                bHasMoveTarget = true;
            }
        }
    }
}

void AZombieCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bDead)
    {
        if (bUsingFallbackZombie) UpdateFallbackDeath(DeltaSeconds);
        return;
    }

    RetargetTimer -= DeltaSeconds;
    AttackTimer -= DeltaSeconds;
    AnimationLockTimer = FMath::Max(0.0f, AnimationLockTimer - DeltaSeconds);

    if (RetargetTimer <= 0.0f)
    {
        RetargetTimer = 0.25f;
        AcquireTarget();
    }

    if (CurrentTarget.IsValid())
    {
        MoveTarget = CurrentTarget->GetActorLocation();
        bHasMoveTarget = true;
    }

    if (bHasMoveTarget)
    {
        const FVector Delta = MoveTarget - GetActorLocation();
        const float Distance = Delta.Size2D();
        if (CurrentTarget.IsValid() && Distance <= AttackDistanceCm)
        {
            if (AttackTimer <= 0.0f)
            {
                AttackTimer = AttackCooldown;
                if (AttackAnimation)
                    PlayReferenceAnimation(AttackAnimation, false, FMath::Min(0.8f, AttackCooldown * 0.75f));

                UGameplayStatics::ApplyDamage(CurrentTarget.Get(), AttackDamage, GetController(), this, nullptr);
            }
        }
        else if (Distance > 30.0f)
        {
            AddMovementInput(Delta.GetSafeNormal2D(), 1.0f);
        }
        else if (!CurrentTarget.IsValid())
        {
            bHasMoveTarget = false;
        }
    }

    if (bUsingFallbackZombie)
        UpdateFallbackAnimation(DeltaSeconds);
    else
        UpdateReferenceAnimation();
}

float AZombieCharacter::TakeDamage(
    float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser)
{
    if (bDead) return 0.0f;

    const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    Health -= Applied;
    if (Applied > 0.0f && bUsingFallbackZombie)
        FallbackHitReact = 1.0f;

    if (Health <= 0.0f)
    {
        bDead = true;
        CurrentTarget.Reset();
        bHasMoveTarget = false;
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        if (DeathAnimation)
        {
            PlayReferenceAnimation(DeathAnimation, false, DeathAnimation->GetPlayLength());
            SetLifeSpan(FMath::Max(2.0f, DeathAnimation->GetPlayLength() + 1.0f));
        }
        else
        {
            FallbackDeathTime = 0.0f;
            SetLifeSpan(3.2f);
        }
    }
    return Applied;
}

void AZombieCharacter::SetFallbackVisible(bool bVisible)
{
    if (PlaceholderBody) PlaceholderBody->SetVisibility(bVisible, true);
    if (PlaceholderHead) PlaceholderHead->SetVisibility(bVisible, true);
    if (PlaceholderLeftArm) PlaceholderLeftArm->SetVisibility(bVisible, true);
    if (PlaceholderRightArm) PlaceholderRightArm->SetVisibility(bVisible, true);
    if (PlaceholderLeftLeg) PlaceholderLeftLeg->SetVisibility(bVisible, true);
    if (PlaceholderRightLeg) PlaceholderRightLeg->SetVisibility(bVisible, true);
}

void AZombieCharacter::ApplyFallbackZombieMaterials()
{
    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!BaseMaterial) return;

    FRandomStream Stream((int32)(GetUniqueID() ^ 0x0BADC0DE));

    auto MakeMaterial = [&](const TCHAR* Name, const FLinearColor& Color, float Roughness)
    {
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this, Name);
        if (Material)
        {
            Material->SetVectorParameterValue(TEXT("Color"), Color);
            Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
        }
        return Material;
    };

    const float SkinVariation = Stream.FRandRange(-0.025f, 0.025f);
    UMaterialInstanceDynamic* Skin = MakeMaterial(
        TEXT("ZombieSkin"),
        FLinearColor(
            0.19f + SkinVariation,
            0.255f + SkinVariation * 0.4f,
            0.17f + SkinVariation * 0.2f),
        0.88f);

    const FLinearColor ShirtPalette[] =
    {
        FLinearColor(0.10f, 0.12f, 0.105f),
        FLinearColor(0.16f, 0.115f, 0.075f),
        FLinearColor(0.075f, 0.10f, 0.13f),
        FLinearColor(0.15f, 0.15f, 0.12f)
    };
    UMaterialInstanceDynamic* Shirt = MakeMaterial(
        TEXT("ZombieShirt"),
        ShirtPalette[Stream.RandRange(0, UE_ARRAY_COUNT(ShirtPalette) - 1)],
        0.94f);

    UMaterialInstanceDynamic* Pants = MakeMaterial(
        TEXT("ZombiePants"),
        FLinearColor(0.055f, 0.06f, 0.057f),
        0.90f);

    if (PlaceholderHead && Skin) PlaceholderHead->SetMaterial(0, Skin);
    if (PlaceholderLeftArm && Skin) PlaceholderLeftArm->SetMaterial(0, Skin);
    if (PlaceholderRightArm && Skin) PlaceholderRightArm->SetMaterial(0, Skin);
    if (PlaceholderBody && Shirt) PlaceholderBody->SetMaterial(0, Shirt);
    if (PlaceholderLeftLeg && Pants) PlaceholderLeftLeg->SetMaterial(0, Pants);
    if (PlaceholderRightLeg && Pants) PlaceholderRightLeg->SetMaterial(0, Pants);
}

void AZombieCharacter::UpdateFallbackAnimation(float DeltaSeconds)
{
    FallbackAnimTime += DeltaSeconds;
    FallbackHitReact = FMath::FInterpTo(FallbackHitReact, 0.0f, DeltaSeconds, 7.5f);

    const float Speed = GetVelocity().Size2D();
    const float MoveAlpha = FMath::Clamp(Speed / FMath::Max(1.0f, GetCharacterMovement()->MaxWalkSpeed), 0.0f, 1.0f);
    const float Phase = FMath::Sin(FallbackAnimTime * GaitFrequency + GaitPhaseOffset);
    const float CounterPhase = -Phase;
    const float Secondary = FMath::Sin(FallbackAnimTime * GaitFrequency * 0.53f + GaitPhaseOffset * 0.7f);
    const float StepCompression = FMath::Abs(Phase);
    const bool bAttackWindow = CurrentTarget.IsValid() && AttackTimer > AttackCooldown - 0.34f;
    AttackPoseAlpha = FMath::FInterpTo(AttackPoseAlpha, bAttackWindow ? 1.0f : 0.0f, DeltaSeconds, bAttackWindow ? 14.0f : 5.5f);

    const float LimpLeft = FMath::Max(0.0f, LimpBias);
    const float LimpRight = FMath::Max(0.0f, -LimpBias);
    const float HitKick = FallbackHitReact * 12.0f;

    if (PlaceholderBody)
    {
        const float ForwardLean = -10.0f - MoveAlpha * 5.0f - AttackPoseAlpha * 8.0f + HitKick;
        PlaceholderBody->SetRelativeRotation(FRotator(
            ForwardLean + Phase * 1.8f * MoveAlpha,
            Secondary * 2.5f,
            Phase * (2.5f + FMath::Abs(LimpBias) * 4.5f) * MoveAlpha));
        PlaceholderBody->SetRelativeLocation(FVector(
            AttackPoseAlpha * 4.0f,
            LimpBias * StepCompression * 2.0f * MoveAlpha,
            13.0f + StepCompression * 2.2f * MoveAlpha - FallbackHitReact * 1.5f));
    }

    if (PlaceholderHead)
    {
        PlaceholderHead->SetRelativeRotation(FRotator(
            -13.0f + Secondary * 4.0f - FallbackHitReact * 10.0f,
            Phase * 7.0f + ShoulderBias * 0.35f,
            CounterPhase * 3.0f + LimpBias * 4.0f));
        PlaceholderHead->SetRelativeLocation(FVector(
            1.0f + AttackPoseAlpha * 3.0f,
            Secondary * 1.5f,
            68.0f + StepCompression * 1.4f * MoveAlpha));
    }

    const float LeftArmBase = FMath::Lerp(-54.0f, -92.0f, AttackPoseAlpha);
    const float RightArmBase = FMath::Lerp(-60.0f, -96.0f, AttackPoseAlpha);
    const float ArmSwing = (16.0f + GaitStride * 7.0f) * MoveAlpha * (1.0f - AttackPoseAlpha * 0.72f);

    if (PlaceholderLeftArm)
    {
        PlaceholderLeftArm->SetRelativeRotation(FRotator(
            LeftArmBase + Phase * ArmSwing * (1.0f - LimpLeft * 0.35f) + HitKick * 0.45f,
            ShoulderBias * 0.25f,
            -10.0f - LimpBias * 6.0f));
        PlaceholderLeftArm->SetRelativeLocation(FVector(6.0f + AttackPoseAlpha * 8.0f, -31.0f, 23.0f - LimpLeft * 4.0f));
    }

    if (PlaceholderRightArm)
    {
        PlaceholderRightArm->SetRelativeRotation(FRotator(
            RightArmBase + CounterPhase * ArmSwing * (1.0f - LimpRight * 0.35f) + HitKick * 0.30f,
            -ShoulderBias * 0.25f,
            10.0f - LimpBias * 6.0f));
        PlaceholderRightArm->SetRelativeLocation(FVector(6.0f + AttackPoseAlpha * 8.0f, 31.0f, 23.0f - LimpRight * 4.0f));
    }

    const float LegSwing = 23.0f * GaitStride * MoveAlpha;
    if (PlaceholderLeftLeg)
    {
        PlaceholderLeftLeg->SetRelativeRotation(FRotator(
            Phase * LegSwing * (1.0f - LimpLeft * 0.55f),
            0.0f,
            -LimpLeft * 6.0f));
        PlaceholderLeftLeg->SetRelativeLocation(FVector(
            FMath::Max(0.0f, Phase) * 4.0f * MoveAlpha,
            -13.0f,
            -46.0f + LimpLeft * StepCompression * 3.0f));
    }

    if (PlaceholderRightLeg)
    {
        PlaceholderRightLeg->SetRelativeRotation(FRotator(
            CounterPhase * LegSwing * (1.0f - LimpRight * 0.55f),
            0.0f,
            LimpRight * 6.0f));
        PlaceholderRightLeg->SetRelativeLocation(FVector(
            FMath::Max(0.0f, CounterPhase) * 4.0f * MoveAlpha,
            13.0f,
            -46.0f + LimpRight * StepCompression * 3.0f));
    }
}

void AZombieCharacter::UpdateFallbackDeath(float DeltaSeconds)
{
    FallbackDeathTime += DeltaSeconds;
    const float Alpha = FMath::Clamp(FallbackDeathTime / 0.78f, 0.0f, 1.0f);
    const float Ease = Alpha * Alpha * (3.0f - 2.0f * Alpha);
    const float Side = LimpBias >= 0.0f ? 1.0f : -1.0f;

    const FRotator Current = GetActorRotation();
    SetActorRotation(FRotator(
        FMath::Lerp(0.0f, -72.0f, Ease),
        Current.Yaw,
        FMath::Lerp(0.0f, Side * 34.0f, Ease)));

    if (PlaceholderLeftArm)
        PlaceholderLeftArm->SetRelativeRotation(FRotator(-30.0f + Ease * 48.0f, 0.0f, -28.0f * Side));
    if (PlaceholderRightArm)
        PlaceholderRightArm->SetRelativeRotation(FRotator(-42.0f + Ease * 35.0f, 0.0f, 24.0f * Side));
    if (PlaceholderHead)
        PlaceholderHead->SetRelativeRotation(FRotator(-18.0f - Ease * 22.0f, Side * 14.0f, Side * 18.0f));
}

void AZombieCharacter::TryApplyReferenceVisuals()
{
    TArray<USkeletalMesh*> ReferenceMeshes = DeadbrickReferenceAssets::FindSkeletalMeshes(
        {TEXT("enemy"), TEXT("zombie"), TEXT("infected"), TEXT("undead"), TEXT("human"), TEXT("creature"), TEXT("character")}, 16);

    if (ReferenceMeshes.Num() == 0 || !GetMesh())
    {
        bUsingFallbackZombie = true;
        SetFallbackVisible(true);
        UE_LOG(LogTemp, Display, TEXT("DEADBRICK zombie: no editor-valid skeletal reference found; varied articulated zombie fallback active."));
        return;
    }

    const int32 Index = static_cast<int32>(GetUniqueID() % static_cast<uint32>(ReferenceMeshes.Num()));
    USkeletalMesh* ReferenceMesh = ReferenceMeshes[Index];
    bUsingFallbackZombie = false;
    SetFallbackVisible(false);
    GetMesh()->SetSkeletalMesh(ReferenceMesh);
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

    USkeleton* Skeleton = ReferenceMesh->GetSkeleton();
    IdleAnimation = DeadbrickReferenceAssets::FindAnimationForSkeleton(Skeleton, {TEXT("idle"), TEXT("stand"), TEXT("breath")});
    WalkAnimation = DeadbrickReferenceAssets::FindAnimationForSkeleton(Skeleton, {TEXT("walk"), TEXT("shamble"), TEXT("run"), TEXT("move"), TEXT("locomotion")});
    AttackAnimation = DeadbrickReferenceAssets::FindAnimationForSkeleton(Skeleton, {TEXT("attack"), TEXT("bite"), TEXT("grab"), TEXT("hit"), TEXT("melee")});
    DeathAnimation = DeadbrickReferenceAssets::FindAnimationForSkeleton(Skeleton, {TEXT("death"), TEXT("die"), TEXT("dead")});

    UE_LOG(LogTemp, Display, TEXT("DEADBRICK zombie reference visual bound: %s"), *ReferenceMesh->GetPathName());
}

void AZombieCharacter::PlayReferenceAnimation(UAnimSequence* Animation, bool bLoop, float LockSeconds)
{
    if (!Animation || !GetMesh()) return;
    CurrentAnimation = Animation;
    AnimationLockTimer = FMath::Max(AnimationLockTimer, LockSeconds);
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    GetMesh()->SetAnimation(Animation);
    GetMesh()->Play(bLoop);
}

void AZombieCharacter::UpdateReferenceAnimation()
{
    if (!GetMesh() || AnimationLockTimer > 0.0f) return;

    UAnimSequence* Desired = GetVelocity().SizeSquared2D() > FMath::Square(10.0f) ? WalkAnimation : IdleAnimation;
    if (!Desired || Desired == CurrentAnimation) return;
    PlayReferenceAnimation(Desired, true, 0.0f);
}
