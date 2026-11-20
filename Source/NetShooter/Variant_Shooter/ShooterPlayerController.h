// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShooterPlayerController.generated.h"

class UUserWidget;
class UInputMappingContext;
class AShooterCharacter;
class AShooterGameState;
class UShooterBulletCounterUI;
class UShooterUI;

/**
 *  Simple PlayerController for a first person shooter game
 *  Manages input mappings
 *  Respawns the player pawn when it's destroyed
 */
UCLASS(abstract, config="Game")
class NETSHOOTER_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:

	/** Input mapping contexts for this player */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Character class to respawn when the possessed pawn is destroyed */
	UPROPERTY(EditAnywhere, Category="Shooter|Respawn")
	TSubclassOf<AShooterCharacter> CharacterClass;

	/** Type of bullet counter UI widget to spawn */
	UPROPERTY(EditAnywhere, Category="Shooter|UI")
	TSubclassOf<UShooterBulletCounterUI> BulletCounterUIClass;

	/** Tag to grant the possessed pawn to flag it as the player */
	UPROPERTY(EditAnywhere, Category="Shooter|Player")
	FName PlayerPawnTag = FName("Player");

	/** Pointer to the bullet counter UI widget */
	UPROPERTY()
	TObjectPtr<UShooterBulletCounterUI> BulletCounterUI;

	/** Local scoreboard widget */
	UPROPERTY()
	TObjectPtr<UShooterUI> ShooterUI;

	/** Pawn whose destruction is currently allowed to trigger a respawn. */
	UPROPERTY(Transient)
	TObjectPtr<APawn> PawnWithRespawnBinding = nullptr;

	/** Team ID for this player */
	uint8 TeamByte = 0;

	/** Tags that identify each team for PlayerStart selection upon respawning */
	UPROPERTY(EditAnywhere, Category="Shooter|Team")
	TArray<FName> TeamTags;

protected:

	/** Gameplay Initialization */
	virtual void BeginPlay() override;

	/** Clean up scoreboard bindings */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;

	/** Pawn initialization on the authority */
	virtual void OnPossess(APawn* InPawn) override;

	/** Pawn replication on the owning client */
	virtual void OnRep_Pawn() override;

	/** Pawn unpossession */
	virtual void OnUnPossess() override;

	/** Bind HUD delegates for the current pawn */
	void BindToShooterCharacter(APawn* InPawn);
	void UnbindFromShooterCharacter();

	TWeakObjectPtr<AShooterCharacter> BoundShooterCharacter;

	/** Called if the possessed pawn is destroyed */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedActor);

	/** Called when the bullet count on the possessed pawn is updated */
	UFUNCTION()
	void OnBulletCountUpdated(int32 MagazineSize, int32 Bullets);

	/** Called when the possessed pawn is damaged */
	UFUNCTION()
	void OnPawnDamaged(float LifePercent);

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

public:

	/** Assigns a team ID to this player */
	void SetTeam(uint8 Team);

	/** Returns this player's server-assigned team ID */
	uint8 GetTeamByte() const { return TeamByte; }

	/** Creates and initializes the scoreboard on the owning local player */
	void InitializeScoreboard(TSubclassOf<UShooterUI> WidgetClass);

	/** Retries scoreboard initialization when the replicated GameState becomes available */
	void OnShooterGameStateReady();

	/** Widget class received from the server, retained until GameState is ready */
	UPROPERTY(Transient)
	TSubclassOf<UShooterUI> PendingShooterUIClass;

	/** Requests the owning client to create its local scoreboard */
	UFUNCTION(Client, Reliable)
	void ClientInitializeScoreboard(TSubclassOf<UShooterUI> WidgetClass);

protected:

	UFUNCTION()
	void OnTeamScoreChanged(uint8 TeamByte, int32 Score);
};
