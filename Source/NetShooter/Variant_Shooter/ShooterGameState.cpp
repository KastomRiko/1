// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterGameState.h"

#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "ShooterPlayerController.h"

void AShooterGameState::BeginPlay()
{
	Super::BeginPlay();

	// A client RPC can arrive before this replicated actor. Notify local controllers
	// now so they can finish creating and binding their scoreboard widgets.
	if (!HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (AShooterPlayerController* ShooterPlayer = Cast<AShooterPlayerController>(It->Get()))
				{
					if (ShooterPlayer->IsLocalController())
					{
						ShooterPlayer->OnShooterGameStateReady();
					}
				}
			}
		}
	}
}

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
