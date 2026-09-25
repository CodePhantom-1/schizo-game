// SimWorldSubsystem.cpp — PLACEHOLDER (authored pre-editor; bring-up fixes expected).
#include "SimWorldSubsystem.h"

#include "SimRuntimeModule.h"

// The kernel's C ABI (kernel/include/sim/CApi.h) — extern "C", no STL coupling.
#include "sim/CApi.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"

namespace
{
	// Debug pacing override (0 = use SimDaysPerRealMinute): sim.DaysPerRealMinute
	TAutoConsoleVariable<float> CVarSimDaysPerRealMinute(
		TEXT("sim.DaysPerRealMinute"),
		0.f,
		TEXT("Real minutes of engine time per sim day (0 = use the subsystem's configured value)."));
}

namespace
{
	USimWorldSubsystem* GetSim(const UWorld* World)
	{
		return World ? World->GetSubsystem<USimWorldSubsystem>() : nullptr;
	}

	FString CanonDir()
	{
		// The canon ships as loose CSVs beside the game content (copied from
		// data/ue or db/canon at packaging — TODO(Phase 3 bring-up): staging step).
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Sim/canon"));
	}
}

void USimWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogSimRuntime, Log, TEXT("SimRuntime subsystem initialised on '%s' (creation is lazy — first tick after begin-play)."),
		GetWorld() ? *GetWorld()->GetName() : TEXT("<null>"));
	// Single-world policy, from evidence: the -game flow initialises this
	// subsystem on a transient 'Untitled' world AND the real 'OpenWorld' —
	// both pass IsGameWorld(). Creation is therefore LAZY and lives in Tick:
	// only the world that reaches begin-play creates the sim.
}

void USimWorldSubsystem::Deinitialize()
{
	if (SimHandle != nullptr)
	{
		sim_world_destroy(SimHandle);
		SimHandle = nullptr;
	}
	Super::Deinitialize();
}

void USimWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (SimHandle == nullptr)
	{
		// Lazy creation: only the world that actually reaches begin-play owns
		// the sim (the transient 'Untitled' world never does).
		if (bCanonFailed || World == nullptr || !World->HasBegunPlay())
		{
			return;
		}
		SimHandle = sim_world_create(TCHAR_TO_UTF8(*FString(FTCHARToUTF8(*CanonDir()).Length(), FTCHARToUTF8(*CanonDir()).Get())), 1);
		if (SimHandle == nullptr)
		{
			UE_LOG(LogSimRuntime, Error, TEXT("sim_world_create failed — canon missing at %s? Retries stopped."), *CanonDir());
			bCanonFailed = true;
			return;
		}
		char Season[32];
		sim_world_season(SimHandle, Season, sizeof(Season));
		UE_LOG(LogSimRuntime, Log, TEXT("Sim world created: day %lld, season %s."),
			sim_world_day(SimHandle), *FString(UTF8_TO_TCHAR(Season)));
	}
	SecondsSinceLastDay += DeltaTime;
	const float MinutesPerDay = CVarSimDaysPerRealMinute.GetValueOnGameThread() > 0.f
		? CVarSimDaysPerRealMinute.GetValueOnGameThread()
		: SimDaysPerRealMinute;
	const float SecondsPerDay = MinutesPerDay * 60.0f;
	while (SecondsSinceLastDay >= SecondsPerDay)
	{
		sim_world_advance_days(SimHandle, 1);
		SecondsSinceLastDay -= SecondsPerDay;
		UE_LOG(LogSimRuntime, Log, TEXT("Sim day %lld."), sim_world_day(SimHandle));
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

void USimWorldSubsystem::AdvanceSimDays(int32 Days)
{
	USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	if (Sim && Sim->SimHandle && Days > 0)
	{
		sim_world_advance_days(Sim->SimHandle, Days);
		UE_LOG(LogSimRuntime, Log, TEXT("Debug advance: %d days — now day %lld."), Days, sim_world_day(Sim->SimHandle));
	}
}

SimWorld* USimWorldSubsystem::GetSimHandle()
{
	USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	return Sim ? Sim->SimHandle : nullptr;
}

int64 USimWorldSubsystem::GetSimDay()
{
	const USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	return Sim && Sim->SimHandle ? sim_world_day(Sim->SimHandle) : -1;
}

FString USimWorldSubsystem::GetSimSeason()
{
	const USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	if (Sim == nullptr || Sim->SimHandle == nullptr)
	{
		return FString();
	}
	char Season[32];
	sim_world_season(Sim->SimHandle, Season, sizeof(Season));
	return FString(UTF8_TO_TCHAR(Season));
}

int64 USimWorldSubsystem::GetSimPrice(const FString& CityId, const FString& ItemId)
{
	const USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	if (Sim == nullptr || Sim->SimHandle == nullptr)
	{
		return -1;
	}
	const FTCHARToUTF8 City(*CityId);
	const FTCHARToUTF8 Item(*ItemId);
	return sim_world_price(Sim->SimHandle, TCHAR_TO_UTF8(*FString(City.Length(), City.Get())),
		TCHAR_TO_UTF8(*FString(Item.Length(), Item.Get())));
}

void USimWorldSubsystem::SetSimDrought(int32 Stage)
{
	USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	if (Sim && Sim->SimHandle)
	{
		sim_world_set_drought(Sim->SimHandle, Stage);
	}
}

int32 USimWorldSubsystem::GetSimDrought()
{
	const USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	return Sim && Sim->SimHandle ? sim_world_drought(Sim->SimHandle) : -1;
}
