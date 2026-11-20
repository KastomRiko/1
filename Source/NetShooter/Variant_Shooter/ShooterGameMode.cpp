// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/ShooterGameMode.h"
#include "ShooterGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "ShooterPlayerController.h"

AShooterGameMode::AShooterGameMode()
{
	GameStateClass = AShooterGameState::StaticClass();
}

void AShooterGameMode::BeginPlay()
{
	Super::BeginPlay();

	// create each additional local player.
	// Player 0 will be created automatically as part of regular game init
	for (int32 i = 2; i <= NumberOfLocalPlayers; ++i)
	{
		if (AShooterPlayerController* NewPlayer = Cast<AShooterPlayerController>(UGameplayStatics::CreatePlayer(GetWorld(), -1, true)))
		{
			NewPlayer->SetTeam(1 - (i % 2));
		}
	}


	// Local split-screen players occupy teams 0, 1, 0, 1...;
	// continue the same sequence for later network logins.
	NextTeamByte = static_cast<uint8>(NumberOfLocalPlayers % 2);
}

void AShooterGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (AShooterPlayerController* ShooterPlayer = Cast<AShooterPlayerController>(NewPlayer))
	{
		ShooterPlayer->SetTeam(NextTeamByte);
		NextTeamByte = static_cast<uint8>(1 - NextTeamByte);

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

bool AShooterGameMode::ShouldSpawnEnemyNPCs() const
{
	// only spawn enemy NPCs in single player mode
	return NumberOfLocalPlayers < 2;
}
