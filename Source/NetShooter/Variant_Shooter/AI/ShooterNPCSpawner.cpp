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

	if (SpawnedNPC)
	{
		// Subscribe to the death delegate.
		SpawnedNPC->OnPawnDeath.AddDynamic(this, &AShooterNPCSpawner::OnNPCDied);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("%s: failed to spawn NPC class '%s'."), *GetName(), *GetNameSafe(NPCClass.Get()));
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
