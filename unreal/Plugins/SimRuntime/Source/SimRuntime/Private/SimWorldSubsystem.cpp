// SimWorldSubsystem.cpp — the per-map driver and facade over the
// game-instance-owned kernel world (USimGameInstanceSubsystem).
#include "SimWorldSubsystem.h"

#include "SimGameInstanceSubsystem.h"
#include "SimRuntimeModule.h"

// The kernel's C ABI (kernel/include/sim/CApi.h) — extern "C", no STL coupling.
#include "sim/CApi.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// Debug pacing override (0 = use SimDaysPerRealMinute): sim.DaysPerRealMinute
	// Unit: real minutes per sim day (the name is historical, see the header).
	TAutoConsoleVariable<float> CVarSimDaysPerRealMinute(
		TEXT("sim.DaysPerRealMinute"),
		0.f,
		TEXT("Real minutes of engine time per sim day (0 = use the subsystem's configured value)."));
}

namespace
{
	UWorld* PlayWorld()
	{
		return GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
	}

	UWorld* WorldOf(const UObject* WorldContextObject)
	{
		return WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	}

	USimWorldSubsystem* GetSim(const UWorld* World)
	{
		return World ? World->GetSubsystem<USimWorldSubsystem>() : nullptr;
	}

	/** The owner of the kernel world: the game instance of this world. */
	USimGameInstanceSubsystem* FindSimOwner(const UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<USimGameInstanceSubsystem>() : nullptr;
	}

	SimWorld* HandleIn(const UWorld* World)
	{
		const USimGameInstanceSubsystem* SimOwner = FindSimOwner(World);
		return SimOwner ? SimOwner->GetHandle() : nullptr;
	}
}

void USimWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogSimRuntime, Log, TEXT("SimRuntime subsystem initialised on '%s' (creation is lazy — first tick after begin-play; the world is owned by the game instance)."),
		GetWorld() ? *GetWorld()->GetName() : TEXT("<null>"));
	// Single-world policy, from evidence: the -game flow initialises this
	// subsystem on a transient 'Untitled' world AND the real 'OpenWorld' —
	// both pass IsGameWorld(). Creation is therefore LAZY and lives in Tick:
	// only the world that reaches begin-play creates the sim.
}

void USimWorldSubsystem::Deinitialize()
{
	// The kernel world belongs to the game instance (USimGameInstanceSubsystem)
	// and outlives this map: nothing to destroy here.
	Super::Deinitialize();
}

void USimWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	// Only a world that has begun play creates or drives the sim (the
	// transient 'Untitled' world never does). The owner is shared by every
	// world of this game instance, so this also keeps a not-yet-playing world
	// from advancing the clock.
	if (World == nullptr || !World->HasBegunPlay())
	{
		return;
	}
	USimGameInstanceSubsystem* SimOwner = FindSimOwner(World);
	if (SimOwner == nullptr)
	{
		return;
	}
	if (SimOwner->SimHandle == nullptr)
	{
		// Lazy creation on the first begun-play tick. After map travel the
		// owner already holds the world and this branch is skipped.
		if (SimOwner->bCanonFailed)
		{
			return;
		}
		if (!SimOwner->CreateWorld())
		{
			return;  // logged by CreateWorld; retries stopped
		}
		// Play begins in the morning (D-020): start the sub-day clock at StartHour.
		SimOwner->SecondsSinceLastDay = SecondsPerDay() * FMath::Clamp(StartHour, 0.f, 23.999f) / 24.0;
		char Season[32] = {};
		sim_world_season(SimOwner->SimHandle, Season, sizeof(Season));
		UE_LOG(LogSimRuntime, Log, TEXT("Sim world created from %s: day %lld, season %s, hour %.2f."),
			*USimGameInstanceSubsystem::CanonDir(), sim_world_day(SimOwner->SimHandle), *FString(UTF8_TO_TCHAR(Season)),
			FMath::Clamp(StartHour, 0.f, 23.999f));
	}
	SimOwner->SecondsSinceLastDay += DeltaTime;
	const float DaySeconds = SecondsPerDay();
	while (SimOwner->SecondsSinceLastDay >= DaySeconds)
	{
		sim_world_advance_days(SimOwner->SimHandle, 1);
		SimOwner->SecondsSinceLastDay -= DaySeconds;
		UE_LOG(LogSimRuntime, Log, TEXT("Sim day %lld."), sim_world_day(SimOwner->SimHandle));
	}
}

TStatId USimWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USimWorldSubsystem, STATGROUP_Tickables);
}

