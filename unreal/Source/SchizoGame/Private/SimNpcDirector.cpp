// SimNpcDirector.cpp — see SimNpcDirector.h. The kernel's per-person
// schedule resolution (Schedule.hpp person_task_at) already answers "where
// is this npc this hour"; this director turns that answer into spawn, walk
// and despawn orders — nothing here invents a schedule or a place mapping
// beyond the documented fallback below.
#include "SimNpcDirector.h"

#include "SimNpc.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimNpcDirector, Log, All);

namespace
{
// Relevance LOD (the spec's radius idea): only npc actors within this radius
// of the player exist; the rest are despawned until they matter again.
// sim.NpcRelevanceRadius, cm; 0 = everyone. Default covers the whole slice
// street (SimGameMode's floor spans ~6,500 cm) plus margin.
TAutoConsoleVariable<float> CVarSimNpcRelevanceRadius(
	TEXT("sim.NpcRelevanceRadius"),
	30000.f,
	TEXT("NPC relevance radius around the player, cm (0 = spawn all scheduled npcs)."));

// Hard cap on simultaneous npc actors, nearest first. sim.NpcMaxVisible; 0 = uncapped.
TAutoConsoleVariable<int32> CVarSimNpcMaxVisible(
	TEXT("sim.NpcMaxVisible"),
	48,
	TEXT("Maximum simultaneous NPC actors (0 = no cap). Nearest to the player are kept."));

// The grey-box street extent the fallback hashes into (SimGameMode's two
// floor segments: X roughly [-2000, 4500], Y in [-500, 500], floor top Z=0).
constexpr float kFallbackXMin = -1800.f;
constexpr float kFallbackXMax = 4300.f;
constexpr float kFallbackYMin = -500.f;
constexpr float kFallbackYMax = 500.f;
constexpr float kFloorZ = 100.f;  // capsule centre: half-height above the floor top

FString Utf8ToFString(const char* Buf, const int Len)
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
	UE_LOG(LogSimNpcDirector, Log, TEXT("Npc director up: waiting for the kernel world (it is created lazily on the first world tick)."));
}

ASimNpcDirector* ASimNpcDirector::GetInstance(const UObject* WorldContextObject)
{
	return Cast<ASimNpcDirector>(UGameplayStatics::GetActorOfClass(WorldContextObject, ASimNpcDirector::StaticClass()));
}

void ASimNpcDirector::SetPlaceResolver(ISimPlaceResolver* Resolver)
{
	PlaceResolver = Resolver;
	RefreshAll();  // the street's truth just changed: re-read every npc now
}

FSimResolvePlace& ASimNpcDirector::GetPlaceResolverDelegate()
{
	return PlaceResolverDelegate;  // bind on the returned reference, then call RefreshAll()
}

void ASimNpcDirector::RefreshAll()
{
	++ForceRefreshCookie;
}

void ASimNpcDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const int64 Day = USimWorldSubsystem::GetSimDayFor(World);
	if (Day < 0)
	{
		return;  // no kernel world yet — Tick retries until it exists
	}
	const int32 Hour = FMath::Clamp(FMath::FloorToInt(USimWorldSubsystem::GetSimHourFor(World)), 0, 23);

	// One refresh per (day, hour), plus one per RefreshAll (resolver wiring,
	// street spawn, debug day skips — anything that changes the answer).
	if (Day * 24 + Hour == LastAppliedKey && ForceRefreshCookie == LastCookie)
	{
		return;
	}
	LastAppliedKey = Day * 24 + Hour;
	LastCookie = ForceRefreshCookie;

	RefreshFromKernel();
}

namespace
{
/** One kernel npc this hour, as far as the street is concerned. */
struct FStreetNpc
{
	FString NpcId;
	FString PlaceId;     // kernel npc_place_at (a places.csv id)
	FString ScheduleId;  // kernel npc_schedule_at
	FString TaskText;    // kernel npc_task_at
	FVector Target = FVector::ZeroVector;
	float DistSq = 0.f;  // to the relevance centre, for culling and the cap
};
}  // namespace

