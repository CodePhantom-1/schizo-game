// SimGameMode.cpp — wires the slice together: the street (W6-B), the
// residents and the sun (W6-C), the verbs and HUD (W6-A, via the player
// controller), and the sim clock on screen.
#include "SimGameMode.h"
#include "UI/SimShellSubsystem.h"
#include "SimWorldSubsystem.h"

// The kernel's C API through SimRuntime's public include path.
#include "SimDayNight.h"  // W6-C: the sun and sky on the sim clock
#include "SimNpcDirector.h"  // W6-C: the residents, spawned from kernel schedules
#include "SimCharacter.h"  // the protagonist's body — replaces the spectator pawn
#include "SimPlayerController.h"
#include "SimStreetBuilder.h"  // W6-B: the Moon Gate Quarter from the kit
#include "SimTablet.h"
#include "sim/CApi.h"

#include "SimEnvironment.h"  // the city around the quarter, built by code (D-023)

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
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
	// The slice's player controller arrives with its verbs and HUD; the pawn
	// is the protagonist's own body (a walking character, not a spectator).
	PlayerControllerClass = ASimPlayerController::StaticClass();
	DefaultPawnClass = ASimCharacter::StaticClass();
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

	// Development screens (the HUD draws the clock): off unless asked for.
	if (GEngine != nullptr && !FParse::Param(FCommandLine::Get(), TEXT("SimDebugMessages")))
	{
		GEngine->bEnableOnScreenDebugMessages = false;
	}

	// --- the city around the quarter (terrain, walls, ziggurat, lighthouse,
	// fields, sea) — first, because its terrain is the street's ground.
	Environment = ASimEnvironment::BuildWorld(World);

	// --- the street (W6-B) ---------------------------------------------------
	// Built BEFORE Super::StartPlay(): the pawn must find our PlayerStart,
	// not the engine's fallback. The builder owns the kit meshes; the
	// environment owns the ground and SimDayNight the sky.
	const FSimStreetBuildResult Street = ASimStreetBuilder::BuildQuarter(World);
	if (Environment != nullptr)
	{
		Environment->BuildStreetDressing();
	}
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
	// (GateLocation already sits at ground z=100 for the pivot-centred cube;
	// +40 more floats it at reading height without burying the bottom half.)
	if (World->SpawnActor<ASimTablet>(Street.GateLocation + FVector(300, 0, 40), FRotator(0, 90, 0)) == nullptr)
	{
		UE_LOG(LogSimGameMode, Warning, TEXT("tablet spawn failed"));
	}

	// A PlayerStart so the pawn has somewhere to be: facing through the gate.
	StreetStart = World->SpawnActor<APlayerStart>(Street.GateLocation + FVector(1100, 0, 50), FRotator(0, 180, 0));
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

namespace
{
	struct FSimShotView
	{
		const TCHAR* Name;
		FVector Loc;
		FVector Target;  // ZeroVector + bPlayer = the player's own camera
		bool bPlayer;
	};
	const FSimShotView GShotViews[] = {
		{ TEXT("player"),   FVector::ZeroVector,                 FVector::ZeroVector,              true },
		{ TEXT("street"),   FVector(2600.f, -260.f, 175.f),      FVector(12000.f, 1200.f, 500.f),  false },
		{ TEXT("gate"),     FVector(-4200.f, -1600.f, 260.f),    FVector(0.f, 0.f, 900.f),         false },
		{ TEXT("fields"),   FVector(-1800.f, 1200.f, 1100.f),    FVector(-30000.f, -4000.f, 0.f),  false },
		{ TEXT("zig"),      FVector(16200.f, -2600.f, 260.f),    FVector(22000.f, 3000.f, 1500.f), false },
		{ TEXT("overview"), FVector(-9000.f, -14000.f, 6500.f),  FVector(16000.f, 5000.f, 0.f),    false },
		{ TEXT("harbor"),   FVector(33000.f, -9000.f, 2400.f),   FVector(48500.f, 6000.f, 2500.f), false },
	};
}

