#include "DeadbrickGameModeBase.h"
#include "Player/DeadbrickCharacter.h"

ADeadbrickGameModeBase::ADeadbrickGameModeBase()
{
    DefaultPawnClass = ADeadbrickCharacter::StaticClass();
    bStartPlayersAsSpectators = false;
}
