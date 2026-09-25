// SimGameMode.cpp — wires the slice together: the street (W6-B), the
// residents and the sun (W6-C), the verbs and HUD (W6-A, via the player
// controller), and the sim clock on screen.
#include "SimGameMode.h"
#include "SimWorldSubsystem.h"

// The kernel's C API through SimRuntime's public include path.
#include "SimDayNight.h"  // W6-C: the sun and sky on the sim clock
#include "SimNpcDirector.h"  // W6-C: the residents, spawned from kernel schedules
#include "SimPlayerController.h"
#include "SimStreetBuilder.h"  // W6-B: the Moon Gate Quarter from the kit
#include "SimTablet.h"
#include "sim/CApi.h"

#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimGameMode, Log, All);

namespace
{
	// The street is the street's own map: every kernel place id the director
	// asks about resolves through the builder's registry (CSV row order, so
	// the same canon always lays out the same quarter).
	struct FStreetPlaceResolver final : public ISimPlaceResolver
	{
		bool ResolvePlaceLocation(const FString& PlaceId, FVector& OutLocation) const override
		{
			return ASimStreetBuilder::GetPlaceLocation(FName(*PlaceId), OutLocation);
		}
	};

	// Static storage: the director holds a raw pointer, so the resolver must
	// outlive it for the whole process.
	FStreetPlaceResolver GStreetPlaceResolver;
}

ASimGameMode::ASimGameMode()
{
	// The slice's player controller arrives with its verbs and HUD; the
	// engine's default pawn is enough to walk the street.
	PlayerControllerClass = ASimPlayerController::StaticClass();
	// The clock on screen needs the game mode to tick (actors don't by default).
	PrimaryActorTick.bCanEverTick = true;
}

void ASimGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Exactly one director and one day/night actor per world, spawned here
	// (never in a map, so no authored level is required). Both poll the
	// kernel — it is created lazily on the first world-subsystem tick after
	// begin play — so spawn order needs no care. The director resolves
	// places through the street's registry; the hash fallback is for the
	// pre-street world only.
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ASimNpcDirector* Director = World->SpawnActor<ASimNpcDirector>(FVector::ZeroVector, FRotator::ZeroRotator, Params))
		{
			Director->SetPlaceResolver(&GStreetPlaceResolver);
			Director->SetHashPlaceFallbackEnabled(false);
		}
		else
		{
			UE_LOG(LogSimGameMode, Warning, TEXT("Npc director spawn failed — the street stays empty."));
		}
		if (World->SpawnActor<ASimDayNight>(FVector::ZeroVector, FRotator::ZeroRotator, Params) == nullptr)
		{
			UE_LOG(LogSimGameMode, Warning, TEXT("Day/night actor spawn failed — the light stays static."));
		}
	}
}

void ASimGameMode::StartPlay()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// --- the street (W6-B) ---------------------------------------------------
	// Built BEFORE Super::StartPlay(): the pawn must find our PlayerStart,
	// not the engine's fallback. The builder owns the ground, the kit meshes
	// and the tagged sun/sky (SimDayNight finds those instead of spawning
	// its own — the street is lit once).
	const FSimStreetBuildResult Street = ASimStreetBuilder::BuildQuarter(World);
	if (!Street.bOk)
	{
		UE_LOG(LogSimGameMode, Warning, TEXT("The street failed to build — the quarter is missing."));
	}
	else
	{
		UE_LOG(LogSimGameMode, Log,
			TEXT("The quarter stands: %d places, %d door slots (kit meshes: %s)."),
			Street.NumPlacesBuilt, Street.NumDoorSlots, Street.bKitMeshesFound ? TEXT("yes") : TEXT("no — engine cubes"));
	}

	// The first readable thing: a clay tablet by the gate (notes L198).
	if (World->SpawnActor<ASimTablet>(Street.GateLocation + FVector(300, 0, 140), FRotator(0, 90, 0)) == nullptr)
	{
		UE_LOG(LogSimGameMode, Warning, TEXT("tablet spawn failed"));
	}

	// A PlayerStart so the pawn has somewhere to be: facing through the gate.
	StreetStart = World->SpawnActor<APlayerStart>(Street.GateLocation + FVector(500, 0, 50), FRotator(0, 180, 0));
	if (StreetStart == nullptr)
	{
		UE_LOG(LogSimGameMode, Warning, TEXT("PlayerStart spawn failed — the pawn will start at the default."));
	}
	// --- end the street ------------------------------------------------------

	// The pawn spawned during Login, before any PlayerStart stood — restart
	// it so FindPlayerStart places it at ours, facing the gate.
	Super::StartPlay();
	// No street start (its spawn failed, logged above): keep the engine's pawn.
	APlayerController* PC = World->GetFirstPlayerController();
	if (PC != nullptr && StreetStart != nullptr)
	{
		// Direct placement (RestartPlayer's own spawn was not landing at the
		// street start): spawn ours at the PlayerStart FIRST, possess it (which
		// unpossesses the fallback), and only then destroy the fallback — a
		// failed spawn keeps the player in the engine's pawn.
		APawn* Old = PC->GetPawn();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APawn* NewPawn = World->SpawnActor<APawn>(DefaultPawnClass, StreetStart->GetActorTransform(), Params))
		{
			PC->Possess(NewPawn);
			if (Old != nullptr && Old != NewPawn)
			{
				Old->Destroy();
			}
			UE_LOG(LogSimGameMode, Log, TEXT("Pawn '%s' placed at (%.0f,%.0f,%.0f), facing the gate."),
				*NewPawn->GetName(),
				NewPawn->GetActorLocation().X, NewPawn->GetActorLocation().Y, NewPawn->GetActorLocation().Z);
		}
		else
		{
			UE_LOG(LogSimGameMode, Warning, TEXT("Pawn spawn at the street start failed — keeping the engine's pawn."));
		}
	}
}

AActor* ASimGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	// The street has exactly one start spot, and the pawn starts there —
	// facing through the moon gate.
	if (StreetStart != nullptr)
	{
		return StreetStart;
	}
	return Super::FindPlayerStart_Implementation(Player, IncomingName);
}

void ASimGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The clock on screen: sim day, season and hour, from the kernel via the
	// C API. The world subsystem drives the clock; the sim itself is owned by
	// the game instance (it survives map travel). Reads resolve through THIS
	// world (the ...For getters), so each PIE instance shows its own clock.
	if (UWorld* World = GetWorld())
	{
		const int64 Day = USimWorldSubsystem::GetSimDayFor(World);
		if (Day < 0)
		{
			return;  // the sim world does not exist yet (or its canon is missing)
		}
		const FString Season = USimWorldSubsystem::GetSimSeasonFor(World);
		const float Hour = USimWorldSubsystem::GetSimHourFor(World);
		const int32 WholeHour = FMath::Clamp(FMath::FloorToInt(Hour), 0, 23);
		const int32 Minute = FMath::Clamp(FMath::FloorToInt((Hour - WholeHour) * 60.f), 0, 59);
		// Same key (1) every tick: the message is replaced, not stacked, and
		// stays on screen as long as the game mode ticks (audit U17).
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(1, 1.f, FColor::White,
				FString::Printf(TEXT("Day %lld — %s — %02d:%02d"), Day, *Season, WholeHour, Minute));
		}
		if (Day != LastShownDay)
		{
			LastShownDay = Day;
			UE_LOG(LogSimGameMode, Log, TEXT("Clock: day %lld — %s."), Day, *Season);
		}
	}
}
