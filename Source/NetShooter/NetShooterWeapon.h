
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NetShooterWeapon.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UPrimitiveComponent;
class ANetShooterCharacter;

struct FHitResult;

UCLASS()
class NETSHOOTER_API ANetShooterWeapon : public AActor
{
    GENERATED_BODY()

public:
    ANetShooterWeapon();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    /** Подключение оружия к персонажу */
    void Equip(ANetShooterCharacter* Character);

    /** Выстрел, выполняемый сервером */
    void FireFromServer();

    /** Текущее количество патронов */
    UFUNCTION(BlueprintPure, Category = "Weapon")
    int32 GetAmmo() const
    {
        return Ammo;
    }

protected:
    virtual void BeginPlay() override;

    /** Модель оружия */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> WeaponMesh;

    /** Зона подбора оружия */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> PickupSphere;

    /** Урон от одного попадания */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    float Damage = 25.0f;

    /** Дальность стрельбы */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    float Range = 10000.0f;

    /** Минимальный интервал между выстрелами */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    float FireInterval = 0.15f;

    /** Максимальное количество патронов */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    int32 MaxAmmo = 30;

    /** Текущее количество патронов */
    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    int32 Ammo = 30;

    /** Имя сокета на скелете персонажа */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    FName AttachSocketName = TEXT("hand_r");

    /** Время последнего выстрела */
    float LastFireTime = -1000.0f;

    /** Обработка подбора */
    UFUNCTION()
    void OnPickupOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );
};
