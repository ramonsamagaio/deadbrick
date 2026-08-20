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
#include "Reference/ReferenceAssetResolver.h"
#include "UObject/ConstructorHelpers.h"

AZombieCharacter::AZombieCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCharacterMovement()->MaxWalkSpeed = 230.0f;
    GetCharacterMovement()->MaxAcceleration = 900.0f;
    GetCharacterMovement()->BrakingDecelerationWalking = 1200.0f;

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

    auto ConfigurePart = [&](UStaticMeshComponent* Part, const FVector& Location, const FVector& Scale)
    {
        Part->SetupAttachment(GetRootComponent());
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeScale3D(Scale);
        if (CubeMesh.Succeeded()) Part->SetStaticMesh(CubeMesh.Object);
    };

    PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
    ConfigurePart(PlaceholderBody, FVector(0.0f, 0.0f, 12.0f), FVector(0.34f, 0.25f, 0.48f));

    PlaceholderHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderHead"));
    ConfigurePart(PlaceholderHead, FVector(0.0f, 0.0f, 69.0f), FVector(0.25f, 0.24f, 0.27f));

    PlaceholderLeftArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderLeftArm"));
    ConfigurePart(PlaceholderLeftArm, FVector(4.0f, -32.0f, 22.0f), FVector(0.12f, 0.10f, 0.42f));
    PlaceholderLeftArm->SetRelativeRotation(FRotator(-54.0f, 0.0f, -7.0f));

    PlaceholderRightArm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderRightArm"));
    ConfigurePart(PlaceholderRightArm, FVector(4.0f, 32.0f, 22.0f), FVector(0.12f, 0.10f, 0.42f));
    PlaceholderRightArm->SetRelativeRotation(FRotator(-58.0f, 0.0f, 7.0f));

    PlaceholderLeftLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderLeftLeg"));
    ConfigurePart(PlaceholderLeftLeg, FVector(0.0f, -13.0f, -46.0f), FVector(0.14f, 0.13f, 0.45f));

    PlaceholderRightLeg = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderRightLeg"));
    ConfigurePart(PlaceholderRightLeg, FVector(0.0f, 13.0f, -46.0f), FVector(0.14f, 0.13f, 0.45f));
}

void AZombieCharacter::BeginPlay()
{
    Super::BeginPlay();
    Health = MaxHealth;
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
    if (bDead) return;

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
                {
                    PlayReferenceAnimation(AttackAnimation, false, FMath::Min(0.8f, AttackCooldown * 0.75f));
                }
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

float AZombieCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (bDead) return 0.0f;

    const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    Health -= Applied;
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
            // Even the asset-free fallback now has a readable death instead of simply vanishing.
            SetActorRotation(GetActorRotation() + FRotator(0.0f, 0.0f, 82.0f));
            SetLifeSpan(2.2f);
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

void AZombieCharacter::UpdateFallbackAnimation(float DeltaSeconds)
{
    FallbackAnimTime += DeltaSeconds;

    const float Speed = GetVelocity().Size2D();
    const float MoveAlpha = FMath::Clamp(Speed / FMath::Max(1.0f, GetCharacterMovement()->MaxWalkSpeed), 0.0f, 1.0f);
    const float Phase = FMath::Sin(FallbackAnimTime * 7.0f);
    const float CounterPhase = -Phase;
    const bool bAttacking = CurrentTarget.IsValid() && AttackTimer > AttackCooldown - 0.30f;

    if (PlaceholderBody)
    {
        PlaceholderBody->SetRelativeRotation(FRotator(-7.0f + Phase * 2.0f * MoveAlpha, 0.0f, Phase * 2.0f * MoveAlpha));
        PlaceholderBody->SetRelativeLocation(FVector(0.0f, 0.0f, 12.0f + FMath::Abs(Phase) * 2.5f * MoveAlpha));
    }

    if (PlaceholderHead)
        PlaceholderHead->SetRelativeRotation(FRotator(-10.0f + Phase * 3.0f, Phase * 6.0f, CounterPhase * 3.0f));

    const float ArmBase = bAttacking ? -92.0f : -56.0f;
    const float ArmSwing = bAttacking ? 12.0f : 18.0f * MoveAlpha;
    if (PlaceholderLeftArm) PlaceholderLeftArm->SetRelativeRotation(FRotator(ArmBase + Phase * ArmSwing, 0.0f, -9.0f));
    if (PlaceholderRightArm) PlaceholderRightArm->SetRelativeRotation(FRotator(ArmBase + CounterPhase * ArmSwing, 0.0f, 9.0f));

    if (PlaceholderLeftLeg) PlaceholderLeftLeg->SetRelativeRotation(FRotator(Phase * 24.0f * MoveAlpha, 0.0f, 0.0f));
    if (PlaceholderRightLeg) PlaceholderRightLeg->SetRelativeRotation(FRotator(CounterPhase * 24.0f * MoveAlpha, 0.0f, 0.0f));
}

void AZombieCharacter::TryApplyReferenceVisuals()
{
    TArray<USkeletalMesh*> ReferenceMeshes = DeadbrickReferenceAssets::FindSkeletalMeshes(
        {TEXT("enemy"), TEXT("skeleton"), TEXT("goblin"), TEXT("creature"), TEXT("character")}, 12);

    if (ReferenceMeshes.Num() == 0 || !GetMesh())
    {
        bUsingFallbackZombie = true;
        SetFallbackVisible(true);
        UE_LOG(LogTemp, Display, TEXT("DEADBRICK zombie: no cooked reference enemy mesh found; articulated zombie fallback active."));
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
    WalkAnimation = DeadbrickReferenceAssets::FindAnimationForSkeleton(Skeleton, {TEXT("walk"), TEXT("run"), TEXT("move"), TEXT("locomotion")});
    AttackAnimation = DeadbrickReferenceAssets::FindAnimationForSkeleton(Skeleton, {TEXT("attack"), TEXT("bite"), TEXT("hit"), TEXT("melee")});
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
