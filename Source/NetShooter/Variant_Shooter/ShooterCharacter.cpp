// Copyright Epic Games, Inc. All Rights Reserved. 
 
#include "ShooterCharacter.h" 
 
#include "Weapons/ShooterWeapon.h"
// Copyright Epic Games, Inc. All Rights Reserved.


AShooterCharacter::AShooterCharacter()
{
    PawnNoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("Pawn Noise Emitter"));
    CurrentHP = MaxHP;
}

void AShooterCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority()) { CurrentHP = MaxHP; }
    OnDamaged.Broadcast(1.0f);
}

void AShooterCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    if (UEnhancedInputComponent* EIC=Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EIC->BindAction(FireAction,ETriggerEvent::Started,this,&AShooterCharacter::DoStartFiring);
        EIC->BindAction(FireAction,ETriggerEvent::Completed,this,&AShooterCharacter::DoStopFiring);
        EIC->BindAction(SwitchWeaponAction,ETriggerEvent::Triggered,this,&AShooterCharacter::DoSwitchWeapon);
    }
}

void AShooterCharacter::OnRep_CurrentHP()
{
    OnDamaged.Broadcast(FMath::Max(0.0f,CurrentHP/MaxHP));
}

void AShooterCharacter::OnRep_TeamByte()
{
}

void AShooterCharacter::OnRep_CurrentWeapon()
{
}

float AShooterCharacter::TakeDamage(float Damage,struct FDamageEvent const& DamageEvent,AController* EventInstigator,AActor* DamageCauser)
{
    if (!HasAuthority()) return 0.0f;
    if (CurrentHP <= 0.0f || IsDead()) return 0.0f;
    CurrentHP -= Damage;
    if (CurrentHP < 0.0f) CurrentHP = 0.0f;
    ForceNetUpdate();
    OnDamaged.Broadcast(FMath::Max(0.0f,CurrentHP/MaxHP));
    if (CurrentHP <= 0.0f) Die(EventInstigator);
    return Damage;
}

void AShooterCharacter::SetTeam(uint8 Team)
{
    if (!HasAuthority()) return;
    TeamByte = Team;
    ForceNetUpdate();
}

void AShooterCharacter::DoAim(float Yaw,float Pitch)
{
    if (!IsDead()) Super::DoAim(Yaw,Pitch);
}

void AShooterCharacter::DoMove(float Right,float Forward)
{
    if (!IsDead()) Super::DoMove(Right,Forward);
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
    if (CurrentWeapon && !IsDead()) CurrentWeapon->StartFiring();
}

void AShooterCharacter::DoStopFiring()
{
    if (CurrentWeapon && !IsDead()) CurrentWeapon->StopFiring();
}

void AShooterCharacter::DoSwitchWeapon()
{
    if (!HasAuthority()) return;
    if (OwnedWeapons.Num() > 1 && !IsDead() && CurrentWeapon)
    {
        CurrentWeapon->DeactivateWeapon();
        int32 idx = OwnedWeapons.IndexOfByKey(CurrentWeapon);
        idx = (idx == OwnedWeapons.Num()-1) ? 0 : idx+1;
        CurrentWeapon = OwnedWeapons[idx];
        CurrentWeapon->ActivateWeapon(PlayerTag);
    }
}

void AShooterCharacter::AttachWeaponMeshes(AShooterWeapon* Weapon)
{
    FAttachmentTransformRules AR(EAttachmentRule::SnapToTarget,false);
    Weapon->AttachToActor(this,AR);
    if (Weapon->GetFirstPersonMesh()) Weapon->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(),AR,FirstPersonWeaponSocket);
    if (Weapon->GetThirdPersonMesh())  Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(),AR,ThirdPersonWeaponSocket);
}

void AShooterCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
    if (Montage && GetFirstPersonMesh()) GetFirstPersonMesh()->PlayAnimation(Montage,false);
}

void AShooterCharacter::AddWeaponRecoil(float Recoil)
{
    AddControllerPitchInput(Recoil);
}

void AShooterCharacter::UpdateWeaponHUD(int32 CurrentAmmo,int32 MagazineSize)
{
    OnBulletCountUpdated.Broadcast(MagazineSize,CurrentAmmo);
}

FVector AShooterCharacter::GetWeaponTargetLocation()
{
    FHitResult OutHit;
    FVector Start = GetFirstPersonCameraComponent()->GetComponentLocation();
    FVector End = Start + GetFirstPersonCameraComponent()->GetForwardVector()*MaxAimDistance;
    FCollisionQueryParams QP; QP.AddIgnoredActor(this);
    GetWorld()->LineTraceSingleByChannel(OutHit,Start,End,ECC_Visibility,QP);
    return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
}

void AShooterCharacter::AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass)
{
    if (!HasAuthority()) return;
    AShooterWeapon* OW = FindWeaponOfType(WeaponClass);
    if (!OW)
    {
        FActorSpawnParameters SP; SP.Owner=this; SP.Instigator=this;
        SP.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AShooterWeapon* AW = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass,GetActorTransform(),SP);
        if (AW)
        {
            OwnedWeapons.Add(AW);
            if (CurrentWeapon) CurrentWeapon->DeactivateWeapon();
            CurrentWeapon = AW;
            CurrentWeapon->ActivateWeapon(PlayerTag);
        }
    }
}

void AShooterCharacter::OnWeaponActivated(AShooterWeapon* Weapon)
{
    OnBulletCountUpdated.Broadcast(Weapon->GetMagazineSize(),Weapon->GetBulletCount());
    if (Weapon->GetFirstPersonAnimInstanceClass()) GetFirstPersonMesh()->SetAnimInstanceClass(Weapon->GetFirstPersonAnimInstanceClass());
    if (Weapon->GetThirdPersonAnimInstanceClass()) GetMesh()->SetAnimInstanceClass(Weapon->GetThirdPersonAnimInstanceClass());
}

void AShooterCharacter::OnWeaponDeactivated(AShooterWeapon*){}
void AShooterCharacter::OnSemiWeaponRefire(){}

AShooterWeapon* AShooterCharacter::FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const
{
    for (AShooterWeapon* W : OwnedWeapons)
        if (W && W->IsA(WeaponClass)) return W;
    return nullptr;
}

void AShooterCharacter::Die(AController* Killer)
{
    if (!HasAuthority() || IsDead()) return;
    if (IsValid(CurrentWeapon)) CurrentWeapon->DeactivateWeapon();
    if (AShooterGameMode* GM=Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
        GM->IncrementTeamScore(TeamByte);
    Tags.Add(DeathTag);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    GetCharacterMovement()->SetComponentTickEnabled(false);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DisableInput(nullptr);
    OnBulletCountUpdated.Broadcast(0,0);
    BP_OnDeath();
    GetWorld()->GetTimerManager().SetTimer(RespawnTimer,this,&AShooterCharacter::OnRespawn,RespawnTime,false);
}

void AShooterCharacter::OnRespawn()
{
    Destroy();
}

bool AShooterCharacter::IsDead() const
{
    return CurrentHP <= 0.0f;
}

void AShooterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AShooterCharacter,CurrentHP);
    DOREPLIFETIME(AShooterCharacter,TeamByte);
    DOREPLIFETIME(AShooterCharacter,OwnedWeapons);
    DOREPLIFETIME_CONDITION(AShooterCharacter,CurrentWeapon,COND_OwnerOnly);
}
