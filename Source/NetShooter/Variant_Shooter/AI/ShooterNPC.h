// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ANetShooterCharacter.h"
#include "ShooterWeaponHolder.h"
#include "ShooterNPC.generated.h"

class AShooterWeapon;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPawnDeathDelegate);

UCLASS(abstract)
class NETSHOOTER_API AShooterNPC : public ANetShooterCharacter, public IShooterWeaponHolder
{
    GENERATED_BODY()

public:
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Damage")
    float CurrentHP = 100.0f;

protected:
    UPROPERTY(EditAnywhere, Category="Damage")
    FName RagdollCollisionProfile = FName("Ragdoll");

    UPROPERTY(EditAnywhere, Category="Damage")
    float DeferredDestructionTime = 5.0f;

    UPROPERTY(Replicated, EditAnywhere, Category="Team")
    uint8 TeamByte = 1;

    UPROPERTY(EditAnywhere, Category="Team")
    FName DeathTag = FName("Dead");

    UPROPERTY(Replicated)
    TObjectPtr<AShooterWeapon> Weapon;

    UPROPERTY(EditAnywhere, Category="Weapon")
    TSubclassOf<AShooterWeapon> WeaponClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapons")
    FName FirstPersonWeaponSocket = FName("HandGrip_R");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapons")
    FName ThirdPersonWeaponSocket = FName("HandGrip_R");

    UPROPERTY(EditAnywhere, Category="Aim")
    float AimRange = 10000.0f;

    UPROPERTY(EditAnywhere, Category="Aim")
    float AimVarianceHalfAngle = 10.0f;

    UPROPERTY(EditAnywhere, Category="Aim")
    float MinAimOffsetZ = -35.0f;

    UPROPERTY(EditAnywhere, Category="Aim")
    float MaxAimOffsetZ = -60.0f;

    TWeakObjectPtr<AActor> CurrentAimTarget;

    bool bIsShooting = false;

    UPROPERTY(ReplicatedUsing=OnRep_IsDead)
    bool bIsDead = false;

    FTimerHandle DeathTimer;

public:
    FPawnDeathDelegate OnPawnDeath;

    UFUNCTION(BlueprintPure, Category="Team")
    uint8 GetTeamByte() const { return TeamByte; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

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
    UFUNCTION()
    void OnRep_IsDead();

    void ApplyDeathState();
    void Die(AController* Killer);
    void DeferredDestruction();

public:
    void StartShooting(AActor* ActorToShoot);
    void StopShooting();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
