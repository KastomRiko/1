#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ANetShooterCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputComponent;
class UInputMappingContext;
class UInputAction;
class ANetShooterWeapon;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCharacterDamagedDelegate, float, Damage, AController*, Instigator);

UCLASS(Abstract)
class NETSHOOTER_API ANetShooterCharacter : public ACharacter
{
    GENERATED_BODY()

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UCameraComponent> FirstPersonCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
    TObjectPtr<USpringArmComponent> ViewPointArm;
    UPROPERTY(EditAnywhere, Category="Input")
    UInputMappingContext* DefaultMappingContext;
    UPROPERTY(EditAnywhere, Category="Input")
    TObjectPtr<UInputAction> MoveAction;
    UPROPERTY(EditAnywhere, Category="Input")
    TObjectPtr<UInputAction> LookAction;
    UPROPERTY(EditAnywhere, Category="Input")
    TObjectPtr<UInputAction> JumpAction;
    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnCharacterDamagedDelegate OnDamaged;

public:
    ANetShooterCharacter();
    UFUNCTION(BlueprintPure, Category="Camera")
    UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCamera; }
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
    void HandleMoveInput(const FInputActionValue& Value);
    void HandleLookInput(const FInputActionValue& Value);
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
    virtual FVector GetWeaponCameraLocation() const;

public:
    UFUNCTION(BlueprintCallable, Category="Character")
    void SetMovementEnabled(bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="Character")
    void DisablePlayerInput();
    UFUNCTION(BlueprintCallable, Category="Character")
    void EnablePlayerInput();
    // Keep these reflected functions available to existing Blueprint assets.
    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoJumpStart();

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoJumpEnd();

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoAim(float Yaw, float Pitch);

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoMove(float Right, float Forward);

    // Native Enhanced Input helpers; these are not exposed to Blueprint.
    virtual void DoMove(const FVector2D& Value);
    virtual void DoLook(const FVector2D& Value);
    UFUNCTION(BlueprintCallable, Category="Character")
    virtual void EquipWeapon(class ANetShooterWeapon* Weapon);
};
