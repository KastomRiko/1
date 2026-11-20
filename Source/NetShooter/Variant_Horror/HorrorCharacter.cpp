// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variant_Horror/HorrorCharacter.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"

AHorrorCharacter::AHorrorCharacter()
{
	// create the spotlight
	SpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("SpotLight"));
	SpotLight->SetupAttachment(GetFirstPersonCameraComponent());

	SpotLight->SetRelativeLocationAndRotation(FVector(30.0f, 17.5f, -5.0f), FRotator(-18.6f, -1.3f, 5.26f));
	SpotLight->Intensity = 0.5;
	SpotLight->SetIntensityUnits(ELightUnits::Lumens);
	SpotLight->AttenuationRadius = 1050.0f;
	SpotLight->InnerConeAngle = 18.7f;
	SpotLight->OuterConeAngle = 45.24f;
}

void AHorrorCharacter::BeginPlay()
{
	Super::BeginPlay();

	// initialize sprint meter to max
	SprintMeter = FMath::Max(SprintTime, 0.0f);

	// Initialize the walk speed
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}

	// start the sprint tick timer with a safe, non-zero interval
	if (UWorld* World = GetWorld())
	{
		const float TickInterval = FMath::Max(SprintFixedTickTime, 0.001f);
		World->GetTimerManager().SetTimer(
			SprintTimer, this, &AHorrorCharacter::SprintFixedTick, TickInterval, true);
	}
}

void AHorrorCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// Clear the sprint timer while the world is still available.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SprintTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void AHorrorCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AHorrorCharacter::DoStartSprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AHorrorCharacter::DoEndSprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &AHorrorCharacter::DoEndSprint);
		}
	}
}

void AHorrorCharacter::DoStartSprint()
{
	// set the sprinting flag
	bSprinting = true;

	// are we out of recovery mode?
	if (!bRecovering)
	{
		// set the sprint walk speed
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = SprintSpeed;
		}

		// call the sprint state changed delegate
		OnSprintStateChanged.Broadcast(true);
	}
}

void AHorrorCharacter::DoEndSprint()
{
	// set the sprinting flag
	bSprinting = false;

	// are we out of recovery?
	if (!bRecovering)
	{
		// set the default walk speed
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = WalkSpeed;
		}

		// call the sprint state changed delegate
		OnSprintStateChanged.Broadcast(false);
	}
}

void AHorrorCharacter::SprintFixedTick()
{
	const float TickInterval = FMath::Max(SprintFixedTickTime, 0.001f);
	const float SafeSprintTime = FMath::Max(SprintTime, 0.0f);

	// are we out of recovery, still have stamina and are moving faster than our walk speed?
	if (bSprinting && !bRecovering && GetVelocity().Length() > WalkSpeed && SprintMeter > 0.0f)
	{
		// update the sprint meter
		SprintMeter = FMath::Max(SprintMeter - TickInterval, 0.0f);

		// have we run out of stamina?
		if (SprintMeter <= 0.0f)
		{
			// raise the recovering flag
			bRecovering = true;

			// set the recovering walk speed
			if (UCharacterMovementComponent* Movement = GetCharacterMovement())
			{
				Movement->MaxWalkSpeed = RecoveringWalkSpeed;
			}
		}
	}
	else
	{
		// Preserve the original one-second-per-second recovery when RecoveryTime is zero.
		// When configured, RecoveryTime controls how long a fully depleted meter takes to refill.
		const float RecoveryRate = RecoveryTime > KINDA_SMALL_NUMBER
			? SafeSprintTime / RecoveryTime
			: 1.0f;

		SprintMeter = FMath::Min(SprintMeter + TickInterval * RecoveryRate, SafeSprintTime);

		if (SprintMeter >= SafeSprintTime)
		{
			// lower the recovering flag
			bRecovering = false;

			// set the walk or sprint speed depending on whether the sprint button is down
			if (UCharacterMovementComponent* Movement = GetCharacterMovement())
			{
				Movement->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
			}

			// update the sprint state depending on whether the button is down or not
			OnSprintStateChanged.Broadcast(bSprinting);
		}
	}

	// broadcast the sprint meter updated delegate
	const float SafeSprintTimeForPercent = FMath::Max(SafeSprintTime, KINDA_SMALL_NUMBER);
	OnSprintMeterUpdated.Broadcast(FMath::Clamp(SprintMeter / SafeSprintTimeForPercent, 0.0f, 1.0f));
}
