#include "AI/ZombieCharacter.h"

#include "AI/ZombieDirectorSubsystem.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
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
    GetCharacterMovement()->BrakingDecelerationWalking = 1200.0f;

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();

    PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
    PlaceholderBody->SetupAttachment(GetRootComponent());
    PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlaceholderBody->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.65f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        PlaceholderBody->SetStaticMesh(CubeMesh.Object);
    }
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
    RetargetTimer -= DeltaSeconds;
    AttackTimer -= DeltaSeconds;

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
    const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    Health -= Applied;
    if (Health <= 0.0f)
    {
        Destroy();
    }
    return Applied;
}

void AZombieCharacter::TryApplyReferenceVisuals()
{
    FString MeshPath;
    USkeletalMesh* ReferenceMesh = DeadbrickReferenceAssets::FindSkeletalMesh(
        {TEXT("enemy"), TEXT("skeleton"), TEXT("goblin"), TEXT("creature"), TEXT("character")},
        &MeshPath);

    if (!ReferenceMesh || !GetMesh())
    {
        UE_LOG(LogTemp, Display, TEXT("DEADBRICK zombie: no cooked reference enemy mesh found; cube placeholder remains."));
        return;
    }

    PlaceholderBody->SetVisibility(false, true);
    GetMesh()->SetSkeletalMesh(ReferenceMesh);
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

    USkeleton* Skeleton = ReferenceMesh->GetSkeleton();
    IdleAnimation = DeadbrickReferenceAssets::FindAnimationForSkeleton(
        Skeleton, {TEXT("idle"), TEXT("stand"), TEXT("breath")});
    WalkAnimation = DeadbrickReferenceAssets::FindAnimationForSkeleton(
        Skeleton, {TEXT("walk"), TEXT("run"), TEXT("move")});

    UE_LOG(LogTemp, Display, TEXT("DEADBRICK zombie reference visual bound: %s"), *MeshPath);
}

void AZombieCharacter::UpdateReferenceAnimation()
{
    if (!GetMesh()) return;

    UAnimSequence* Desired = GetVelocity().SizeSquared2D() > FMath::Square(10.0f) ? WalkAnimation : IdleAnimation;
    if (!Desired || Desired == CurrentAnimation) return;

    CurrentAnimation = Desired;
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    GetMesh()->SetAnimation(Desired);
    GetMesh()->Play(true);
}
