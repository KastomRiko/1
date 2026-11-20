// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterWeaponHolder.h"
#include "Animation/AnimInstance.h"
#include "ShooterWeapon.generated.h"

class AShooterProjectile;
class USkeletalMeshComponent;
class UAnimMontage;
class UAnimInstance;
class APawn;

UCLASS(Abstract)
class NETSHOOTER_API AShooterWeapon : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    TObjectPtr<USkeletalMeshComponent> ThirdPersonMesh;

protected:
    IShooterWeaponHolder* WeaponOwner = nullptr;

    UPROPERTY(EditAnywhere, Category="Ammo")
    TSubclassOf<AShooterProjectile> ProjectileClass;

    UPROPERTY(EditAnywhere, Category="Ammo", meta=(ClampMin=0, ClampMax=100))
    int32 MagazineSize = 10;

    UPROPERTY(ReplicatedUsing=OnRep_CurrentBullets, BlueprintReadWrite, Category="Ammo")
    int32 CurrentBullets = 0;

    UPROPERTY(EditAnywhere, Category="Animation")
    TObjectPtr<UAnimMontage> FiringMontage;

    UPROPERTY(EditAnywhere, Category="Animation")
    TSubclassOf<UAnimInstance> FirstPersonAnimInstanceClass;

    UPROPERTY(EditAnywhere, Category="Animation")
    TSubclassOf<UAnimInstance> ThirdPersonAnimInstanceClass;

    UPROPERTY(EditAnywhere, Category="Aim", meta=(ClampMin=0, ClampMax=90, Units="Degrees"))
    float AimVariance = 0.0f;

    UPROPERTY(EditAnywhere, Category="Aim", meta=(ClampMin=0, ClampMax=100))
    float FiringRecoil = 0.0f;

    UPROPERTY(EditAnywhere, Category="Aim")
    FName MuzzleSocketName;

    UPROPERTY(EditAnywhere, Category="Aim", meta=(ClampMin=0, ClampMax=1000, Units="cm"))
    float MuzzleOffset = 10.0f;

    UPROPERTY(EditAnywhere, Category="Refire")
    bool bFullAuto = false;

    UPROPERTY(EditAnywhere, Category="Refire", meta=(ClampMin=0, ClampMax=5, Units="s"))
    float RefireRate = 0.5f;

    float TimeOfLastShot = -1.0f;

    UPROPERTY(Replicated)
    bool bIsFiring = false;

    FTimerHandle RefireTimer;
    TObjectPtr<APawn> PawnOwner;

    UPROPERTY(EditAnywhere, Category="Perception", meta=(ClampMin=0, ClampMax=100))
    float ShotLoudness = 1.0f;

    UPROPERTY(EditAnywhere, Category="Perception", meta=(ClampMin=0, ClampMax=100000, Units="cm"))
    float ShotNoiseRange = 300.0f;

    UPROPERTY(EditAnywhere, Category="Perception")
    FName NoiseOwnerTag = FName("Shot");

public:
    AShooterWeapon();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
    virtual void OnRep_Owner() override;

    void InitializeWeaponOwner();

    UFUNCTION()
    void OnOwnerDestroyed(AActor* DestroyedActor);

    UFUNCTION()
    void OnRep_CurrentBullets();

public:
    void ActivateWeapon(const FName& OwnerTag);
    void DeactivateWeapon();
    void StartFiring();
    void StopFiring();

    UFUNCTION(Server, Reliable)
    void ServerStartFiring();

    UFUNCTION(Server, Reliable)
    void ServerStopFiring();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastFireVisuals();

protected:
    virtual void Fire();
    void FireCooldownExpired();
    virtual void FireProjectile(const FVector& TargetLocation);
    FTransform CalculateProjectileSpawnTransform(const FVector& TargetLocation) const;

public:
    UFUNCTION(BlueprintPure, Category="Weapon")
    USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

    UFUNCTION(BlueprintPure, Category="Weapon")
    USkeletalMeshComponent* GetThirdPersonMesh() const { return ThirdPersonMesh; }

    const TSubclassOf<UAnimInstance>& GetFirstPersonAnimInstanceClass() const;
    const TSubclassOf<UAnimInstance>& GetThirdPersonAnimInstanceClass() const;
    int32 GetMagazineSize() const { return MagazineSize; }
    int32 GetBulletCount() const { return CurrentBullets; }

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
