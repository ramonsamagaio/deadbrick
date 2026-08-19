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

    PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
    PlaceholderBody->SetupAttachment(GetRootComponent());
    PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlaceholderBody->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.65f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded()) PlaceholderBody->SetStaticMesh(CubeMesh.Object);
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
            SetLifeSpan(1.0f);
        }
    }
    return Applied;
}

void AZombieCharacter::TryApplyReferenceVisuals()
{
    TArray<USkeletalMesh*> ReferenceMeshes = DeadbrickReferenceAssets::FindSkeletalMeshes(
        {TEXT("enemy"), TEXT("skeleton"), TEXT("goblin"), TEXT("creature"), TEXT("character")}, 12);

    if (ReferenceMeshes.Num() == 0 || !GetMesh())
    {
        UE_LOG(LogTemp, Display, TEXT("DEADBRICK zombie: no cooked reference enemy mesh found; cube placeholder remains."));
        return;
    }

    const int32 Index = FMath::Abs(GetUniqueID()) % ReferenceMeshes.Num();
    USkeletalMesh* ReferenceMesh = ReferenceMeshes[Index];
    PlaceholderBody->SetVisibility(false, true);
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
