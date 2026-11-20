// Copyright Epic Games, Inc. All Rights Reserved.

#include "ANetShooterCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Math/RotationMatrix.h"
#include "NetShooterWeapon.h"

ANetShooterCharacter::ANetShooterCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetMesh());
    FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
    FirstPersonCamera->bUsePawnControlRotation = true;

    ViewPointArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ViewPointArm"));
    ViewPointArm->SetupAttachment(GetRootComponent());
    ViewPointArm->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
    ViewPointArm->bUsePawnControlRotation = true;
    ViewPointArm->TargetArmLength = 0.0f;
    ViewPointArm->bEnableCameraLag = false;
}

void ANetShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    APlayerController* PC = Cast<APlayerController>(GetController());

    if (PC && DefaultMappingContext)
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
            {
                Subsystem->AddMappingContext(DefaultMappingContext, 0);
            }
        }
    }

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (MoveAction)
        {
            EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this,
                static_cast<void (ANetShooterCharacter::*)(const FVector2D&)>(&ANetShooterCharacter::DoMove));
        }

        if (LookAction)
        {
            EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ANetShooterCharacter::DoLook);
        }

        if (JumpAction)
        {
            EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ANetShooterCharacter::DoJumpStart);
            EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ANetShooterCharacter::DoJumpEnd);
        }
    }
}

void ANetShooterCharacter::BeginPlay()
{
    Super::BeginPlay();
}

void ANetShooterCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
}

FVector ANetShooterCharacter::GetWeaponCameraLocation() const
{
    return FirstPersonCamera ? FirstPersonCamera->GetComponentLocation() : GetActorLocation();
}

void ANetShooterCharacter::SetMovementEnabled(bool bEnabled)
{
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->SetMovementMode(bEnabled ? MOVE_Walking : MOVE_None);
    }
}

void ANetShooterCharacter::DisablePlayerInput()
{
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (PC->IsLocalController())
        {
            DisableInput(PC);
        }
    }
}

void ANetShooterCharacter::EnablePlayerInput()
{
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (PC->IsLocalController())
        {
            EnableInput(PC);
        }
    }
}

void ANetShooterCharacter::DoMove(const FVector2D& Value)
{
    if (Controller && Value.SizeSquared() > 0.0f)
    {
        const FRotator Rot = Controller->GetControlRotation();
        const FRotator YawRot(0.0f, Rot.Yaw, 0.0f);

        AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Value.Y);
        AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Value.X);
    }
}

void ANetShooterCharacter::DoLook(const FVector2D& Value)
{
    if (Controller)
    {
        AddControllerYawInput(Value.X);
        AddControllerPitchInput(Value.Y);
    }
}

void ANetShooterCharacter::DoJumpStart()
{
    Jump();
}

void ANetShooterCharacter::DoJumpEnd()
{
    StopJumping();
}

void ANetShooterCharacter::DoAim(float Yaw, float Pitch)
{
    if (Controller)
    {
        AddControllerYawInput(Yaw);
        AddControllerPitchInput(Pitch);
    }
}

void ANetShooterCharacter::DoMove(float Right, float Forward)
{
    DoMove(FVector2D(Right, Forward));
}

void ANetShooterCharacter::EquipWeapon(ANetShooterWeapon* Weapon)
{
    if (HasAuthority() && IsValid(Weapon))
    {
        Weapon->Equip(this);
    }
}
