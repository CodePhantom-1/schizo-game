// SimNpcDirector.cpp — see SimNpcDirector.h and
// docs/proposals/invented-ledger-npc-movement.md for the full mapping.
#include "SimNpcDirector.h"

#include "SimDayNight.h"
#include "SimNpc.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

#include "Engine/World.h"
#include "HAL/CriticalSection.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimNpcDirector, Log, All);

namespace
{
// Night starts once the household's lamps are lit (schedules.csv
// evening_meal_and_lamps, hour 20) and the watch mounts (hour 21); ends at
// the gatekeeper's dawn open (hour 6). INVENTED boundary — the kernel's
// schedule rows don't carry a day/night flag, so the director decides.
constexpr int32 kNightStartHour = 21;
constexpr int32 kNightEndHour = 6;

bool IsNightHour(int32 Hour)
{
	return Hour >= kNightStartHour || Hour < kNightEndHour;
}

// Role -> the place it works by day, for roles whose canon "own place" (if
// any) isn't where the role is actually found (people.csv source_ref notes
// "works" rather than "owns" for these; see the ledger doc). Market traders
// are pointed at the square rather than their individual stalls, per the
// track's own spec ("market for trader") — a deliberate simplification.
const TMap<FString, FString>& DayPlaceByRole()
{
	static const TMap<FString, FString> Table = {
		{TEXT("gatekeeper"), TEXT("moon_gate_place")},
		{TEXT("watchman"), TEXT("gate_watch_post_place")},
		{TEXT("market trader"), TEXT("market_square_place")},
		{TEXT("water-carrier"), TEXT("street_well_place")},
		{TEXT("tavern keeper"), TEXT("brewery_tavern_place")},
		{TEXT("cook-shop keeper"), TEXT("cookshop_place")},
		{TEXT("priest of the moon"), TEXT("temple_front_place")},
	};
	return Table;
}

FString Utf8ToFString(const char* Buf, int Len)
{
	return Len > 0 ? FString(UTF8_TO_TCHAR(Buf)) : FString();
}
}  // namespace

ASimNpcDirector::ASimNpcDirector()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASimNpcDirector::BeginPlay()
{
	Super::BeginPlay();
	// The other half of SimGameMode's one spawn line: the director owns
	// bringing the sun up too (UE-4's second actor), so the game mode never
	// needs a second line.
	GetWorld()->SpawnActor<ASimDayNight>();
}

void ASimNpcDirector::SpawnResidentsIfReady()
{
	SimWorld* Handle = USimWorldSubsystem::GetSimHandle();
	if (Handle == nullptr) return;  // kernel not up yet — Tick retries

	const int32 Count = sim_world_npc_count(Handle);
	for (int32 i = 0; i < Count; ++i)
	{
		char IdBuf[128];
		const int IdLen = sim_world_npc_id(Handle, i, IdBuf, sizeof(IdBuf));
		if (IdLen <= 0) continue;
		const FString NpcId = Utf8ToFString(IdBuf, IdLen);

		char RoleBuf[64];
		const int RoleLen = sim_world_npc_role(Handle, TCHAR_TO_UTF8(*NpcId), RoleBuf, sizeof(RoleBuf));
		if (RoleLen <= 0) continue;  // named leaders with no schedule role (the_prophet) sit out

		char CityBuf[64];
		const int CityLen = sim_world_npc_home_city(Handle, TCHAR_TO_UTF8(*NpcId), CityBuf, sizeof(CityBuf));
		const FString HomeCity = Utf8ToFString(CityBuf, CityLen);
		if (HomeCity != TEXT("city_of_the_moon")) continue;  // this track owns the slice's one street

		char NameBuf[64];
		const int NameLen = sim_world_npc_name(Handle, TCHAR_TO_UTF8(*NpcId), NameBuf, sizeof(NameBuf));
		const FString ResidentName = Utf8ToFString(NameBuf, NameLen);
		const FString Role = Utf8ToFString(RoleBuf, RoleLen);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASimNpc* Npc = GetWorld()->SpawnActor<ASimNpc>(ResolvePlaceLocation(NpcId), FRotator::ZeroRotator, Params);
		if (Npc == nullptr) continue;
		Npc->InitResident(NpcId, ResidentName, Role);
		Residents.Add(Npc);
	}
	UE_LOG(LogSimNpcDirector, Log, TEXT("Spawned %d City-of-the-Moon residents with a role."), Residents.Num());
}

