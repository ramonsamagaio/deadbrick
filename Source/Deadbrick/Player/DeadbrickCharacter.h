#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Items/DeadbrickItemTypes.h"
#include "DeadbrickCharacter.generated.h"

class UAnimSequence;
class UCameraComponent;
class UFirearmComponent;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

UCLASS()
class DEADBRICK_API ADeadbrickCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ADeadbrickCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> FirstPersonCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UFirearmComponent> Firearm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="First Person")
    TObjectPtr<USceneComponent> ViewModelRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="First Person")
    TObjectPtr<USkeletalMeshComponent> FirstPersonArms;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="First Person")
    TObjectPtr<UStaticMeshComponent> ViewWeapon;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="First Person")
    TObjectPtr<UStaticMeshComponent> FallbackLeftArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="First Person")
    TObjectPtr<UStaticMeshComponent> FallbackRightArm;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Survival")
    float MaxHealth = 100.0f;

    UPROPERTY(BlueprintReadOnly, Category="Survival")
    float Health = 100.0f;

    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    TMap<EDeadbrickItemType, int32> Inventory;

    UFUNCTION(BlueprintPure, Category="Inventory")
    int32 GetInventoryQuantity(EDeadbrickItemType ItemType) const;

    UFUNCTION(BlueprintCallable, Category="Inventory")
    void AddInventoryItem(EDeadbrickItemType ItemType, int32 Quantity);

    UFUNCTION(BlueprintCallable, Category="Inventory")
    bool ConsumeInventoryItem(EDeadbrickItemType ItemType, int32 Quantity);

    UFUNCTION(BlueprintCallable, Category="Player")
    void SetSafeSpawnTransform(const FVector& Location, const FRotator& Rotation);

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> IdleAnimation;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> WalkAnimation;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> CurrentAnimation;

    FVector SafeSpawnLocation = FVector::ZeroVector;
    FRotator SafeSpawnRotation = FRotator::ZeroRotator;
    bool bHasSafeSpawn = false;
    bool bDead = false;

    float WalkSpeed = 500.0f;
    float SprintSpeed = 760.0f;
    float ForwardInput = 0.0f;
    float RightInput = 0.0f;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void LookYaw(float Value);
    void LookPitch(float Value);
    void Fire();
    void Reload();
    void Interact();
    void SprintPressed();
    void SprintReleased();
    void QuickCraft();
    void QuickSave();
    void QuickLoad();
    void TryApplyReferenceVisuals();
    void UpdateReferenceAnimation();
    void UpdateGroundBraking();
    void RecoverFromFall();
    void RespawnAtSafeLocation();
};
