// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ShooterGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShooterTeamScoreChanged, uint8, TeamByte, int32, Score);

/**
 * Replicated team scores for the Shooter game mode.
 */
UCLASS()
class NETSHOOTER_API AShooterGameState : public AGameStateBase
{
	GENERATED_BODY()

protected:

	UPROPERTY(ReplicatedUsing=OnRep_TeamScores, BlueprintReadOnly, Category="Shooter")
	TArray<int32> TeamScores;

	UFUNCTION()
	void OnRep_TeamScores();

public:

	UPROPERTY(BlueprintAssignable, Category="Shooter")
	FShooterTeamScoreChanged OnTeamScoreChanged;

	void IncrementTeamScore(uint8 TeamByte);

	const TArray<int32>& GetTeamScores() const { return TeamScores; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
