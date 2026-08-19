#include "Deadbrick.h"
#include "Modules/ModuleManager.h"

class FDeadbrickGameModule final : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override
    {
        FDefaultGameModuleImpl::StartupModule();
        UE_LOG(LogTemp, Display, TEXT("DEADBRICK MODULE BUILD 2026-08-19-I LOTL-SAFE-BINDING-FPS-COMBAT-LIGHTING LOADED"));
    }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FDeadbrickGameModule, Deadbrick, "Deadbrick");
