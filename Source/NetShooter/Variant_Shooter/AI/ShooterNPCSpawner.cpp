// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/AI/ShooterNPCSpawner.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "TimerManager.h"
#include "ShooterNPC.h"
#include "Kismet/GameplayStatics.h"
#include "ShooterGameMode.h"

// Sets default values
AShooterNPCSpawner::AShooterNPCSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	// create the root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the reference spawn capsule
	SpawnCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Spawn Capsule"));
	SpawnCapsule->SetupAttachment(RootComponent);

	SpawnCapsule->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	SpawnCapsule->SetCapsuleSize(35.0f, 90.0f);
	SpawnCapsule->SetCollisionProfileName(FName("NoCollision"));

	SpawnDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("Spawn Direction"));
	SpawnDirection->SetupAttachment(RootComponent);
}

void AShooterNPCSpawner::BeginPlay()
{
	Super::BeginPlay();

	// NPCs must only be spawned by the authoritative server.
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	// ignore if enemies are disabled at the GameMode level
	if (AShooterGameMode* GM = Cast<AShooterGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		if (!GM->ShouldSpawnEnemyNPCs())
		{
			return;
		}
	}

	// ensure we don't spawn NPCs if our initial spawn count is zero
	if (SpawnCount > 0)
	{
		// schedule the first NPC spawn
		if (InitialSpawnDelay <= KINDA_SMALL_NUMBER)
		{
			SpawnNPC();
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &AShooterNPCSpawner::SpawnNPC, InitialSpawnDelay);
		}
	}
}

void AShooterNPCSpawner::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AShooterNPCSpawner::SpawnNPC()
{
	if (!HasAuthority() || !GetWorld() || SpawnCount <= 0)
	{
		return;
	}

	// Validate the spawn configuration before attempting to create an NPC.
	if (!IsValid(NPCClass) || !IsValid(SpawnCapsule))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: cannot spawn NPC because NPCClass or SpawnCapsule is invalid."), *GetName());
		return;
	}

	// Spawn the NPC at the reference capsule's transform.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AShooterNPC* SpawnedNPC = GetWorld()->SpawnActor<AShooterNPC>(NPCClass, SpawnCapsule->GetComponentTransform(), SpawnParams);

	if (IsValid(SpawnedNPC))
	{
		// A successful spawn starts a fresh failure sequence.
		ConsecutiveSpawnFailures = 0;

		// Subscribe to the death delegate.
		SpawnedNPC->OnPawnDeath.AddUniqueDynamic(this, &AShooterNPCSpawner::OnNPCDied);
	}
	else
	{
		// Keep SpawnCount unchanged: this NPC has not yet been created.
		// Retry a few times in case the spawn failure is transient. A non-zero
		// minimum delay prevents recursive immediate retries when RespawnDelay is 0.
		++ConsecutiveSpawnFailures;

		if (ConsecutiveSpawnFailures <= 3 && GetWorld())
		{
			const float RetryDelay = FMath::Max(RespawnDelay, 1.0f);
			UE_LOG(LogTemp, Warning,
				TEXT("%s: failed to spawn NPC class '%s' (attempt %d). Retrying in %.1f seconds."),
				*GetName(), *GetNameSafe(NPCClass.Get()), ConsecutiveSpawnFailures, RetryDelay);

			GetWorld()->GetTimerManager().SetTimer(
				SpawnTimer, this, &AShooterNPCSpawner::SpawnNPC, RetryDelay, false);
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("%s: failed to spawn NPC class '%s' after %d retries. Spawner stopped."),
				*GetName(), *GetNameSafe(NPCClass.Get()), ConsecutiveSpawnFailures);
		}
	}
}

void AShooterNPCSpawner::OnNPCDied()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	// decrease the spawn counter
	--SpawnCount;

	// is this the last NPC we should spawn?
	if (SpawnCount <= 0)
	{
		return;
	}

	// schedule the next NPC spawn
	if (RespawnDelay <= KINDA_SMALL_NUMBER)
	{
		SpawnNPC();
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &AShooterNPCSpawner::SpawnNPC, RespawnDelay);
	}
}
