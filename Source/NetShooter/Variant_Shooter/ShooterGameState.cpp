// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterGameState.h"

#include "Net/UnrealNetwork.h"

void AShooterGameState::IncrementTeamScore(uint8 TeamByte)
{
	if (!HasAuthority())
	{
		return;
	}

	int32& Score = TeamScores.FindOrAdd(TeamByte);
	++Score;

	// Broadcast locally for listen-server players; clients receive the RepNotify.
	OnTeamScoreChanged.Broadcast(TeamByte, Score);
	ForceNetUpdate();
}

void AShooterGameState::OnRep_TeamScores()
{
	for (const TPair<uint8, int32>& TeamScore : TeamScores)
	{
		OnTeamScoreChanged.Broadcast(TeamScore.Key, TeamScore.Value);
	}
}

void AShooterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShooterGameState, TeamScores);
}
