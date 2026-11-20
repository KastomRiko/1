#pragma once

#include "CoreMinimal.h"
#include "Variant_Shooter/ShooterGameMode.h"
#include "NetShooterGameMode.generated.h"

UCLASS()
class NETSHOOTER_API ANetShooterGameMode : public AShooterGameMode
{
    GENERATED_BODY()

public:
    ANetShooterGameMode();
};
