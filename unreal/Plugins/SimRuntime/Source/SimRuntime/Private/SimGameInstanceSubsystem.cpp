// SimGameInstanceSubsystem.cpp — owns the kernel world across map travel.
#include "SimGameInstanceSubsystem.h"

#include "SimRuntimeModule.h"

// The kernel's C ABI (kernel/include/sim/CApi.h) — extern "C", no STL coupling.
#include "sim/CApi.h"

#include "Misc/Paths.h"

FString USimGameInstanceSubsystem::CanonDir()
{
	// The canon ships as loose CSVs beside the game content: staged into
	// Content/Sim/canon by tools/stage_canon_for_ue.py, and packaged as non-UFS
	// files by Config/DefaultGame.ini (DirectoriesToAlwaysStageAsNonUFS).
	// ProjectContentDir() is relative to the binary directory in both editor
	// and packaged builds; the kernel's file reads resolve against the process
	// working directory, so hand it (and the log) an absolute path.
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Sim/canon")));
}

bool USimGameInstanceSubsystem::CreateWorld()
{
	if (SimHandle != nullptr)
	{
		return true;
	}
	if (bCanonFailed)
	{
		return false;
	}
	const FString Dir = CanonDir();
	// The kernel skips missing tables, so a wrong directory would still
	// "succeed" with an empty world: require a sentinel table first.
	if (!FPaths::FileExists(FPaths::Combine(Dir, TEXT("seasons.csv"))))
	{
		UE_LOG(LogSimRuntime, Error, TEXT("Canon missing at %s (no seasons.csv) — run tools/stage_canon_for_ue.py. The sim world was NOT created; retries stopped."), *Dir);
		bCanonFailed = true;
		return false;
	}
	SimHandle = sim_world_create(TCHAR_TO_UTF8(*Dir), 1);
	if (SimHandle == nullptr)
	{
		UE_LOG(LogSimRuntime, Error, TEXT("sim_world_create failed on the canon at %s. Retries stopped."), *Dir);
		bCanonFailed = true;
		return false;
	}
	SecondsSinceLastDay = 0.0;
	return true;
}

void USimGameInstanceSubsystem::Deinitialize()
{
	if (SimHandle != nullptr)
	{
		sim_world_destroy(SimHandle);
		SimHandle = nullptr;
	}
	Super::Deinitialize();
}
