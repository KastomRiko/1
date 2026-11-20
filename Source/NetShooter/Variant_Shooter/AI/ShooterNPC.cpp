// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variant_Shooter/AI/ShooterNPC.h"

#include "ShooterWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "ShooterGameMode.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

void AShooterNPC::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && WeaponClass && GetWorld())
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        Weapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);
        if (IsValid(Weapon))
        {
            Weapon->ActivateWeapon(NAME_None);
        }
    }
}

void AShooterNPC::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(DeathTimer);
    }

    Super::EndPlay(EndPlayReason);
}

float AShooterNPC::TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || bIsDead || Damage <= 0.0f)
    {
        return 0.0f;
    }

    const float AppliedDamage = FMath::Min(Damage, FMath::Max(CurrentHP, 0.0f));
    CurrentHP = FMath::Max(0.0f, CurrentHP - AppliedDamage);
    ForceNetUpdate();

    if (CurrentHP <= 0.0f)
    {
        Die();
    }

    return AppliedDamage;
}

void AShooterNPC::AttachWeaponMeshes(AShooterWeapon* WeaponToAttach)
{
    if (!IsValid(WeaponToAttach))
    {
        return;
    }

    const FAttachmentTransformRules AttachmentRule(EAttachmentRule::SnapToTarget, false);
    WeaponToAttach->AttachToActor(this, AttachmentRule);

    if (USkeletalMeshComponent* FirstPersonWeaponMesh = WeaponToAttach->GetFirstPersonMesh())
    {
        FirstPersonWeaponMesh->SetHiddenInGame(true);
    }

    if (USkeletalMeshComponent* ThirdPersonWeaponMesh = WeaponToAttach->GetThirdPersonMesh())
    {
        ThirdPersonWeaponMesh->AttachToComponent(GetMesh(), AttachmentRule, ThirdPersonWeaponSocket);
        ThirdPersonWeaponMesh->SetHiddenInGame(false);
    }
}

void AShooterNPC::PlayFiringMontage(UAnimMontage* Montage)
{
}

void AShooterNPC::AddWeaponRecoil(float Recoil)
{
}

void AShooterNPC::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
}

FVector AShooterNPC::GetWeaponTargetLocation()
{
    const UCameraComponent* Camera = GetFirstPersonCameraComponent();
    if (!Camera)
    {
        return GetActorLocation() + GetActorForwardVector() * AimRange;
    }

    const FVector AimSource = Camera->GetComponentLocation();
    FVector AimDirection;

    if (IsValid(CurrentAimTarget))
    {
        FVector AimTarget = CurrentAimTarget->GetActorLocation();
        AimTarget.Z += FMath::RandRange(FMath::Min(MinAimOffsetZ, MaxAimOffsetZ), FMath::Max(MinAimOffsetZ, MaxAimOffsetZ));
        AimDirection = (AimTarget - AimSource).GetSafeNormal();
    }
    else
    {
        AimDirection = Camera->GetForwardVector();
    }

    AimDirection = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(AimDirection, AimVarianceHalfAngle);
    const FVector TraceEnd = AimSource + AimDirection * AimRange;

    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShooterNPCAim), true, this);
    QueryParams.AddIgnoredActor(this);

    if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, AimSource, TraceEnd, ECC_Visibility, QueryParams) && Hit.bBlockingHit)
    {
        return Hit.ImpactPoint;
    }

    return TraceEnd;
}

void AShooterNPC::AddWeaponClass(const TSubclassOf<AShooterWeapon>& InWeaponClass)
{
    if (!HasAuthority() || !InWeaponClass || !GetWorld())
    {
        return;
    }

    if (IsValid(Weapon))
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Weapon = GetWorld()->SpawnActor<AShooterWeapon>(InWeaponClass, GetActorTransform(), SpawnParams);
}

void AShooterNPC::OnWeaponActivated(AShooterWeapon* InWeapon)
{
}

void AShooterNPC::OnWeaponDeactivated(AShooterWeapon* InWeapon)
{
}

void AShooterNPC::OnSemiWeaponRefire()
{
    if (HasAuthority() && bIsShooting && IsValid(Weapon) && !bIsDead)
    {
        Weapon->StartFiring();
    }
}

void AShooterNPC::Die()
{
    if (!HasAuthority() || bIsDead)
    {
        return;
    }

    bIsDead = true;
    bIsShooting = false;
    ForceNetUpdate();

    if (IsValid(Weapon))
    {
        Weapon->StopFiring();
        Weapon->DeactivateWeapon();
    }

    Tags.AddUnique(DeathTag);
    OnPawnDeath.Broadcast();

    if (GetWorld())
    {
        if (AShooterGameMode* GameMode = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
        {
            GameMode->IncrementTeamScore(TeamByte);
        }
    }

    if (GetCapsuleComponent())
    {
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->StopActiveMovement();
        Movement->DisableMovement();
    }

    if (GetMesh())
    {
        GetMesh()->SetCollisionProfileName(RagdollCollisionProfile);
        GetMesh()->SetSimulatePhysics(true);
        GetMesh()->SetPhysicsBlendWeight(1.0f);
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(DeathTimer, this, &AShooterNPC::DeferredDestruction, DeferredDestructionTime, false);
    }
}

void AShooterNPC::DeferredDestruction()
{
    if (HasAuthority())
    {
        Destroy();
    }
}

void AShooterNPC::StartShooting(AActor* ActorToShoot)
{
    if (!HasAuthority() || bIsDead || !IsValid(Weapon))
    {
        return;
    }

    CurrentAimTarget = ActorToShoot;
    bIsShooting = true;
    Weapon->StartFiring();
}

void AShooterNPC::StopShooting()
{
    if (!HasAuthority())
    {
        return;
    }

    bIsShooting = false;

    if (IsValid(Weapon))
    {
        Weapon->StopFiring();
    }
}

void AShooterNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AShooterNPC, CurrentHP);
    DOREPLIFETIME(AShooterNPC, TeamByte);
    DOREPLIFETIME(AShooterNPC, Weapon);
    DOREPLIFETIME(AShooterNPC, bIsDead);
}
