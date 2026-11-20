// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variant_Horror/HorrorPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "NetShooterCameraManager.h"
#include "HorrorCharacter.h"
#include "HorrorUI.h"
#include "NetShooter.h"
#include "Widgets/Input/SVirtualJoystick.h"

AHorrorPlayerController::AHorrorPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ANetShooterCameraManager::StaticClass();
}

void AHorrorPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void AHorrorPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetupHorrorCharacterUI(InPawn);
}

void AHorrorPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	SetupHorrorCharacterUI(GetPawn());
}

void AHorrorPlayerController::SetupHorrorCharacterUI(APawn* InPawn)
{
	// Only the owning local player should create and bind its HUD.
	if (!IsLocalPlayerController())
	{
		return;
	}

	AHorrorCharacter* HorrorCharacter = Cast<AHorrorCharacter>(InPawn);

	// Create the HUD when a horror character is first possessed.
	if (HorrorCharacter && !HorrorUI)
	{
		HorrorUI = CreateWidget<UHorrorUI>(this, HorrorUIClass);
		if (HorrorUI)
		{
			HorrorUI->AddToPlayerScreen(0);
		}
		else
		{
			UE_LOG(LogNetShooter, Error, TEXT("Could not create Horror UI widget."));
		}
	}

	// Passing nullptr also unbinds the previous character after unpossession.
	if (HorrorUI)
	{
		HorrorUI->SetupCharacter(HorrorCharacter);
	}
}

void AHorrorPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}

		if (ShouldUseTouchControls())
		{
			// spawn the mobile controls widget
			MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

			if (MobileControlsWidget)
			{
				// add the controls to the player screen
				MobileControlsWidget->AddToPlayerScreen(0);
			}
			else
			{
				UE_LOG(LogNetShooter, Error, TEXT("Could not spawn mobile controls widget."));
			}
		}
	}
}

bool AHorrorPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
