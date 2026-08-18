#include "AI/ZombieCharacter.h"
#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"

AZombieCharacter::AZombieCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    GetCharacterMovement()->MaxWalkSpeed = 230.0f;
}

void AZombieCharacter::BeginPlay()
{
    Super::BeginPlay();
    Health = MaxHealth;
}

void AZombieCharacter::AcquireTarget()
{
    ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
    if (Player && FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(DetectionRadiusCm))
    {
        CurrentTarget = Player;
    }
    else
    {
        CurrentTarget.Reset();
    }
}

void AZombieCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RetargetTimer -= DeltaSeconds;
    AttackTimer -= DeltaSeconds;

    if (RetargetTimer <= 0.0f)
    {
        RetargetTimer = 0.35f;
        AcquireTarget();
    }

    if (!CurrentTarget.IsValid()) return;

    const float Distance = FVector::Dist(CurrentTarget->GetActorLocation(), GetActorLocation());
    if (Distance > AttackDistanceCm)
    {
        UAIBlueprintHelperLibrary::SimpleMoveToActor(GetController(), CurrentTarget.Get());
    }
    else if (AttackTimer <= 0.0f)
    {
        AttackTimer = AttackCooldown;
        UGameplayStatics::ApplyDamage(CurrentTarget.Get(), AttackDamage, GetController(), this, nullptr);
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
