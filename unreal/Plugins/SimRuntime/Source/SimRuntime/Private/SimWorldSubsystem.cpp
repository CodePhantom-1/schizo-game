// SimWorldSubsystem.cpp — PLACEHOLDER (authored pre-editor; bring-up fixes expected).
#include "SimWorldSubsystem.h"

#include "SimRuntimeModule.h"

// The kernel's C ABI (kernel/include/sim/CApi.h) — extern "C", no STL coupling.
#include "sim/CApi.h"

#include "Engine/World.h"
#include "Misc/Paths.h"

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

	const FTCHARToUTF8 Dir(*CanonDir());
	SimHandle = sim_world_create(TCHAR_TO_UTF8(*FString(Dir.Length(), Dir.Get())), 1);
	if (SimHandle == nullptr)
	{
		UE_LOG(LogSimRuntime, Error, TEXT("sim_world_create failed — canon missing at %s?"), *CanonDir());
		return;
	}
	UE_LOG(LogSimRuntime, Log, TEXT("Sim world created: day %lld, season %s."),
		sim_world_day(SimHandle),
		*FString(UTF8_TO_TCHAR("…"))); // TODO(Phase 3 bring-up): season read + log
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
	if (SimHandle == nullptr)
	{
		return;
	}
	SecondsSinceLastDay += DeltaTime;
	const float SecondsPerDay = SimDaysPerRealMinute * 60.0f;
	while (SecondsSinceLastDay >= SecondsPerDay)
	{
		sim_world_advance_days(SimHandle, 1);
		SecondsSinceLastDay -= SecondsPerDay;
		UE_LOG(LogSimRuntime, Verbose, TEXT("Sim day %lld."), sim_world_day(SimHandle));
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
