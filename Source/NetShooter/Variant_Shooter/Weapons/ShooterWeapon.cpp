// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterWeapon.h"

#include "ShooterProjectile.h"
#include "ShooterWeaponHolder.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AShooterWeapon::AShooterWeapon()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
    FirstPersonMesh->SetupAttachment(RootComponent);
    FirstPersonMesh->SetCollisionProfileName(TEXT("NoCollision"));
    FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    FirstPersonMesh->bOnlyOwnerSee = true;

    ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person Mesh"));
    ThirdPersonMesh->SetupAttachment(RootComponent);
    ThirdPersonMesh->SetCollisionProfileName(TEXT("NoCollision"));
    ThirdPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
    ThirdPersonMesh->bOwnerNoSee = true;
}

void AShooterWeapon::BeginPlay()
{
    Super::BeginPlay();

    InitializeWeaponOwner();

    if (HasAuthority())
    {
        CurrentBullets = MagazineSize;
        TimeOfLastShot = -FMath::Max(RefireRate, 0.01f);
    }

}

void AShooterWeapon::OnRep_Owner()
{
    Super::OnRep_Owner();
    InitializeWeaponOwner();
}

void AShooterWeapon::InitializeWeaponOwner()
{
    AActor* WeaponActorOwner = GetOwner();
    WeaponOwner = Cast<IShooterWeaponHolder>(WeaponActorOwner);
    PawnOwner = Cast<APawn>(WeaponActorOwner);

    if (WeaponActorOwner)
    {
        WeaponActorOwner->OnDestroyed.AddUniqueDynamic(this, &AShooterWeapon::OnOwnerDestroyed);
    }

    if (WeaponOwner)
    {
        WeaponOwner->AttachWeaponMeshes(this);
        WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
    }
}

void AShooterWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
    }

    Super::EndPlay(EndPlayReason);
}

void AShooterWeapon::OnOwnerDestroyed(AActor* DestroyedActor)
{
    Destroy();
}

void AShooterWeapon::OnRep_CurrentBullets()
{
    if (WeaponOwner)
    {
        WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
    }
}

void AShooterWeapon::ActivateWeapon(const FName& OwnerTag)
{
    if (!HasAuthority())
    {
        return;
    }

    SetActorHiddenInGame(false);

    if (WeaponOwner)
    {
        WeaponOwner->OnWeaponActivated(this);
        WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
    }

    ForceNetUpdate();
}

void AShooterWeapon::DeactivateWeapon()
{
    if (!HasAuthority())
    {
        return;
    }

    StopFiring();
    SetActorHiddenInGame(true);

    if (WeaponOwner)
    {
        WeaponOwner->OnWeaponDeactivated(this);
    }

    ForceNetUpdate();
}

void AShooterWeapon::StartFiring()
{
    ServerStartFiring();
}

void AShooterWeapon::StopFiring()
{
    ServerStopFiring();
}

void AShooterWeapon::ServerStartFiring_Implementation()
{
    UWorld* World = GetWorld();
    if (!HasAuthority() || !World || !IsValid(GetOwner()) || !WeaponOwner ||
        !IsValid(PawnOwner) || CurrentBullets <= 0 || IsHidden())
    {
        return;
    }

    if (bIsFiring)
    {
        return;
    }

    bIsFiring = true;

    const float SafeRefireRate = FMath::Max(RefireRate, 0.01f);
    const float TimeSinceLastShot = World->GetTimeSeconds() - TimeOfLastShot;

    if (TimeSinceLastShot >= SafeRefireRate)
    {
        Fire();
    }
    else
    {
        World->GetTimerManager().SetTimer(
            RefireTimer,
            this,
            &AShooterWeapon::FireCooldownExpired,
            SafeRefireRate - TimeSinceLastShot,
            false);
    }
}

void AShooterWeapon::ServerStopFiring_Implementation()
{
    if (!HasAuthority())
    {
        return;
    }

    bIsFiring = false;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
    }

    ForceNetUpdate();
}

void AShooterWeapon::MulticastFireVisuals_Implementation()
{
    if (FiringMontage && WeaponOwner)
    {
        WeaponOwner->PlayFiringMontage(FiringMontage);
    }

    if (WeaponOwner)
    {
        WeaponOwner->AddWeaponRecoil(FiringRecoil);
    }
}

