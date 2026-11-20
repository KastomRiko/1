// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterGameMode.generated.h"

class UShooterUI;

/**
 *  Simple GameMode for a first person shooter game
 *  Manages game UI
 *  Keeps track of team scores
 */
UCLASS(abstract)
class NETSHOOTER_API AShooterGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
protected:

	/** Type of UI widget to spawn */
	UPROPERTY(EditAnywhere, Category="Shooter")
	TSubclassOf<UShooterUI> ShooterUIClass;

protected:

	/** Determines how many local players should be spawned on game start */
	UPROPERTY(EditDefaultsOnly, Category="Local Multiplayer", meta = (ClampMin = 1, ClampMax = 4))
	int32 NumberOfLocalPlayers = 1;

	/** Used to assign players to different PlayerStarts in the level */
	int32 CurrentPlayerStartAssignment = 0;

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Assigns each joining player to one of the two teams */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** Team assigned to the next joining player */
	uint8 NextTeamByte = 0;

	/** Assigns a PlayerStart to a specific player */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

public:

	/** Constructor */
	AShooterGameMode();

	/** Increases the score for the given team */
	void IncrementTeamScore(uint8 TeamByte);

	/** Returns true if enemy NPCs should be used */
	bool ShouldSpawnEnemyNPCs() const;
};
