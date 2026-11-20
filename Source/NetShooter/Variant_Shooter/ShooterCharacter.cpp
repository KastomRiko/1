// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterCharacter.h"

#include "Weapons/ShooterWeapon.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "ShooterGameMode.h"

AShooterCharacter::AShooterCharacter()
{
    FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
    FirstPersonMesh->SetupAttachment(FirstPersonCamera);
    FirstPersonMesh->SetCollisionProfileName(TEXT("NoCollision"));
    FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    FirstPersonMesh->bOnlyOwnerSee = true;
    FirstPersonMesh->CastShadow = false;

    PawnNoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("Pawn Noise Emitter"));
}

void AShooterCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        CurrentHP = MaxHP;
    }

    OnDamaged.Broadcast(MaxHP > 0.0f ? 1.0f : 0.0f);
}

void AShooterCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
    }

    Super::EndPlay(EndPlayReason);
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (FireAction)
        {
            EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AShooterCharacter::DoStartFiring);
            EIC->BindAction(FireAction, ETriggerEvent::Completed, this, &AShooterCharacter::DoStopFiring);
            EIC->BindAction(FireAction, ETriggerEvent::Canceled, this, &AShooterCharacter::DoStopFiring);
        }

        if (SwitchWeaponAction)
        {
            EIC->BindAction(SwitchWeaponAction, ETriggerEvent::Started, this, &AShooterCharacter::DoSwitchWeapon);
        }
    }
}

void AShooterCharacter::OnRep_CurrentHP()
{
    const float LifePercent = MaxHP > 0.0f ? FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f) : 0.0f;
    OnDamaged.Broadcast(LifePercent);
}

void AShooterCharacter::RefreshHUDState()
{
    const float LifePercent = MaxHP > 0.0f ? FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f) : 0.0f;
    OnDamaged.Broadcast(LifePercent);

    if (IsValid(CurrentWeapon))
    {
        OnBulletCountUpdated.Broadcast(CurrentWeapon->GetMagazineSize(), CurrentWeapon->GetBulletCount());
    }
    else
    {
        OnBulletCountUpdated.Broadcast(0, 0);
    }
}

void AShooterCharacter::OnRep_IsDead()
{
    if (bIsDead)
    {
        ApplyDeathState();
    }
}

void AShooterCharacter::ApplyDeathState()
{
    if (bDeathPresentationApplied)
    {
        return;
    }

    bDeathPresentationApplied = true;

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
        Movement->SetComponentTickEnabled(false);
    }

    if (GetCapsuleComponent())
    {
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // Only disable input on the locally controlled owning player. Passing a null
    // controller to DisableInput can affect the first local player in split-screen.
    if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        if (PlayerController->IsLocalController())
        {
            DisableInput(PlayerController);
        }
    }

    OnBulletCountUpdated.Broadcast(0, 0);
    BP_OnDeath();
}

void AShooterCharacter::OnRep_TeamByte()
{
}

void AShooterCharacter::OnRep_CurrentWeapon()
{
    if (CurrentWeapon)
    {
        OnWeaponActivated(CurrentWeapon);
    }
}

float AShooterCharacter::TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || Damage <= 0.0f || CurrentHP <= 0.0f || IsDead())
    {
        return 0.0f;
    }

    const float AppliedDamage = FMath::Min(Damage, CurrentHP);
    CurrentHP = FMath::Max(0.0f, CurrentHP - AppliedDamage);
    ForceNetUpdate();

    const float LifePercent = MaxHP > 0.0f ? FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f) : 0.0f;
    OnDamaged.Broadcast(LifePercent);

    if (CurrentHP <= 0.0f)
    {
        Die(EventInstigator);
    }

    return AppliedDamage;
}

void AShooterCharacter::SetTeam(uint8 Team)
{
    if (!HasAuthority())
    {
        return;
    }

    TeamByte = Team;
    ForceNetUpdate();
}

void AShooterCharacter::DoAim(float Yaw, float Pitch)
{
    if (!IsDead()) Super::DoAim(Yaw, Pitch);
}

void AShooterCharacter::DoMove(float Right, float Forward)
{
    if (!IsDead()) Super::DoMove(Right, Forward);
}

void AShooterCharacter::DoJumpStart()
{
    if (!IsDead()) Super::DoJumpStart();
}

void AShooterCharacter::DoJumpEnd()
{
    if (!IsDead()) Super::DoJumpEnd();
}

void AShooterCharacter::DoStartFiring()
{
    if (IsValid(CurrentWeapon) && !IsDead())
    {
        CurrentWeapon->StartFiring();
    }
}

void AShooterCharacter::DoStopFiring()
{
    if (IsValid(CurrentWeapon))
    {
        CurrentWeapon->StopFiring();
    }
}

void AShooterCharacter::DoSwitchWeapon()
{
    if (!IsDead())
    {
        ServerSwitchWeapon();
    }
}

void AShooterCharacter::ServerSwitchWeapon_Implementation()
{
    if (OwnedWeapons.Num() <= 1 || IsDead() || !IsValid(CurrentWeapon))
    {
        return;
    }

    CurrentWeapon->DeactivateWeapon();

    int32 Index = OwnedWeapons.IndexOfByKey(CurrentWeapon);
    if (Index == INDEX_NONE)
    {
        Index = 0;
    }
    else
    {
        Index = (Index + 1) % OwnedWeapons.Num();
    }

    CurrentWeapon = OwnedWeapons[Index];

    if (IsValid(CurrentWeapon))
    {
        CurrentWeapon->ActivateWeapon(PlayerTag);
    }

    ForceNetUpdate();
}

