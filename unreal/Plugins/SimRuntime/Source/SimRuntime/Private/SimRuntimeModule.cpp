// SimRuntimeModule.cpp — PLACEHOLDER skeleton (authored pre-editor).
// The Phase 3 binding this module will grow into (plan v2 §6):
//   1. A WorldSubsystem owns one sim::WorldState (kernel/include/sim/World.hpp),
//      initialised from the shipped canon tables (db/canon/*.csv via DataTables
//      import or raw-file read at init).
//   2. The daily tick advances on the game clock (configurable real-minutes per
//      day; parent architecture §4.2 default 45 real min) and publishes
//      prices/facts/weather to the engine-side consumers each morning.
//   3. Player/NPC/UI modules read the WorldContext; NOTHING writes module state
//      except its own tick — the kernel's rule is the engine's rule.
#include "SimRuntimeModule.h"

DEFINE_LOG_CATEGORY(LogSimRuntime);

#define LOCTEXT_NAMESPACE "SimRuntime"

void FSimRuntimeModule::StartupModule()
{
	UE_LOG(LogSimRuntime, Warning,
		TEXT("SimRuntime skeleton loaded — the kernel binding is Phase 3 bring-up work (plan v2 §6)."));
}

void FSimRuntimeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSimRuntimeModule, SimRuntime)
