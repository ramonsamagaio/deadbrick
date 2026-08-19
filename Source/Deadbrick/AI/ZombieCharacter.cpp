#include "AI/ZombieCharacter.h"
#include "AI/ZombieDirectorSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

AZombieCharacter::AZombieCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCharacterMovement()->MaxWalkSpeed = 230.0f;
}

void AZombieCharacter::BeginPlay()
{
    Super::BeginPlay();
    Health = MaxHealth;
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

    if (!bHasMoveTarget) return;

    const FVector Delta = MoveTarget - GetActorLocation();
    const float Distance = Delta.Size2D();
    if (CurrentTarget.IsValid() && Distance <= AttackDistanceCm)
    {
        if (AttackTimer <= 0.0f)
        {
            AttackTimer = AttackCooldown;
            UGameplayStatics::ApplyDamage(CurrentTarget.Get(), AttackDamage, GetController(), this, nullptr);
        }
        return;
    }

    if (Distance > 30.0f)
    {
        AddMovementInput(Delta.GetSafeNormal2D(), 1.0f);
    }
    else if (!CurrentTarget.IsValid())
    {
        bHasMoveTarget = false;
    }
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
