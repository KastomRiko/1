// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterWeapon.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "ShooterProjectile.h"
#include "ShooterWeaponHolder.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

AShooterWeapon::AShooterWeapon()
{
    PrimaryActorTick.bCanEverTick = true;

    // Enable network replication
    bReplicates = true;
    SetReplicateMovement(true);

    // Root component
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    // First person mesh
    FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
    FirstPersonMesh->SetupAttachment(RootComponent);
    FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
    FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    FirstPersonMesh->bOnlyOwnerSee = true;

    // Third person mesh
    ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person Mesh"));
    ThirdPersonMesh->SetupAttachment(RootComponent);
    ThirdPersonMesh->SetCollisionProfileName(FName("NoCollision"));
    ThirdPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
    ThirdPersonMesh->bOwnerNoSee = true;
}

void AShooterWeapon::BeginPlay()
{
    Super::BeginPlay();

    // Subscribe to owner destroyed
    GetOwner()->OnDestroyed.AddDynamic(this, &AShooterWeapon::OnOwnerDestroyed);

    // Cast owner
    WeaponOwner = Cast<IShooterWeaponHolder>(GetOwner());
    PawnOwner = Cast<APawn>(GetOwner());

    // Fill first ammo clip (only on server)
    if (HasAuthority())
    {
        CurrentBullets = MagazineSize;
    }

    // Attach meshes to owner
    if (WeaponOwner)
    {
        WeaponOwner->AttachWeaponMeshes(this);
    }
}

void AShooterWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
}

void AShooterWeapon::StopFiring()
()
)
{
    ServerStopFiring();
}

void AShooterWeapon::OnOwnerDestroyed(AActor* DestroyedActor)
{
    Destroy();
}

void AShooterWeapon::ServerStartFiring_Implementation()
{
    if (!HasAuthority()) return;
    bIsFiring = true;
    const float TimeSinceLastShot = GetWorld()->GetTimeSeconds() - TimeOfLastShot;
    if (TimeSinceLastShot > RefireRate)
    {
        Fire();
    }
    else if (bFullAuto)
    {
        GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, TimeSinceLastShot, false);
    }
}

void AShooterWeapon::ServerStopFiring_Implementation()
{
    if (HasAuthority()) { bIsFiring = false; GetWorld()->GetTimerManager().ClearTimer(RefireTimer); }
}

void AShooterWeapon::MulticastFireVisuals_Implementation()
(
)
(
    if (FiringMontage && WeaponOwner)
    {
        WeaponOwner->PlayFiringMontage(FiringMontage);
    }
    if (WeaponOwner)
    {
        WeaponOwner->AddWeaponRecoil(FiringRecoil);
    }
)

void AShooterWeapon::Fire()
(
)
(
    if (!bIsFiring) return;
    if (!WeaponOwner) return;
    FireProjectile(WeaponOwner->GetWeaponTargetLocation());
    TimeOfLastShot = GetWorld()->GetTimeSeconds();
    if (PawnOwner)
    {
        MakeNoise(ShotLoudness, PawnOwner, PawnOwner->GetActorLocation(), ShotNoiseRange, NoiseOwnerTag);
    }
    if (bFullAuto)
    {
        GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, RefireRate, false);
    }
    else
    {
        GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::FireCooldownExpired, RefireRate, false);
    }
)