void ASimGameMode::TickShots(float DeltaSeconds)
{
	// Development capture: -SimShots=<dir> [-SimShotHours=7,12,17.5,21]
	// [-SimShotViews=player,street,...] walks every hour x view, saves a PNG
	// per pair and quits. The clock is slowed so each frame holds its hour.
	UWorld* World = GetWorld();
	if (World == nullptr || ShotStage < 0)
	{
		return;
	}
	if (ShotStage == 0)
	{
		if (!FParse::Value(FCommandLine::Get(), TEXT("SimShots="), ShotDir))
		{
			ShotStage = -1;
			return;
		}
		FString HoursArg = TEXT("7,12,17.5,21");
		FParse::Value(FCommandLine::Get(), TEXT("SimShotHours="), HoursArg, false);
		TArray<FString> Parts;
		HoursArg.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& P : Parts)
		{
			ShotHours.Add(FCString::Atof(*P));
		}
		FString ViewsArg;
		FParse::Value(FCommandLine::Get(), TEXT("SimShotViews="), ViewsArg, false);
		for (int32 i = 0; i < UE_ARRAY_COUNT(GShotViews); ++i)
		{
			if (ViewsArg.IsEmpty() || ViewsArg.Contains(GShotViews[i].Name))
			{
				ShotViews.Add(i);
			}
		}
		FString ScreensArg;
		if (FParse::Value(FCommandLine::Get(), TEXT("SimShotScreens="), ScreensArg, false))
		{
			ScreensArg.ParseIntoArray(ShotScreens, TEXT(","), true);
			SetTickableWhenPaused(true);  // the screens pause the world; the tour must go on
		}
		ShotStage = 1;
		ShotTimer = 0.f;
		return;
	}
	if (USimWorldSubsystem::GetSimHourFor(World) < 0.f)
	{
		return;  // no kernel world yet
	}
	ShotTimer += DeltaSeconds;
	APlayerController* PC = World->GetFirstPlayerController();
	if (PC == nullptr)
	{
		return;
	}
	if (ShotStage == 1)
	{
		// Let the world settle (shader compiles, the director's first spawn).
		if (ShotTimer < 6.f)
		{
			return;
		}
		if (IConsoleVariable* Rate = IConsoleManager::Get().FindConsoleVariable(TEXT("sim.DaysPerRealMinute")))
		{
			Rate->Set(100000.f, ECVF_SetByCode);
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ShotCamera = World->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (ShotCamera != nullptr && ShotCamera->GetCameraComponent() != nullptr)
		{
			ShotCamera->GetCameraComponent()->SetFieldOfView(80.f);
			ShotCamera->GetCameraComponent()->bConstrainAspectRatio = false;
		}
		ShotIndex = 0;
		ShotStage = 2;
		ShotTimer = 0.f;
		bShotPrepared = false;
		return;
	}
	const int32 ViewShots = ShotHours.Num() * ShotViews.Num();
	const int32 Total = ViewShots + ShotScreens.Num();
	if (ShotIndex >= ViewShots && ShotIndex < Total)
	{
		USimShellSubsystem* Shell = USimShellSubsystem::Get(this);
		const FString& Name = ShotScreens[ShotIndex - ViewShots];
		if (!bShotPrepared)
		{
			PC->SetViewTarget(PC->GetPawn());
			if (Shell != nullptr)
			{
				Shell->CloseAll();
				for (ESimScreen S : {ESimScreen::MainMenu, ESimScreen::Pause, ESimScreen::SaveLoad, ESimScreen::Settings,
					ESimScreen::Journal, ESimScreen::Tablet, ESimScreen::Loading})
				{
					if (Name == USimShellSubsystem::ScreenName(S))
					{
						if (S == ESimScreen::Tablet)
						{
							Shell->ShowTablet(NSLOCTEXT("SimShots", "TabletTitle", "A tablet"), NSLOCTEXT("SimShots", "TabletBody", "Wedges in clay: a sample text for the reader."));
						}
						else
						{
							Shell->Open(S);
						}
					}
				}
			}
			bShotPrepared = true;
			ShotTimer = 0.f;
			return;
		}
		if (ShotTimer >= 1.5f)
		{
			const FString File = FPaths::Combine(ShotDir, FString::Printf(TEXT("screen_%s.png"), *Name));
			FScreenshotRequest::RequestScreenshot(File, true, false);
			UE_LOG(LogSimGameMode, Log, TEXT("SimShots: %s"), *File);
			++ShotIndex;
			bShotPrepared = false;
			ShotTimer = -0.5f;
			if (ShotIndex == Total && Shell != nullptr)
			{
				Shell->CloseAll();
			}
		}
		return;
	}
	if (ShotIndex >= Total)
	{
		if (ShotTimer > 1.5f)
		{
			UE_LOG(LogSimGameMode, Log, TEXT("SimShots: %d captures written to %s — quitting."), Total, *ShotDir);
			FGenericPlatformMisc::RequestExit(false);
			ShotStage = -1;
		}
		return;
	}
	const float Hour = ShotHours[ShotIndex / ShotViews.Num()];
	const FSimShotView& View = GShotViews[ShotViews[ShotIndex % ShotViews.Num()]];
	if (!bShotPrepared)
	{
		if (ShotIndex % ShotViews.Num() == 0)
		{
			const float Now = USimWorldSubsystem::GetSimHourFor(World);
			const float Delta = FMath::Fmod(Hour - Now + 48.f, 24.f);
			USimWorldSubsystem::SkipSimHoursFor(World, Delta);
		}
		if (View.bPlayer || ShotCamera == nullptr)
		{
			PC->SetViewTarget(PC->GetPawn());
		}
		else
		{
			ShotCamera->SetActorLocationAndRotation(View.Loc, (View.Target - View.Loc).Rotation());
			PC->SetViewTarget(ShotCamera);
		}
		bShotPrepared = true;
		ShotTimer = 0.f;
		return;
	}
	const float Settle = (ShotIndex % ShotViews.Num() == 0) ? 5.f : 3.f;
	if (ShotTimer >= Settle)
	{
		const FString File = FPaths::Combine(ShotDir, FString::Printf(TEXT("%05.2f_%s.png"), Hour, View.Name));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogSimGameMode, Log, TEXT("SimShots: %s"), *File);
		++ShotIndex;
		bShotPrepared = false;
		ShotTimer = -0.5f;
	}
}

void ASimGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickShots(DeltaSeconds);

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
