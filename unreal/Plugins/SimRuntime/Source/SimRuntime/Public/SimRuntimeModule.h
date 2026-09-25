// SimRuntimeModule.h — PLACEHOLDER (authored pre-editor; bring-up fixes expected).
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSimRuntime, Log, All);

class FSimRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