void ASimNpcDirector::RefreshFromKernel()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	SimWorld* Handle = USimWorldSubsystem::GetSimHandleFor(World);
	if (Handle == nullptr)
	{
		return;
	}
	const int32 Hour = FMath::Clamp(FMath::FloorToInt(USimWorldSubsystem::GetSimHourFor(World)), 0, 23);

	const float Radius = FMath::Max(0.f, CVarSimNpcRelevanceRadius.GetValueOnGameThread());
	const int32 MaxVisible = FMath::Max(0, CVarSimNpcMaxVisible.GetValueOnGameThread());
	const FVector Centre = RelevanceCentre();

	// Pass 1: ask the kernel for every npc's hour; keep the ones on this street.
	TArray<FStreetNpc> Wanted;
	const int32 Count = sim_world_npc_count(Handle);
	for (int32 i = 0; i < Count; ++i)
	{
		char IdBuf[128];
		const int IdLen = sim_world_npc_id(Handle, i, IdBuf, sizeof(IdBuf));
		if (IdLen <= 0)
		{
			continue;
		}
		FStreetNpc Npc;
		Npc.NpcId = Utf8ToFString(IdBuf, IdLen);

		// Gone from the world entirely (slain, captive, sold east): off the
		// street for good — its actor (if any) is despawned below.
		char FateBuf[48];
		const int FateLen = sim_world_npc_fate(Handle, TCHAR_TO_UTF8(*Npc.NpcId), FateBuf, sizeof(FateBuf));
		if (FateLen < 0 || FateBuf[0] != '\0')
		{
			continue;
		}

		// The kernel's truth for this hour: no schedule row means the npc has
		// no day plan at all (named leaders like the_prophet) — no actor.
		char SchedBuf[96];
		const int SchedLen = sim_world_npc_schedule_at(Handle, TCHAR_TO_UTF8(*Npc.NpcId), Hour, SchedBuf, sizeof(SchedBuf));
		if (SchedLen < 0)
		{
			continue;
		}
		Npc.ScheduleId = Utf8ToFString(SchedBuf, SchedLen);

		// "" = unplaced this hour (off the street: asleep inside, away) — no actor.
		char PlaceBuf[96];
		const int PlaceLen = sim_world_npc_place_at(Handle, TCHAR_TO_UTF8(*Npc.NpcId), Hour, PlaceBuf, sizeof(PlaceBuf));
		if (PlaceLen < 0)
		{
			continue;
		}
		Npc.PlaceId = Utf8ToFString(PlaceBuf, PlaceLen);
		if (Npc.PlaceId.IsEmpty())
		{
			continue;
		}

		// A place this street does not know is off this street.
		FVector PlaceLocation;
		if (!ResolvePlaceLocation(Npc.PlaceId, PlaceLocation))
		{
			continue;
		}
		Npc.Target = PlaceLocation + ClusterOffsetFor(Npc.NpcId);
		Npc.DistSq = FVector::DistSquared2D(Npc.Target, Centre);
		if (Radius > 0.f && Npc.DistSq > FMath::Square(Radius))
		{
			continue;  // relevance LOD: too far from the player to matter
		}

		char TaskBuf[256];
		const int TaskLen = sim_world_npc_task_at(Handle, TCHAR_TO_UTF8(*Npc.NpcId), Hour, TaskBuf, sizeof(TaskBuf));
		Npc.TaskText = Utf8ToFString(TaskBuf, TaskLen);

		Wanted.Add(Npc);
	}

	// Pass 2: the visible cap, nearest first.
	if (MaxVisible > 0 && Wanted.Num() > MaxVisible)
	{
		Wanted.Sort([](const FStreetNpc& A, const FStreetNpc& B) { return A.DistSq < B.DistSq; });
		Wanted.SetNum(MaxVisible);
	}

	// Pass 3: despawn whoever is no longer wanted (left the street, culled,
	// capped out, dead), then spawn/walk whoever is.
	TSet<FString> Keep;
	Keep.Reserve(Wanted.Num());
	for (const FStreetNpc& Npc : Wanted)
	{
		Keep.Add(Npc.NpcId);
	}
	TArray<FString> ToRemove;
	for (const TPair<FString, TObjectPtr<ASimNpc>>& Pair : NpcActorsById)
	{
		if (!Keep.Contains(Pair.Key))
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FString& Id : ToRemove)
	{
		if (ASimNpc* Actor = NpcActorsById.FindAndRemoveChecked(Id))
		{
			UE_LOG(LogSimNpcDirector, Verbose, TEXT("Despawn %s (off the street this hour)."), *Id);
			Actor->Destroy();
		}
	}

	int32 Spawned = 0;
	for (const FStreetNpc& Npc : Wanted)
	{
		ASimNpc** Found = NpcActorsById.Find(Npc.NpcId);
		ASimNpc* Actor = Found ? Found->Get() : nullptr;
		if (Actor == nullptr)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Actor = World->SpawnActor<ASimNpc>(Npc.Target, FRotator::ZeroRotator, Params);
			if (Actor == nullptr)
			{
				UE_LOG(LogSimNpcDirector, Warning, TEXT("Spawn failed for npc %s."), *Npc.NpcId);
				continue;
			}
			Actor->InitSimIdentity(Npc.NpcId);
			NpcActorsById.Add(Npc.NpcId, Actor);
			++Spawned;
		}
		// The kernel's order for this hour: stand at the resolved place,
		// doing the kernel's task. The actor walks there on its own tick.
		Actor->WalkTo(Npc.Target);
		Actor->SetCurrentTask(Npc.ScheduleId, Npc.TaskText);
	}

	UE_LOG(LogSimNpcDirector, Log,
		TEXT("Hour refresh: %d/%d kernel npcs on the street (%d newly spawned, radius %.0f cm, cap %d)."),
		Wanted.Num(), Count, Spawned, Radius, MaxVisible);
}

