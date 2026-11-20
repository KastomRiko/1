// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterGameState.h"

#include "Net/UnrealNetwork.h"

void AShooterGameState::IncrementTeamScore(uint8 TeamByte)
{
	if (!HasAuthority())
	{
		return;
	}

	// Team IDs are used as array indices; expand for any newly introduced team.
	if (TeamScores.Num() <= static_cast<int32>(TeamByte))
	{
		TeamScores.SetNum(static_cast<int32>(TeamByte) + 1);
	}

	const int32 NewScore = ++TeamScores[static_cast<int32>(TeamByte)];

	// Broadcast locally for listen-server players; clients receive the RepNotify.
	OnTeamScoreChanged.Broadcast(TeamByte, NewScore);
	ForceNetUpdate();
}

void AShooterGameState::OnRep_TeamScores()
{
	for (int32 TeamIndex = 0; TeamIndex < TeamScores.Num(); ++TeamIndex)
	{
		OnTeamScoreChanged.Broadcast(static_cast<uint8>(TeamIndex), TeamScores[TeamIndex]);
	}
}

void AShooterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShooterGameState, TeamScores);
}
