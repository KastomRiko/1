
#include "NetShooterWeapon.h"

#include "ANetShooterCharacter.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"

#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ANetShooterWeapon::ANetShooterWeapon()
{
    PrimaryActorTick.bCanEverTick = false;

    // Репликация оружия
    bReplicates = true;
    SetReplicateMovement(true);

    // Модель оружия
    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("WeaponMesh")
    );

    SetRootComponent(WeaponMesh);

    WeaponMesh->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );

    // Зона подбора
    PickupSphere = CreateDefaultSubobject<USphereComponent>(
        TEXT("PickupSphere")
    );

    PickupSphere->SetupAttachment(RootComponent);

    PickupSphere->SetSphereRadius(150.0f);

    PickupSphere->SetCollisionProfileName(
        TEXT("OverlapAllDynamic")
    );

    PickupSphere->SetGenerateOverlapEvents(true);

    PickupSphere->OnComponentBeginOverlap.AddDynamic(
        this,
        &ANetShooterWeapon::OnPickupOverlap
    );
}

void ANetShooterWeapon::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        Ammo = MaxAmmo;
    }
}

void ANetShooterWeapon::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(
        ANetShooterWeapon,
        Ammo
    );
}

void ANetShooterWeapon::OnPickupOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult
)
{
    // Подбор обрабатывается только сервером
    if (!HasAuthority())
    {
        return;
    }

    ANetShooterCharacter* Character =
        Cast<ANetShooterCharacter>(OtherActor);

    if (!IsValid(Character))
    {
        return;
    }

    Character->EquipWeapon(this);
}

void ANetShooterWeapon::Equip(
    ANetShooterCharacter* Character
)
{
    if (!HasAuthority() || !IsValid(Character))
    {
        return;
    }

    if (GetOwner() != nullptr)
    {
        return;
    }

    // Назначаем владельца
    SetOwner(Character);

    // Отключаем возможность повторного подбора
    PickupSphere->SetGenerateOverlapEvents(false);

    PickupSphere->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );

    WeaponMesh->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );

    // Прикрепляем оружие к скелету персонажа
    AttachToComponent(
        Character->GetMesh(),
        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        AttachSocketName
    );

    // После прикрепления отдельное движение оружия не нужно
    SetReplicateMovement(false);

    SetActorEnableCollision(false);
}

void ANetShooterWeapon::FireFromServer()
{
    // Все проверки выполняются на сервере
    if (!HasAuthority())
    {
        return;
    }

    ANetShooterCharacter* Character =
        Cast<ANetShooterCharacter>(GetOwner());

    if (!IsValid(Character))
    {
        return;
    }

    if (!Character->GetController())
    {
        return;
    }

    if (Ammo <= 0)
    {
        return;
    }

    if (!GetWorld())
    {
        return;
    }

    // Ограничиваем скорострельность
    const float CurrentTime =
        GetWorld()->GetTimeSeconds();

    if (CurrentTime - LastFireTime < FMath::Max(FireInterval, 0.01f))
    {
        return;
    }

    LastFireTime = CurrentTime;

    // Списываем патрон на сервере
    Ammo--;

    // Получаем направление взгляда персонажа
    FVector ViewLocation;
    FRotator ViewRotation;

    Character->GetActorEyesViewPoint(
        ViewLocation,
        ViewRotation
    );

    // Конечная точка трассировки
    const FVector TraceEnd =
        ViewLocation + ViewRotation.Vector() * Range;

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(NetShooterWeaponTrace),
        true,
        Character
    );

    QueryParams.AddIgnoredActor(this);

    FHitResult Hit;

    // Проверяем попадание
    const bool bHit =
        GetWorld()->LineTraceSingleByChannel(
            Hit,
            ViewLocation,
            TraceEnd,
            ECC_Visibility,
            QueryParams
        );

    if (bHit && Hit.GetActor())
    {
        // Передаём урон стандартной системе Unreal Engine
        UGameplayStatics::ApplyPointDamage(
            Hit.GetActor(),
            Damage,
            ViewRotation.Vector(),
            Hit,
            Character->GetController(),
            this,
            UDamageType::StaticClass()
        );

        UE_LOG(
            LogTemp,
            Log,
            TEXT("Weapon hit: %s"),
            *GetNameSafe(Hit.GetActor())
        );
    }
}