void AShooterCharacter::AttachWeaponMeshes(AShooterWeapon* Weapon)
{
    if (!IsValid(Weapon))
    {
        return;
    }

    const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, false);
    Weapon->AttachToActor(this, AttachRules);

    if (USkeletalMeshComponent* WeaponFirstPersonMesh = Weapon->GetFirstPersonMesh())
    {
        if (FirstPersonMesh)
        {
            WeaponFirstPersonMesh->AttachToComponent(FirstPersonMesh, AttachRules, FirstPersonWeaponSocket);
        }
        else
        {
            WeaponFirstPersonMesh->SetHiddenInGame(true);
        }
    }

    if (USkeletalMeshComponent* WeaponThirdPersonMesh = Weapon->GetThirdPersonMesh())
    {
        WeaponThirdPersonMesh->AttachToComponent(GetMesh(), AttachRules, ThirdPersonWeaponSocket);
    }
}

void AShooterCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
    if (Montage && FirstPersonMesh)
    {
        FirstPersonMesh->PlayAnimation(Montage, false);
    }
}

void AShooterCharacter::AddWeaponRecoil(float Recoil)
{
    if (IsLocallyControlled())
    {
        AddControllerPitchInput(Recoil);
    }
}

void AShooterCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
    OnBulletCountUpdated.Broadcast(MagazineSize, CurrentAmmo);
}

FVector AShooterCharacter::GetWeaponTargetLocation()
{
    if (!FirstPersonCamera)
    {
        return GetActorLocation() + GetActorForwardVector() * MaxAimDistance;
    }

    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End = Start + FirstPersonCamera->GetForwardVector() * MaxAimDistance;

    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShooterWeaponAim), true, this);
    QueryParams.AddIgnoredActor(this);

    if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams) && Hit.bBlockingHit)
    {
        return Hit.ImpactPoint;
    }

    return End;
}

void AShooterCharacter::AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass)
{
    if (!HasAuthority() || !WeaponClass || !GetWorld())
    {
        return;
    }

    if (AShooterWeapon* ExistingWeapon = FindWeaponOfType(WeaponClass))
    {
        if (CurrentWeapon && CurrentWeapon != ExistingWeapon)
        {
            CurrentWeapon->DeactivateWeapon();
        }

        CurrentWeapon = ExistingWeapon;
        CurrentWeapon->ActivateWeapon(PlayerTag);
        ForceNetUpdate();
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AShooterWeapon* NewWeapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);
    if (!IsValid(NewWeapon))
    {
        return;
    }

    OwnedWeapons.Add(NewWeapon);

    if (CurrentWeapon)
    {
        CurrentWeapon->DeactivateWeapon();
    }

    CurrentWeapon = NewWeapon;
    CurrentWeapon->ActivateWeapon(PlayerTag);
    ForceNetUpdate();
}

void AShooterCharacter::OnWeaponActivated(AShooterWeapon* Weapon)
{
    if (!IsValid(Weapon))
    {
        return;
    }

    OnBulletCountUpdated.Broadcast(Weapon->GetMagazineSize(), Weapon->GetBulletCount());

    if (FirstPersonMesh && Weapon->GetFirstPersonAnimInstanceClass())
    {
        FirstPersonMesh->SetAnimInstanceClass(Weapon->GetFirstPersonAnimInstanceClass());
    }

    if (GetMesh() && Weapon->GetThirdPersonAnimInstanceClass())
    {
        GetMesh()->SetAnimInstanceClass(Weapon->GetThirdPersonAnimInstanceClass());
    }
}

void AShooterCharacter::OnWeaponDeactivated(AShooterWeapon* Weapon)
{
}

void AShooterCharacter::OnSemiWeaponRefire()
{
}

AShooterWeapon* AShooterCharacter::FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const
{
    if (!WeaponClass)
    {
        return nullptr;
    }

    for (AShooterWeapon* OwnedWeapon : OwnedWeapons)
    {
        if (IsValid(OwnedWeapon) && OwnedWeapon->IsA(WeaponClass))
        {
            return OwnedWeapon;
        }
    }

    return nullptr;
}

void AShooterCharacter::Die(AController* Killer)
{
    if (!HasAuthority() || bIsDead)
    {
        return;
    }

    bIsDead = true;
    ForceNetUpdate();

    if (IsValid(CurrentWeapon))
    {
        CurrentWeapon->DeactivateWeapon();
    }

    if (GetWorld())
    {
        if (AShooterGameMode* GameMode = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
        {
            GameMode->IncrementTeamScoreForController(Killer);
        }
    }

    Tags.AddUnique(DeathTag);
    ApplyDeathState();

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AShooterCharacter::OnRespawn, RespawnTime, false);
    }
}

void AShooterCharacter::OnRespawn()
{
    Destroy();
}

bool AShooterCharacter::IsDead() const
{
    // The replicated death flag may arrive before the updated health value.
    return bIsDead || CurrentHP <= 0.0f;
}

void AShooterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AShooterCharacter, CurrentHP);
    DOREPLIFETIME(AShooterCharacter, bIsDead);
    DOREPLIFETIME(AShooterCharacter, TeamByte);
    DOREPLIFETIME(AShooterCharacter, OwnedWeapons);
    DOREPLIFETIME(AShooterCharacter, CurrentWeapon);
}
