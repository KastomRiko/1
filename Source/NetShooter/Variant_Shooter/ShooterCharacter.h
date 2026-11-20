// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ANetShooterCharacter.h"
#include "ShooterWeaponHolder.h"
#include "ShooterCharacter.generated.h"

class AShooterWeapon;
class UInputAction;
class UInputComponent;
class UPawnNoiseEmitterComponent;
class USkeletalMeshComponent;
class UAnimMontage;
class UAnimInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBulletCountUpdatedDelegate, int32, MagazineSize, int32, Bullets);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDamagedDelegate, float, LifePercent);

UCLASS(abstract)
class NETSHOOTER_API AShooterCharacter : public ANetShooterCharacter, public IShooterWeaponHolder
{
    GENERATED_BODY()

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UPawnNoiseEmitterComponent> PawnNoiseEmitter;

    UPROPERTY(EditAnywhere, Category="Input")
    TObjectPtr<UInputAction> FireAction;

    UPROPERTY(EditAnywhere, Category="Input")
    TObjectPtr<UInputAction> SwitchWeaponAction;

    UPROPERTY(EditAnywhere, Category="Weapons")
    FName FirstPersonWeaponSocket = FName("HandGrip_R");

    UPROPERTY(EditAnywhere, Category="Weapons")
    FName ThirdPersonWeaponSocket = FName("HandGrip_R");

    UPROPERTY(EditAnywhere, Category="Aim", meta=(ClampMin=0, ClampMax=100000, Units="cm"))
    float MaxAimDistance = 10000.0f;

    UPROPERTY(EditAnywhere, Category="Health")
    float MaxHP = 500.0f;

    UPROPERTY(ReplicatedUsing=OnRep_CurrentHP, BlueprintReadOnly, Category="Health")
    float CurrentHP = 500.0f;

    UPROPERTY(ReplicatedUsing=OnRep_IsDead, BlueprintReadOnly, Category="Health")
    bool bIsDead = false;

    bool bDeathPresentationApplied = false;

    UPROPERTY(ReplicatedUsing=OnRep_TeamByte, BlueprintReadOnly, Category="Team")
    uint8 TeamByte = 0;

    UPROPERTY(EditAnywhere, Category="Team")
    FName DeathTag = FName("Dead");

    UPROPERTY(EditAnywhere, Category="Tags")
    FName PlayerTag = FName("Player");

    UPROPERTY(Replicated)
    TArray<TObjectPtr<AShooterWeapon>> OwnedWeapons;

    UPROPERTY(ReplicatedUsing=OnRep_CurrentWeapon)
    TObjectPtr<AShooterWeapon> CurrentWeapon;

    UPROPERTY(EditAnywhere, Category="Destruction", meta=(ClampMin=0, ClampMax=10, Units="s"))
    float RespawnTime = 5.0f;

    FTimerHandle RespawnTimer;

public:
    FBulletCountUpdatedDelegate OnBulletCountUpdated;
    FDamagedDelegate OnDamaged;

    AShooterCharacter();

    UFUNCTION(BlueprintPure, Category="Components")
    USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

    UFUNCTION()
    void OnRep_CurrentHP();

    UFUNCTION()
    void OnRep_IsDead();

    void ApplyDeathState();

    UFUNCTION()
    void OnRep_TeamByte();

    UFUNCTION()
    void OnRep_CurrentWeapon();

    UFUNCTION(Server, Reliable)
    void ServerSwitchWeapon();

    virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

public:
    virtual void DoAim(float Yaw, float Pitch) override;
    virtual void DoMove(float Right, float Forward) override;
    virtual void DoJumpStart() override;
    virtual void DoJumpEnd() override;

    UFUNCTION(BlueprintCallable, Category="Input")
    void DoStartFiring();

    UFUNCTION(BlueprintCallable, Category="Input")
    void DoStopFiring();

    UFUNCTION(BlueprintCallable, Category="Input")
    void DoSwitchWeapon();

public:
    virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) override;
    virtual void PlayFiringMontage(UAnimMontage* Montage) override;
    virtual void AddWeaponRecoil(float Recoil) override;
    virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) override;
    virtual FVector GetWeaponTargetLocation() override;
    virtual void AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) override;
    virtual void OnWeaponActivated(AShooterWeapon* Weapon) override;
    virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) override;
    virtual void OnSemiWeaponRefire() override;

protected:
    AShooterWeapon* FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const;
    void Die(AController* Killer);

    UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta=(DisplayName="On Death"))
    void BP_OnDeath();

    void OnRespawn();

public:
    bool IsDead() const;

    UFUNCTION(BlueprintPure, Category="Health")
    float GetCurrentHP() const { return CurrentHP; }

    UFUNCTION(BlueprintPure, Category="Health")
    float GetMaxHP() const { return MaxHP; }

    UFUNCTION(BlueprintPure, Category="Team")
    uint8 GetTeamByte() const { return TeamByte; }

    void SetTeam(uint8 Team);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
