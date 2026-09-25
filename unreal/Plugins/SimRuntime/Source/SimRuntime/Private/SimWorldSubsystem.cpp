// SimWorldSubsystem.cpp — PLACEHOLDER (authored pre-editor; bring-up fixes expected).
#include "SimWorldSubsystem.h"

#include "SimRuntimeModule.h"

// The kernel's C ABI (kernel/include/sim/CApi.h) — extern "C", no STL coupling.
#include "sim/CApi.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// Debug pacing override (0 = use SimDaysPerRealMinute): sim.DaysPerRealMinute
	TAutoConsoleVariable<float> CVarSimDaysPerRealMinute(
		TEXT("sim.DaysPerRealMinute"),
		0.f,
		TEXT("Real minutes of engine time per sim day (0 = use the subsystem's configured value)."));

	// Start-of-day override for a NEW world (-1 = use SimStartHour): sim.StartHour
	TAutoConsoleVariable<float> CVarSimStartHour(
		TEXT("sim.StartHour"),
		-1.f,
		TEXT("Sim hour (0..24) a new world starts at (negative = use the subsystem's configured value)."));
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
		SimHandle = sim_world_create(TCHAR_TO_UTF8(*CanonDir()), 1);
		if (SimHandle == nullptr)
		{
			UE_LOG(LogSimRuntime, Error, TEXT("sim_world_create failed — canon missing at %s? Retries stopped."), *CanonDir());
			bCanonFailed = true;
			return;
		}
		// A fresh world starts at SimStartHour (default: morning), not midnight.
		const float StartHourCVar = CVarSimStartHour.GetValueOnGameThread();
		const float StartHour = FMath::Clamp(StartHourCVar >= 0.f ? StartHourCVar : SimStartHour, 0.f, 23.999f);
		SecondsSinceLastDay = (StartHour / 24.0) * SecondsPerDay();
		char Season[32];
		sim_world_season(SimHandle, Season, sizeof(Season));
		// Single-world policy evidence (bring-up TODO, docs/plan.md §6): this log
		// line fires at most once per world, only for the world that reaches
		// begin-play — the transient 'Untitled' world that Initialize() also saw
		// never gets here, so exactly one "Sim world created" line means exactly
		// one live kernel instance.
		UE_LOG(LogSimRuntime, Log, TEXT("Sim world created on world '%s': day %lld, season %s, start hour %.2f."),
			World ? *World->GetName() : TEXT("<null>"), sim_world_day(SimHandle), *FString(UTF8_TO_TCHAR(Season)), StartHour);
	}
	SecondsSinceLastDay += DeltaTime;
	const float DaySeconds = SecondsPerDay();
	while (SecondsSinceLastDay >= DaySeconds)
	{
		sim_world_advance_days(SimHandle, 1);
		SecondsSinceLastDay -= DaySeconds;
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

float USimWorldSubsystem::SecondsPerDay() const
{
	const float MinutesPerDay = CVarSimDaysPerRealMinute.GetValueOnGameThread() > 0.f
		? CVarSimDaysPerRealMinute.GetValueOnGameThread()
		: SimDaysPerRealMinute;
	return FMath::Max(MinutesPerDay, 0.01f) * 60.0f;
}

float USimWorldSubsystem::GetSimHour()
{
	const USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	if (Sim == nullptr || Sim->SimHandle == nullptr)
	{
		return -1.f;
	}
	return FMath::Clamp(static_cast<float>(24.0 * Sim->SecondsSinceLastDay / Sim->SecondsPerDay()), 0.f, 23.999f);
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

// --- needs ------------------------------------------------------------------

int32 USimWorldSubsystem::GetSimHunger(const FString& Actor)
{
	const USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	return Sim && Sim->SimHandle ? sim_world_hunger(Sim->SimHandle, TCHAR_TO_UTF8(*Actor)) : -1;
}

int32 USimWorldSubsystem::GetSimThirst(const FString& Actor)
{
	const USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	return Sim && Sim->SimHandle ? sim_world_thirst(Sim->SimHandle, TCHAR_TO_UTF8(*Actor)) : -1;
}

int32 USimWorldSubsystem::GetSimFatigue(const FString& Actor)
{
	const USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	return Sim && Sim->SimHandle ? sim_world_fatigue(Sim->SimHandle, TCHAR_TO_UTF8(*Actor)) : -1;
}

FString USimWorldSubsystem::GetSimNeedEffects(const FString& Actor)
{
	const USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	if (Sim == nullptr || Sim->SimHandle == nullptr)
	{
		return FString();
	}
	char Effects[256];
	sim_world_need_effects(Sim->SimHandle, TCHAR_TO_UTF8(*Actor), Effects, sizeof(Effects));
	return FString(UTF8_TO_TCHAR(Effects));
}

void USimWorldSubsystem::AdvanceSimNeeds(const FString& Actor, int32 Hours, bool bSleeping)
{
	USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	if (Sim && Sim->SimHandle)
	{
		sim_world_advance_needs(Sim->SimHandle, TCHAR_TO_UTF8(*Actor), Hours, bSleeping ? 1 : 0);
	}
}

int32 USimWorldSubsystem::EatSimItem(const FString& Actor, const FString& Item)
{
	USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	return Sim && Sim->SimHandle ? sim_world_eat(Sim->SimHandle, TCHAR_TO_UTF8(*Actor), TCHAR_TO_UTF8(*Item)) : -1;
}

int32 USimWorldSubsystem::DrinkSimItem(const FString& Actor, const FString& Item)
{
	USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	return Sim && Sim->SimHandle ? sim_world_drink(Sim->SimHandle, TCHAR_TO_UTF8(*Actor), TCHAR_TO_UTF8(*Item)) : -1;
}

// --- inventory ----------------------------------------------------------------

int32 USimWorldSubsystem::GetSimItemCount(const FString& Actor, const FString& Item)
{
	const USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	return Sim && Sim->SimHandle ? sim_world_item_count(Sim->SimHandle, TCHAR_TO_UTF8(*Actor), TCHAR_TO_UTF8(*Item)) : -1;
}

int32 USimWorldSubsystem::GiveSimItem(const FString& Actor, const FString& Item, int32 Qty)
{
	USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	return Sim && Sim->SimHandle ? sim_world_give_item(Sim->SimHandle, TCHAR_TO_UTF8(*Actor), TCHAR_TO_UTF8(*Item), Qty) : -1;
}

// --- crafting -------------------------------------------------------------------

int32 USimWorldSubsystem::CraftSim(const FString& Actor, const FString& Recipe, const FString& StationsSemicolonList, int32 Times)
{
	USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	if (Sim == nullptr || Sim->SimHandle == nullptr)
	{
		return -1;
	}
	return sim_world_craft(Sim->SimHandle, TCHAR_TO_UTF8(*Actor), TCHAR_TO_UTF8(*Recipe),
		StationsSemicolonList.IsEmpty() ? nullptr : TCHAR_TO_UTF8(*StationsSemicolonList), Times);
}

// --- schedule ---------------------------------------------------------------------

FString USimWorldSubsystem::GetSimTaskAt(const FString& Role, int32 Hour)
{
	const USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	if (Sim == nullptr || Sim->SimHandle == nullptr)
	{
		return FString();
	}
	char Task[128];
	const int32 Len = sim_world_task_at(Sim->SimHandle, TCHAR_TO_UTF8(*Role), Hour, Task, sizeof(Task));
	return Len >= 0 ? FString(UTF8_TO_TCHAR(Task)) : FString();
}

// --- population ---------------------------------------------------------------------

int32 USimWorldSubsystem::GetSimNpcCount()
{
	const USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	return Sim && Sim->SimHandle ? sim_world_npc_count(Sim->SimHandle) : -1;
}

// --- save / load --------------------------------------------------------------------

FString USimWorldSubsystem::SaveSlotPath(const FString& Slot)
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), Slot + TEXT(".simsave"));
}

