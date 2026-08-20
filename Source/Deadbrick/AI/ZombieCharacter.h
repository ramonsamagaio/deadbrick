#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZombieCharacter.generated.h"

class UAnimSequence;
class UStaticMeshComponent;

UCLASS()
class DEADBRICK_API AZombieCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AZombieCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie") TObjectPtr<UStaticMeshComponent> PlaceholderBody;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie") TObjectPtr<UStaticMeshComponent> PlaceholderHead;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie") TObjectPtr<UStaticMeshComponent> PlaceholderLeftArm;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie") TObjectPtr<UStaticMeshComponent> PlaceholderRightArm;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie") TObjectPtr<UStaticMeshComponent> PlaceholderLeftLeg;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie") TObjectPtr<UStaticMeshComponent> PlaceholderRightLeg;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zombie") float MaxHealth = 100.0f;
    UPROPERTY(BlueprintReadOnly, Category="Zombie") float Health = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zombie") float VisualDetectionRadiusCm = 2800.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zombie") float HearingRadiusCm = 7000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zombie") float AttackDistanceCm = 130.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zombie") float AttackDamage = 15.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zombie") float AttackCooldown = 1.1f;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> IdleAnimation;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> WalkAnimation;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> AttackAnimation;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> DeathAnimation;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> CurrentAnimation;

    TWeakObjectPtr<AActor> CurrentTarget;
    FVector MoveTarget = FVector::ZeroVector;
    bool bHasMoveTarget = false;
    bool bDead = false;
    bool bUsingFallbackZombie = true;
    float RetargetTimer = 0.0f;
    float AttackTimer = 0.0f;
    float AnimationLockTimer = 0.0f;
    float FallbackAnimTime = 0.0f;

    void AcquireTarget();
    void TryApplyReferenceVisuals();
    void UpdateReferenceAnimation();
    void UpdateFallbackAnimation(float DeltaSeconds);
    void SetFallbackVisible(bool bVisible);
    void PlayReferenceAnimation(UAnimSequence* Animation, bool bLoop, float LockSeconds = 0.0f);
};
