// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/ShooterGameMode.h"
#include "ShooterGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "ShooterPlayerController.h"
#include "ShooterCharacter.h"
#include "ShooterNPC.h"

AShooterGameMode::AShooterGameMode()
{
	GameStateClass = AShooterGameState::StaticClass();
}

void AShooterGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Player 0 is created automatically; create any additional local split-screen players.
	// PostLogin assigns every player to the currently less populated team.
	for (int32 i = 2; i <= NumberOfLocalPlayers; ++i)
	{
		UGameplayStatics::CreatePlayer(GetWorld(), -1, true);
	}
}

void AShooterGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (AShooterPlayerController* ShooterPlayer = Cast<AShooterPlayerController>(NewPlayer))
	{
		int32 TeamCounts[2] = { 0, 0 };

		if (GetWorld())
		{
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* ExistingPlayer = It->Get();
				if (!IsValid(ExistingPlayer) || ExistingPlayer == NewPlayer)
				{
					continue;
				}

				if (const AShooterPlayerController* ExistingShooterPlayer = Cast<AShooterPlayerController>(ExistingPlayer))
				{
					const uint8 ExistingTeam = ExistingShooterPlayer->GetTeamByte();
					++TeamCounts[ExistingTeam == 0 ? 0 : 1];
				}
			}
		}

		uint8 AssignedTeam = NextTeamByte;
		if (TeamCounts[0] < TeamCounts[1])
		{
			AssignedTeam = 0;
		}
		else if (TeamCounts[1] < TeamCounts[0])
		{
			AssignedTeam = 1;
		}

		ShooterPlayer->SetTeam(AssignedTeam);
		NextTeamByte = static_cast<uint8>(1 - AssignedTeam);

		// The scoreboard widget must be created by each owning local player.
		if (ShooterPlayer->IsLocalController())
		{
			ShooterPlayer->InitializeScoreboard(ShooterUIClass);
		}
		else
		{
			ShooterPlayer->ClientInitializeScoreboard(ShooterUIClass);
		}
	}
}

AActor* AShooterGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// build the current player tag
	FName PlayerTag = FName(*FString::Printf(TEXT("Player%d"), CurrentPlayerStartAssignment));

	// find all player starts with the matching player tag
	TArray<AActor*> PlayerStarts;

	UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), APlayerStart::StaticClass(), PlayerTag, PlayerStarts);

	// increment the player start assignment index
	++CurrentPlayerStartAssignment;

	// if no PlayerStarts were found, default to all PlayerStarts instead
	if (PlayerStarts.IsEmpty())
	{
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), PlayerStarts);
	}

	// have we found at least one PlayerStart?
	if (!PlayerStarts.IsEmpty())
	{
		return PlayerStarts[ FMath::RandRange(0, PlayerStarts.Num() - 1) ];
	}

	// no PlayerStarts in the level
	return nullptr;
}

void AShooterGameMode::IncrementTeamScore(uint8 TeamByte)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>())
	{
		ShooterGameState->IncrementTeamScore(TeamByte);
	}
}

void AShooterGameMode::IncrementTeamScoreForController(AController* ScoringController)
{
	if (!HasAuthority() || !IsValid(ScoringController))
	{
		return;
	}

	uint8 ScoringTeam = 0;
	bool bFoundScoringTeam = false;

	if (APawn* ScoringPawn = ScoringController->GetPawn())
	{
		if (const AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(ScoringPawn))
		{
			ScoringTeam = ShooterCharacter->GetTeamByte();
			bFoundScoringTeam = true;
		}
		else if (const AShooterNPC* ShooterNPC = Cast<AShooterNPC>(ScoringPawn))
		{
			ScoringTeam = ShooterNPC->GetTeamByte();
			bFoundScoringTeam = true;
		}
	}

	// A PlayerController may already have a team assigned before its pawn is possessed.
	if (!bFoundScoringTeam)
	{
		if (const AShooterPlayerController* ShooterPlayer = Cast<AShooterPlayerController>(ScoringController))
		{
			ScoringTeam = ShooterPlayer->GetTeamByte();
			bFoundScoringTeam = true;
		}
	}

	if (bFoundScoringTeam)
	{
		IncrementTeamScore(ScoringTeam);
	}
}

bool AShooterGameMode::ShouldSpawnEnemyNPCs() const
{
	// Enemy NPCs are only used in standalone single-player matches.
	return GetNetMode() == NM_Standalone && NumberOfLocalPlayers < 2;
}
