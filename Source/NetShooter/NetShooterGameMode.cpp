#include "NetShooterGameMode.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "ShooterUI.h"

ANetShooterGameMode::ANetShooterGameMode()
{
    // Keep the Shooter variant's gameplay classes while using this network GameMode.
    static ConstructorHelpers::FClassFinder<APawn> ShooterCharacterClass(
        TEXT("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter"));
    if (ShooterCharacterClass.Class)
    {
        DefaultPawnClass = ShooterCharacterClass.Class;
    }

    static ConstructorHelpers::FClassFinder<APlayerController> ShooterPlayerControllerClass(
        TEXT("/Game/Variant_Shooter/Blueprints/BP_ShooterPlayerController"));
    if (ShooterPlayerControllerClass.Class)
    {
        PlayerControllerClass = ShooterPlayerControllerClass.Class;
    }

    static ConstructorHelpers::FClassFinder<UShooterUI> ShooterWidgetClass(
        TEXT("/Game/Variant_Shooter/UI/UI_Shooter"));
    if (ShooterWidgetClass.Class)
    {
        ShooterUIClass = ShooterWidgetClass.Class;
    }

    // Preserve seamless travel between maps.
    bUseSeamlessTravel = true;
}
