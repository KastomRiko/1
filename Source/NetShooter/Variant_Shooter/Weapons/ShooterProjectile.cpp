// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"

AShooterProjectile::AShooterProjectile()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    RootComponent = CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision Component"));
    CollisionComponent->SetSphereRadius(16.0f);
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
    CollisionComponent->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Projectile Movement"));
    ProjectileMovement->InitialSpeed = 3000.0f;
    ProjectileMovement->MaxSpeed = 3000.0f;
    ProjectileMovement->bShouldBounce = true;

    HitDamageType = UDamageType::StaticClass();
}

void AShooterProjectile::BeginPlay()
{
    Super::BeginPlay();

    if (CollisionComponent && GetInstigator())
    {
        CollisionComponent->IgnoreActorWhenMoving(GetInstigator(), true);
    }
}

void AShooterProjectile::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(DestructionTimer);
    }

    Super::EndPlay(EndPlayReason);
}

void AShooterProjectile::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!HasAuthority() || bHit || !CollisionComponent || !GetWorld())
    {
        return;
    }

    bHit = true;
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MakeNoise(NoiseLoudness, GetInstigator(), GetActorLocation(), NoiseRange, NoiseTag);

    if (bExplodeOnHit)
    {
        ExplosionCheck(GetActorLocation());
    }
    else
    {
        ProcessHit(Other, OtherComp, Hit.ImpactPoint, -Hit.ImpactNormal);
    }

    MulticastProjectileHit(Hit);

    if (DeferredDestructionTime > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            DestructionTimer, this, &AShooterProjectile::OnDeferredDestruction,
            DeferredDestructionTime, false);
    }
    else
    {
        Destroy();
    }
}

void AShooterProjectile::MulticastProjectileHit_Implementation(const FHitResult& Hit)
{
    BP_OnProjectileHit(Hit);
}

void AShooterProjectile::ExplosionCheck(const FVector& ExplosionCenter)
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    TArray<FOverlapResult> Overlaps;
    FCollisionShape OverlapShape;
    OverlapShape.SetSphere(ExplosionRadius);

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
    ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    if (!bDamageOwner && GetOwner())
    {
        QueryParams.AddIgnoredActor(GetOwner());
    }

    GetWorld()->OverlapMultiByObjectType(Overlaps, ExplosionCenter, FQuat::Identity, ObjectParams, OverlapShape, QueryParams);

    TSet<AActor*> DamagedActors;

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* OverlappedActor = Overlap.GetActor();
        if (!IsValid(OverlappedActor) || DamagedActors.Contains(OverlappedActor))
        {
            continue;
        }

        DamagedActors.Add(OverlappedActor);

        const FVector ExplosionDirection = OverlappedActor->GetActorLocation() - ExplosionCenter;
        ProcessHit(OverlappedActor, Overlap.GetComponent(), ExplosionCenter, ExplosionDirection.GetSafeNormal());
    }
}

void AShooterProjectile::ProcessHit(AActor* HitActor, UPrimitiveComponent* HitComp, const FVector& HitLocation, const FVector& HitDirection)
{
    if (!HasAuthority())
    {
        return;
    }

    if (ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
    {
        const bool bIsOwner = HitCharacter == GetOwner() || HitCharacter == GetInstigator();
        if (!bIsOwner || bDamageOwner)
        {
            AController* InstigatorController = GetInstigator() ? GetInstigator()->GetController() : nullptr;
            UGameplayStatics::ApplyDamage(
                HitCharacter, HitDamage, InstigatorController, this,
                HitDamageType ? HitDamageType : UDamageType::StaticClass());
        }
    }

    if (HitComp && HitComp->IsSimulatingPhysics())
    {
        HitComp->AddImpulseAtLocation(HitDirection * PhysicsForce, HitLocation);
    }
}

void AShooterProjectile::OnDeferredDestruction()
{
    if (HasAuthority())
    {
        Destroy();
    }
}

void AShooterProjectile::SetNoiseTag(const FName& Tag)
{
    NoiseTag = Tag;
}
