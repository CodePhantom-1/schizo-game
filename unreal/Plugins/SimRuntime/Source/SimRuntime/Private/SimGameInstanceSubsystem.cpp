// SimGameInstanceSubsystem.cpp — owns the kernel world across map travel.
#include "SimGameInstanceSubsystem.h"

#include "SimRuntimeModule.h"

// The kernel's C ABI (kernel/include/sim/CApi.h) — extern "C", no STL coupling.
#include "sim/CApi.h"

#include "Misc/Paths.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

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

bool USimGameInstanceSubsystem::CanonReady()
{
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
	return true;
}

bool USimGameInstanceSubsystem::CreateWorld()
{
	if (SimHandle != nullptr)
	{
		return true;
	}
	if (!NewWorld(1))
	{
		bCanonFailed = true;  // retries stopped: logged by NewWorld/CanonReady
		return false;
	}
	return true;
}

bool USimGameInstanceSubsystem::NewWorld(uint64 Seed)
{
	if (!CanonReady())
	{
		return false;
	}
	SimWorld* Fresh = sim_world_create(TCHAR_TO_UTF8(*CanonDir()), Seed);
	if (Fresh == nullptr)
	{
		UE_LOG(LogSimRuntime, Error, TEXT("sim_world_create failed on the canon at %s."), *CanonDir());
		return false;
	}
	if (SimHandle != nullptr)
	{
		sim_world_destroy(SimHandle);
	}
	SimHandle = Fresh;
	SecondsSinceLastDay = 0.0;
	return true;
}

bool USimGameInstanceSubsystem::SaveToString(FString& Out) const
{
	if (SimHandle == nullptr)
	{
		return false;
	}
	const int Len = sim_world_save_to_buffer(SimHandle, nullptr, 0);
	if (Len < 0)
	{
		return false;
	}
	TArray<ANSICHAR> Buf;
	Buf.SetNumZeroed(Len + 1);
	if (sim_world_save_to_buffer(SimHandle, Buf.GetData(), Len + 1) != Len)
	{
		return false;
	}
	Out = UTF8_TO_TCHAR(Buf.GetData());
	return true;
}

bool USimGameInstanceSubsystem::LoadFromString(const FString& Data)
{
	if (!CanonReady())
	{
		return false;
	}
	SimWorld* Loaded = sim_world_load_from_buffer(TCHAR_TO_UTF8(*CanonDir()), TCHAR_TO_UTF8(*Data));
	if (Loaded == nullptr)
	{
		UE_LOG(LogSimRuntime, Error, TEXT("Load refused: not a SchizoGame save (or made from a different canon). The current world continues."));
		return false;
	}
	if (SimHandle != nullptr)
	{
		sim_world_destroy(SimHandle);
	}
	SimHandle = Loaded;
	return true;
}

void USimGameInstanceSubsystem::SetSecondsSinceLastDay(double Seconds)
{
	SecondsSinceLastDay = FMath::Max(0.0, Seconds);
}

USimGameInstanceSubsystem* USimGameInstanceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<USimGameInstanceSubsystem>() : nullptr;
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
