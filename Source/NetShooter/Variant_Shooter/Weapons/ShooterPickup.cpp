// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterPickup.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ShooterWeaponHolder.h"
#include "ShooterWeapon.h"
#include "Engine/World.h"
#include "TimerManager.h"

AShooterPickup::AShooterPickup()
{
 	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// create the root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the collision sphere
	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere Collision"));
	SphereCollision->SetupAttachment(RootComponent);

	SphereCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 84.0f));
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SphereCollision->SetCollisionObjectType(ECC_WorldStatic);
	SphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SphereCollision->bFillCollisionUnderneathForNavmesh = true;

	// subscribe to the collision overlap on the sphere
	SphereCollision->OnComponentBeginOverlap.AddDynamic(this, &AShooterPickup::OnOverlap);

	// create the mesh
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SphereCollision);

	Mesh->SetCollisionProfileName(FName("NoCollision"));
}

void AShooterPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (FWeaponTableRow* WeaponData = WeaponType.GetRow<FWeaponTableRow>(FString()))
	{
		// set the mesh
		Mesh->SetStaticMesh(WeaponData->StaticMesh.LoadSynchronous());
	}
}

void AShooterPickup::BeginPlay()
{
	Super::BeginPlay();

	if (FWeaponTableRow* WeaponData = WeaponType.GetRow<FWeaponTableRow>(FString()))
	{
		// copy the weapon class
		WeaponClass = WeaponData->WeaponToSpawn;
	}
}

void AShooterPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void AShooterPickup::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only the server may grant a weapon or change pickup state.
	if (!HasAuthority() || !IsValid(OtherActor) || !WeaponClass || !GetWorld())
	{
		return;
	}

	// have we collided against a weapon holder?
	if (IShooterWeaponHolder* WeaponHolder = Cast<IShooterWeaponHolder>(OtherActor))
	{
		WeaponHolder->AddWeaponClass(WeaponClass);

		// hide this mesh
		SetActorHiddenInGame(true);

		// disable collision
		SetActorEnableCollision(false);

		// disable ticking
		SetActorTickEnabled(false);

		// A zero timer rate clears the timer instead of scheduling it.
		// Respawn immediately when no delay is configured.
		if (RespawnTime <= KINDA_SMALL_NUMBER)
		{
			RespawnPickup();
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AShooterPickup::RespawnPickup, RespawnTime, false);
		}
	}
}

void AShooterPickup::RespawnPickup()
{
	if (!HasAuthority())
	{
		return;
	}

	// Unhide on the server; the actor hidden state is replicated.
	SetActorHiddenInGame(false);
	MulticastRespawnPickup();
}

void AShooterPickup::MulticastRespawnPickup_Implementation()
{
	// Run the Blueprint respawn animation on every connected client.
	BP_OnRespawn();
}

void AShooterPickup::FinishRespawn()
{
	if (!HasAuthority())
	{
		return;
	}

	// enable collision
	SetActorEnableCollision(true);

	// enable tick
	SetActorTickEnabled(true);
}