bool USimWorldSubsystem::SaveSimGame(const FString& Slot)
{
	USimWorldSubsystem* Sim = GetSim(GEngine ? GEngine->GetCurrentPlayWorld() : nullptr);
	if (Sim == nullptr || Sim->SimHandle == nullptr || Slot.IsEmpty())
	{
		return false;
	}
	const FString Path = SaveSlotPath(Slot);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), /*Tree=*/true);
	if (sim_world_save(Sim->SimHandle, TCHAR_TO_UTF8(*Path)) != 0)
	{
		UE_LOG(LogSimRuntime, Error, TEXT("sim.Save %s failed (sim_world_save could not write %s)."), *Slot, *Path);
		return false;
	}
	// Sidecar: the sub-day hour, which the kernel snapshot (SIMSAVE) does not
	// carry — GetSimHour() is engine-clock state, not kernel-day state.
	const float Hour = GetSimHour();
	FFileHelper::SaveStringToFile(FString::SanitizeFloat(Hour), *(Path + TEXT(".hour")));
	UE_LOG(LogSimRuntime, Log, TEXT("sim.Save %s -> %s (day %lld, hour %.2f)."),
		*Slot, *Path, sim_world_day(Sim->SimHandle), Hour);
	return true;
}

bool USimWorldSubsystem::LoadSimGame(const FString& Slot)
{
	UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
	USimWorldSubsystem* Sim = GetSim(World);
	if (Sim == nullptr || Slot.IsEmpty())
	{
		return false;
	}
	const FString Path = SaveSlotPath(Slot);
	SimWorld* Loaded = sim_world_load(TCHAR_TO_UTF8(*CanonDir()), TCHAR_TO_UTF8(*Path));
	if (Loaded == nullptr)
	{
		UE_LOG(LogSimRuntime, Error, TEXT("sim.Load %s failed (missing/malformed save at %s)."), *Slot, *Path);
		return false;
	}
	if (Sim->SimHandle != nullptr)
	{
		sim_world_destroy(Sim->SimHandle);
	}
	Sim->SimHandle = Loaded;
	Sim->bCanonFailed = false;

	// Sidecar: restore the sub-day hour (falls back to StartHour if missing).
	FString HourText;
	float Hour = -1.f;
	if (FFileHelper::LoadFileToString(HourText, *(Path + TEXT(".hour"))))
	{
		Hour = FCString::Atof(*HourText);
	}
	if (Hour < 0.f || Hour >= 24.f)
	{
		const float StartHourCVar = CVarSimStartHour.GetValueOnGameThread();
		Hour = StartHourCVar >= 0.f ? StartHourCVar : Sim->SimStartHour;
	}
	Sim->SecondsSinceLastDay = (double)Hour / 24.0 * (double)Sim->SecondsPerDay();

	UE_LOG(LogSimRuntime, Log, TEXT("sim.Load %s <- %s (day %lld, hour %.2f)."),
		*Slot, *Path, sim_world_day(Sim->SimHandle), Hour);
	return true;
}
