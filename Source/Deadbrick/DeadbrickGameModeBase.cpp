#include "DeadbrickGameModeBase.h"
#include "Player/DeadbrickCharacter.h"
#include "UI/DeadbrickHUD.h"

ADeadbrickGameModeBase::ADeadbrickGameModeBase()
{
    DefaultPawnClass = ADeadbrickCharacter::StaticClass();
    HUDClass = ADeadbrickHUD::StaticClass();
    bStartPlayersAsSpectators = false;
}
