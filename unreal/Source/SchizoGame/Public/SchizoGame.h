// SchizoGame.h — the game module (PLACEHOLDER: first compile at Phase 3 bring-up).
#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSchizoGame, Log, All);

class FSchizoGameModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
