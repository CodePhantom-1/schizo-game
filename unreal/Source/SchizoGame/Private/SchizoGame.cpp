// SchizoGame.cpp — the game module (PLACEHOLDER: first compile at Phase 3 bring-up).
#include "SchizoGame.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogSchizoGame);

#define LOCTEXT_NAMESPACE "SchizoGame"

void FSchizoGameModule::StartupModule()
{
	UE_LOG(LogSchizoGame, Log, TEXT("SchizoGame module up — the world ticks in SimRuntime."));
}

void FSchizoGameModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_PRIMARY_GAME_MODULE(FSchizoGameModule, SchizoGame, "SchizoGame");