bool ASimNpcDirector::ResolvePlaceLocation(const FString& PlaceId, FVector& OutLocation) const
{
	// 1. The street's registry, once wired (W6-B). Its answer is final: a
	//    registered registry that does not know a place means "off street".
	if (PlaceResolver != nullptr)
	{
		return PlaceResolver->ResolvePlaceLocation(PlaceId, OutLocation);
	}
	// 2. The delegate form, same contract.
	if (PlaceResolverDelegate.IsBound())
	{
		return PlaceResolverDelegate.Execute(PlaceId, OutLocation);
	}
	// 3. Any actor the street tagged Place:<id> — a street can expose its
	//    registry by tags alone, with no code coupling at all.
	const FName Tag(*FString::Printf(TEXT("Place:%s"), *PlaceId));
	TArray<AActor*> Tagged;
	UGameplayStatics::GetAllActorsWithTag(this, Tag, Tagged);
	if (Tagged.Num() > 0 && Tagged[0] != nullptr)
	{
		OutLocation = Tagged[0]->GetActorLocation();
		return true;
	}
	// 4. The pre-street fallback (on until the street is wired, see header):
	//    deterministic — the same place id lands on the same spot every run,
	//    so days look stable before the real street exists.
	if (!bHashPlaceFallback)
	{
		return false;  // the street is wired: unknown means off the street
	}
	const uint32 Hash = FCrc::StrCrc32(*PlaceId);
	const float TX = static_cast<float>(Hash % 1000u) / 1000.f;
	const float TY = static_cast<float>((Hash / 1000u) % 1000u) / 1000.f;
	OutLocation = FVector(
		FMath::Lerp(kFallbackXMin, kFallbackXMax, TX),
		FMath::Lerp(kFallbackYMin, kFallbackYMax, TY),
		kFloorZ);
	return true;
}

FVector ASimNpcDirector::ClusterOffsetFor(const FString& NpcId) const
{
	// Hash -> angle + radius on a small ring, so co-placed residents stand
	// apart. Deterministic: a resident keeps his offset at every place.
	const uint32 Hash = FCrc::StrCrc32(*NpcId);
	const float Angle = static_cast<float>(Hash % 360u) * (2.f * PI / 360.f);
	const float Radius = 40.f + static_cast<float>((Hash >> 9) % 80u);
	return FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
}

FVector ASimNpcDirector::RelevanceCentre() const
{
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return Pawn->GetActorLocation();
	}
	return FVector::ZeroVector;  // the street's origin until the pawn spawns
}
