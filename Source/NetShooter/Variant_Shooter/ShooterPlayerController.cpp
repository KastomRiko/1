// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/ShooterPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "ShooterCharacter.h"
#include "ShooterBulletCounterUI.h"
#include "ShooterUI.h"
#include "ShooterGameState.h"
#include "NetShooter.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void AShooterPlayerController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (AShooterGameState* ShooterGameState = World->GetGameState<AShooterGameState>())
		{
			ShooterGameState->OnTeamScoreChanged.RemoveDynamic(this, &AShooterPlayerController::OnTeamScoreChanged);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AShooterPlayerController::ClientInitializeScoreboard_Implementation(TSubclassOf<UShooterUI> WidgetClass)
{
	InitializeScoreboard(WidgetClass);
}

void AShooterPlayerController::InitializeScoreboard(TSubclassOf<UShooterUI> WidgetClass)
{
	if (!IsLocalController() || !WidgetClass)
	{
		return;
	}

	// Keep the class in case this RPC arrives before the replicated GameState.
	PendingShooterUIClass = WidgetClass;

	if (!IsValid(ShooterUI))
	{
		ShooterUI = CreateWidget<UShooterUI>(this, WidgetClass);
		if (ShooterUI)
		{
			ShooterUI->AddToPlayerScreen(0);
		}
	}

	if (!IsValid(ShooterUI))
	{
		UE_LOG(LogNetShooter, Error, TEXT("Could not create Shooter scoreboard widget."));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AShooterGameState* ShooterGameState = World->GetGameState<AShooterGameState>())
		{
			ShooterGameState->OnTeamScoreChanged.AddUniqueDynamic(this, &AShooterPlayerController::OnTeamScoreChanged);

			const TArray<int32>& TeamScores = ShooterGameState->GetTeamScores();
			for (int32 TeamIndex = 0; TeamIndex < TeamScores.Num(); ++TeamIndex)
			{
				ShooterUI->BP_UpdateScore(static_cast<uint8>(TeamIndex), TeamScores[TeamIndex]);
			}
		}
	}
}

void AShooterPlayerController::OnShooterGameStateReady()
{
	if (IsLocalController() && PendingShooterUIClass)
	{
		InitializeScoreboard(PendingShooterUIClass);
	}
}

void AShooterPlayerController::OnTeamScoreChanged(uint8 TeamByte, int32 Score)
{
	if (IsValid(ShooterUI))
	{
		ShooterUI->BP_UpdateScore(TeamByte, Score);
	}
}

void AShooterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// add the input mapping contexts
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

			} else {

				UE_LOG(LogNetShooter, Error, TEXT("Could not spawn mobile controls widget."));

			}
		}

		// create the bullet counter widget and add it to the screen
		BulletCounterUI = CreateWidget<UShooterBulletCounterUI>(this, BulletCounterUIClass);

		if (BulletCounterUI)
		{
			BulletCounterUI->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogNetShooter, Error, TEXT("Could not spawn bullet counter widget."));

		}

		// The pawn may already be replicated before the local HUD is created.
		BindToShooterCharacter(GetPawn());
	}
}

void AShooterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (IsValid(InPawn))
	{
		// Respawn handling is authoritative and is registered only on the server.
		InPawn->OnDestroyed.AddUniqueDynamic(this, &AShooterPlayerController::OnPawnDestroyed);
	}

	if (AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(InPawn))
	{
		ShooterCharacter->Tags.AddUnique(PlayerPawnTag);
		ShooterCharacter->SetTeam(TeamByte);
	}

	BindToShooterCharacter(InPawn);
}

void AShooterPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	BindToShooterCharacter(GetPawn());
}

void AShooterPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
	UnbindFromShooterCharacter();
}

void AShooterPlayerController::BindToShooterCharacter(APawn* InPawn)
{
	AShooterCharacter* NewCharacter = Cast<AShooterCharacter>(InPawn);

	if (BoundShooterCharacter.Get() == NewCharacter)
	{
		if (IsValid(NewCharacter))
		{
			NewCharacter->RefreshHUDState();
		}
		return;
	}

	UnbindFromShooterCharacter();

	if (!IsValid(NewCharacter))
	{
		if (IsLocalController() && IsValid(BulletCounterUI))
		{
			BulletCounterUI->BP_UpdateBulletCounter(0, 0);
		}
		return;
	}

	BoundShooterCharacter = NewCharacter;
	NewCharacter->OnBulletCountUpdated.AddUniqueDynamic(this, &AShooterPlayerController::OnBulletCountUpdated);
	NewCharacter->OnDamaged.AddUniqueDynamic(this, &AShooterPlayerController::OnPawnDamaged);
	NewCharacter->RefreshHUDState();
}

void AShooterPlayerController::UnbindFromShooterCharacter()
{
	if (AShooterCharacter* PreviousCharacter = BoundShooterCharacter.Get())
	{
		PreviousCharacter->OnBulletCountUpdated.RemoveDynamic(this, &AShooterPlayerController::OnBulletCountUpdated);
		PreviousCharacter->OnDamaged.RemoveDynamic(this, &AShooterPlayerController::OnPawnDamaged);
	}

	BoundShooterCharacter.Reset();
}

void AShooterPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	// Respawning must be performed by the server, not independently on each client.
	if (!HasAuthority() || !GetWorld() || !CharacterClass)
	{
		return;
	}

	TArray<AActor*> PlayerStarts;

	// Prefer a spawn point assigned to this player's team.
	if (TeamByte < TeamTags.Num() && !TeamTags[TeamByte].IsNone())
	{
		UGameplayStatics::GetAllActorsOfClassWithTag(
			GetWorld(), APlayerStart::StaticClass(), TeamTags[TeamByte], PlayerStarts);
	}

	// If there is no matching team start, fall back to any PlayerStart.
	if (PlayerStarts.IsEmpty())
	{
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), PlayerStarts);
	}

	if (PlayerStarts.IsEmpty())
	{
		UE_LOG(LogNetShooter, Warning, TEXT("Cannot respawn player: no PlayerStart actors found."));
		return;
	}

	AActor* RandomPlayerStart = PlayerStarts[FMath::RandRange(0, PlayerStarts.Num() - 1)];
	const FTransform SpawnTransform = RandomPlayerStart->GetActorTransform();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (AShooterCharacter* RespawnedCharacter =
		GetWorld()->SpawnActor<AShooterCharacter>(CharacterClass, SpawnTransform, SpawnParams))
	{
		Possess(RespawnedCharacter);
	}
	else
	{
		UE_LOG(LogNetShooter, Warning, TEXT("Failed to spawn the respawn character."));
	}
}

void AShooterPlayerController::OnBulletCountUpdated(int32 MagazineSize, int32 Bullets)
{
	// update the UI
	if (BulletCounterUI)
	{
		BulletCounterUI->BP_UpdateBulletCounter(MagazineSize, Bullets);
	}
}

void AShooterPlayerController::OnPawnDamaged(float LifePercent)
{
	if (IsValid(BulletCounterUI))
	{
		BulletCounterUI->BP_Damaged(LifePercent);
	}
}

bool AShooterPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AShooterPlayerController::SetTeam(uint8 Team)
{
	TeamByte = Team;

	// if we already have a pawn, set its team
	if (AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(GetPawn()))
	{
		ShooterCharacter->SetTeam(Team);
	}
}