bool USimWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// The game world and the editor preview tick; pure-PIE--less worlds do not.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

float USimWorldSubsystem::SecondsPerDay() const
{
	const float MinutesPerDay = CVarSimDaysPerRealMinute.GetValueOnGameThread() > 0.f
		? CVarSimDaysPerRealMinute.GetValueOnGameThread()
		: SimDaysPerRealMinute;
	return FMath::Max(MinutesPerDay, 0.01f) * 60.0f;
}

// --- current-play-world getters (unchanged contracts) -----------------------

SimWorld* USimWorldSubsystem::GetSimHandle()
{
	return GetSimHandleFor(PlayWorld());
}

float USimWorldSubsystem::GetSimHour()
{
	return GetSimHourFor(PlayWorld());
}

int64 USimWorldSubsystem::GetSimDay()
{
	return GetSimDayFor(PlayWorld());
}

FString USimWorldSubsystem::GetSimSeason()
{
	return GetSimSeasonFor(PlayWorld());
}

int64 USimWorldSubsystem::GetSimPrice(const FString& CityId, const FString& ItemId)
{
	return GetSimPriceFor(PlayWorld(), CityId, ItemId);
}

void USimWorldSubsystem::SetSimDrought(int32 Stage)
{
	SetSimDroughtFor(PlayWorld(), Stage);
}

int32 USimWorldSubsystem::GetSimDrought()
{
	return GetSimDroughtFor(PlayWorld());
}

void USimWorldSubsystem::AdvanceSimDays(int32 Days)
{
	AdvanceSimDaysFor(PlayWorld(), Days);
}

// --- world-context getters ----------------------------------------------------

SimWorld* USimWorldSubsystem::GetSimHandleFor(const UObject* WorldContextObject)
{
	return HandleIn(WorldOf(WorldContextObject));
}

float USimWorldSubsystem::GetSimHourFor(const UObject* WorldContextObject)
{
	const UWorld* World = WorldOf(WorldContextObject);
	const USimWorldSubsystem* Sim = GetSim(World);
	const USimGameInstanceSubsystem* SimOwner = FindSimOwner(World);
	if (Sim == nullptr || SimOwner == nullptr || SimOwner->GetHandle() == nullptr)
	{
		return -1.f;
	}
	return FMath::Clamp(static_cast<float>(24.0 * SimOwner->GetSecondsSinceLastDay() / Sim->SecondsPerDay()), 0.f, 23.999f);
}

int64 USimWorldSubsystem::GetSimDayFor(const UObject* WorldContextObject)
{
	const SimWorld* Handle = HandleIn(WorldOf(WorldContextObject));
	return Handle ? sim_world_day(Handle) : -1;
}

FString USimWorldSubsystem::GetSimSeasonFor(const UObject* WorldContextObject)
{
	const SimWorld* Handle = HandleIn(WorldOf(WorldContextObject));
	if (Handle == nullptr)
	{
		return FString();
	}
	char Season[32] = {};
	if (sim_world_season(Handle, Season, sizeof(Season)) < 0)
	{
		return FString();
	}
	return FString(UTF8_TO_TCHAR(Season));
}

int64 USimWorldSubsystem::GetSimPriceFor(const UObject* WorldContextObject, const FString& CityId, const FString& ItemId)
{
	const SimWorld* Handle = HandleIn(WorldOf(WorldContextObject));
	if (Handle == nullptr)
	{
		return -1;
	}
	// TCHAR_TO_UTF8 temporaries live until the end of the full expression.
	return sim_world_price(Handle, TCHAR_TO_UTF8(*CityId), TCHAR_TO_UTF8(*ItemId));
}

void USimWorldSubsystem::SetSimDroughtFor(const UObject* WorldContextObject, int32 Stage)
{
	if (SimWorld* Handle = HandleIn(WorldOf(WorldContextObject)))
	{
		sim_world_set_drought(Handle, Stage);
	}
}

int32 USimWorldSubsystem::GetSimDroughtFor(const UObject* WorldContextObject)
{
	const SimWorld* Handle = HandleIn(WorldOf(WorldContextObject));
	return Handle ? sim_world_drought(Handle) : -1;
}

void USimWorldSubsystem::AdvanceSimDaysFor(const UObject* WorldContextObject, int32 Days)
{
	SimWorld* Handle = HandleIn(WorldOf(WorldContextObject));
	if (Handle && Days > 0)
	{
		sim_world_advance_days(Handle, Days);
		UE_LOG(LogSimRuntime, Log, TEXT("Debug advance: %d days — now day %lld."), Days, sim_world_day(Handle));
	}
}