void AShooterWeapon::Fire()
{
    if (!HasAuthority() || !bIsFiring || !WeaponOwner || CurrentBullets <= 0 || !GetWorld())
    {
        bIsFiring = false;
        return;
    }

    const FVector TargetLocation = WeaponOwner->GetWeaponTargetLocation();

    --CurrentBullets;
    TimeOfLastShot = GetWorld()->GetTimeSeconds();

    FireProjectile(TargetLocation);
    MulticastFireVisuals();
    WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
    ForceNetUpdate();

    if (PawnOwner)
    {
        MakeNoise(ShotLoudness, PawnOwner, PawnOwner->GetActorLocation(), ShotNoiseRange, NoiseOwnerTag);
    }

    if (CurrentBullets <= 0)
    {
        bIsFiring = false;
        GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
        return;
    }

    if (bFullAuto && bIsFiring)
    {
        GetWorld()->GetTimerManager().SetTimer(
            RefireTimer,
            this,
            &AShooterWeapon::Fire,
            FMath::Max(RefireRate, 0.01f),
            false);
    }
    else
    {
        bIsFiring = false;
        GetWorld()->GetTimerManager().SetTimer(
            RefireTimer,
            this,
            &AShooterWeapon::FireCooldownExpired,
            FMath::Max(RefireRate, 0.01f),
            false);
    }
}

void AShooterWeapon::FireCooldownExpired()
{
    if (!HasAuthority())
    {
        return;
    }

    if (bIsFiring)
    {
        Fire();
    }
    else if (WeaponOwner && CurrentBullets > 0)
    {
        WeaponOwner->OnSemiWeaponRefire();
    }
}

void AShooterWeapon::FireProjectile(const FVector& TargetLocation)
{
    if (!HasAuthority() || !ProjectileClass || !GetWorld())
    {
        return;
    }

    FTransform SpawnTransform = CalculateProjectileSpawnTransform(TargetLocation);

    if (AimVariance > 0.0f)
    {
        const FVector Direction = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(
            SpawnTransform.GetRotation().Vector(),
            AimVariance);
        SpawnTransform.SetRotation(Direction.Rotation().Quaternion());
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwner();
    SpawnParams.Instigator = PawnOwner;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    if (AShooterProjectile* Projectile = GetWorld()->SpawnActor<AShooterProjectile>(
        ProjectileClass, SpawnTransform, SpawnParams))
    {
        Projectile->SetNoiseTag(NoiseOwnerTag);
    }
}

FTransform AShooterWeapon::CalculateProjectileSpawnTransform(const FVector& TargetLocation) const
{
    FTransform MuzzleTransform = GetActorTransform();

    const bool bUseFirstPersonMesh = PawnOwner && PawnOwner->IsLocallyControlled();

    if (bUseFirstPersonMesh && FirstPersonMesh && !MuzzleSocketName.IsNone() && FirstPersonMesh->DoesSocketExist(MuzzleSocketName))
    {
        MuzzleTransform = FirstPersonMesh->GetSocketTransform(MuzzleSocketName);
    }
    else if (ThirdPersonMesh && !MuzzleSocketName.IsNone() && ThirdPersonMesh->DoesSocketExist(MuzzleSocketName))
    {
        MuzzleTransform = ThirdPersonMesh->GetSocketTransform(MuzzleSocketName);
    }
    else if (FirstPersonMesh && !MuzzleSocketName.IsNone() && FirstPersonMesh->DoesSocketExist(MuzzleSocketName))
    {
        MuzzleTransform = FirstPersonMesh->GetSocketTransform(MuzzleSocketName);
    }

    FVector Direction = (TargetLocation - MuzzleTransform.GetLocation()).GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        Direction = MuzzleTransform.GetRotation().Vector();
    }

    const FVector SpawnLocation = MuzzleTransform.GetLocation() + Direction * MuzzleOffset;
    return FTransform(Direction.Rotation(), SpawnLocation, FVector::OneVector);
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetFirstPersonAnimInstanceClass() const
{
    return FirstPersonAnimInstanceClass;
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetThirdPersonAnimInstanceClass() const
{
    return ThirdPersonAnimInstanceClass;
}

void AShooterWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AShooterWeapon, CurrentBullets);
    DOREPLIFETIME(AShooterWeapon, bIsFiring);
}