FVector ASimNpcDirector::ResolvePlaceLocation(const FString& PlaceOrFallbackKey) const
{
	// First choice: the street agent's registry, once it exists in this
	// world — actors tagged Place:<id> (ASimStreetBuilder::GetPlaceLocation
	// is not on this branch yet; see the ledger doc for the join).
	const FName Tag(*FString::Printf(TEXT("Place:%s"), *PlaceOrFallbackKey));
	TArray<AActor*> Tagged;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, Tagged);
	if (Tagged.Num() > 0 && Tagged[0] != nullptr)
	{
		return Tagged[0]->GetActorLocation();
	}

	// Fallback: a deterministic hash of the id -> a spot along the grey-box
	// street's known extent (SimGameMode's two floor segments span roughly
	// X in [-2000, 4500], Y in [-500, 500]). Same id, same spot, every run.
	const uint32 Hash = FCrc::StrCrc32(*PlaceOrFallbackKey);
	const float TX = static_cast<float>(Hash % 1000u) / 1000.f;
	const float TY = static_cast<float>((Hash / 1000u) % 1000u) / 1000.f;
	return FVector(FMath::Lerp(-1800.f, 4300.f, TX), FMath::Lerp(-500.f, 500.f, TY), 100.f);
}

FVector ASimNpcDirector::ResolveDestination(const ASimNpc& Npc, int32 Hour, FString& OutScheduleId, FString& OutTaskText) const
{
	SimWorld* Handle = USimWorldSubsystem::GetSimHandle();
	OutScheduleId.Reset();
	OutTaskText.Reset();
	if (Handle != nullptr)
	{
		char TaskBuf[192];
		const int Len = sim_world_npc_task(Handle, TCHAR_TO_UTF8(*Npc.NpcId), Hour, TaskBuf, sizeof(TaskBuf));
		if (Len > 0)
		{
			FString Combined = Utf8ToFString(TaskBuf, Len);
			Combined.Split(TEXT("|"), &OutScheduleId, &OutTaskText);
		}
	}

	// Home at night (D-… invented glue, see the ledger doc): the resident's
	// own owned place (places.csv owner_person_id), or — for the roles that
	// own no place at all (gatekeeper, watchman, water-carrier) — a
	// deterministic fallback spot keyed on the npc id. KNOWN CEILING: some
	// residents' one owned place IS their workplace (e.g. traders own only
	// their stall), so they "sleep" there in this build until a distinct
	// home/work pair exists per resident.
	if (Handle != nullptr && IsNightHour(Hour))
	{
		char PlaceBuf[64];
		const int PlaceLen = sim_world_npc_place(Handle, TCHAR_TO_UTF8(*Npc.NpcId), PlaceBuf, sizeof(PlaceBuf));
		const FString OwnPlace = Utf8ToFString(PlaceBuf, PlaceLen);
		return ResolvePlaceLocation(OwnPlace.IsEmpty() ? Npc.NpcId : OwnPlace);
	}

	// Day: the role table first (works-but-doesn't-own roles + the spec's
	// literal "market for trader"/"gate for gatekeeper"/etc examples), else
	// the resident's own place (bakery for the baker, ...), else the
	// deterministic fallback.
	const FString RoleKey = Npc.ResidentRole.ToLower();
	if (const FString* Place = DayPlaceByRole().Find(RoleKey))
	{
		return ResolvePlaceLocation(*Place);
	}
	if (Handle != nullptr)
	{
		char PlaceBuf[64];
		const int PlaceLen = sim_world_npc_place(Handle, TCHAR_TO_UTF8(*Npc.NpcId), PlaceBuf, sizeof(PlaceBuf));
		const FString OwnPlace = Utf8ToFString(PlaceBuf, PlaceLen);
		if (!OwnPlace.IsEmpty())
		{
			return ResolvePlaceLocation(OwnPlace);
		}
	}
	return ResolvePlaceLocation(Npc.NpcId);
}

void ASimNpcDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Residents.Num() == 0)
	{
		SpawnResidentsIfReady();
		return;
	}

	const int32 Hour = FMath::FloorToInt(USimWorldSubsystem::GetSimHour());
	if (Hour < 0 || Hour == LastAppliedHour) return;  // no world yet, or already applied this hour
	LastAppliedHour = Hour;

	for (ASimNpc* Npc : Residents)
	{
		if (Npc == nullptr) continue;
		FString ScheduleId, TaskText;
		const FVector Dest = ResolveDestination(*Npc, Hour, ScheduleId, TaskText);
		Npc->SetTargetLocation(Dest);
		if (!ScheduleId.IsEmpty())
		{
			Npc->SetCurrentTask(ScheduleId, TaskText);
		}
	}
}
