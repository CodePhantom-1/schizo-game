// SimGameMode.cpp — builds the Moon Gate Quarter (ASimStreetBuilder, from
// the mudbrick kit + db/canon/places.csv) and places the pawn at the gate.
#include "SimGameMode.h"
#include "SimStreetBuilder.h"
#include "SimWorldSubsystem.h"

// The kernel's C API through SimRuntime's public include path.
#include "SimPlayerController.h"
#include "SimTablet.h"
#include "sim/CApi.h"

#include "Engine/World.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Components/SkyLightComponent.h"
#include "Engine/SkyLight.h"
#include "GameFramework/PlayerStart.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimGameMode, Log, All);

ASimGameMode::ASimGameMode()
{
	// The slice's player controller arrives with its own work; the engine's
	// default pawn is enough for the grey-box smoke.
	PlayerControllerClass = ASimPlayerController::StaticClass();
	// The clock on screen needs the game mode to tick (actors don't by default).
	PrimaryActorTick.bCanEverTick = true;
}

void ASimGameMode::StartPlay()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// The street is built BEFORE Super::StartPlay(): the pawn spawns there,
	// and it must find the PlayerStart, not the engine's fallback.
	ASimStreetBuilder* Street = World->SpawnActor<ASimStreetBuilder>(FVector::ZeroVector, FRotator::ZeroRotator);
	FVector GateLoc = FVector::ZeroVector;
	if (Street != nullptr)
	{
		GateLoc = Street->Build();
		UE_LOG(LogSimGameMode, Log, TEXT("Street built: %d buildings, %d door slots."), Street->NumBuildingsBuilt, Street->NumDoorSlots);
	}
	else
	{
		UE_LOG(LogSimGameMode, Warning, TEXT("ASimStreetBuilder spawn failed — no street."));
	}

	// The first readable thing: a clay tablet just inside the gate (notes L198).
	const FVector TabletLoc = GateLoc + FVector(300.f, 0.f, 140.f);
	if (World->SpawnActor<ASimTablet>(TabletLoc, FRotator(0, 90, 0)) == nullptr)
	{
		UE_LOG(LogSimGameMode, Warning, TEXT("tablet spawn failed"));
	}

	// Light and sky: the Entry map ships dark — without these the street is
	// a black screen. The sun sits low, like a drought sky. Tagged SimSun so
	// the NPC/time agent (and ASimStreetBuilder's own duplicate-sun guard)
	// can find it (docs/plan.md UE-3 track item 5).
	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 2000), FRotator(-35, 20, 0)))
	{
		Sun->Tags.Add(FName(TEXT("SimSun")));
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(8.f);
			Light->SetLightColor(FLinearColor(1.f, 0.85f, 0.65f));  // dry, dusty daylight
		}
	}
	if (ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector(0, 0, 1500), FRotator::ZeroRotator))
	{
		if (USkyLightComponent* Light = Cast<USkyLightComponent>(Sky->GetLightComponent()))
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(2.f);
			Light->RecaptureSky();
		}
	}

	// A PlayerStart so the pawn has somewhere to be: facing in from the gate,
	// the tablet just ahead of the spawn.
	StreetStart = World->SpawnActor<APlayerStart>(GateLoc + FVector(500.f, 0.f, 50.f), FRotator(0, 180, 0));
	if (StreetStart == nullptr)
	{
		UE_LOG(LogSimGameMode, Warning, TEXT("PlayerStart spawn failed — the pawn will start at the default."));
	}

	UE_LOG(LogSimGameMode, Log, TEXT("The Moon Gate Quarter stands."));

	// The pawn spawned during Login, before any PlayerStart stood — restart
	// it so FindPlayerStart places it at ours, facing the gate.
	Super::StartPlay();
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		// Direct placement (RestartPlayer's own spawn was not landing at the
		// street start): destroy the fallback pawn, spawn ours at the PlayerStart.
		if (APawn* Old = PC->GetPawn())
		{
			Old->Destroy();
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APawn* NewPawn = World->SpawnActor<APawn>(DefaultPawnClass, StreetStart->GetActorTransform(), Params))
		{
			PC->Possess(NewPawn);
			UE_LOG(LogSimGameMode, Log, TEXT("Pawn '%s' placed at (%.0f,%.0f,%.0f), facing the gate."),
				*NewPawn->GetName(),
				NewPawn->GetActorLocation().X, NewPawn->GetActorLocation().Y, NewPawn->GetActorLocation().Z);
		}
		else
		{
			UE_LOG(LogSimGameMode, Warning, TEXT("Pawn spawn at the street start failed."));
		}
	}
}

AActor* ASimGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	// The default OpenWorld map ships its own PlayerStarts; this street has
	// exactly one, and the pawn starts there — facing the gate.
	if (StreetStart != nullptr)
	{
		return StreetStart;
	}
	return Super::FindPlayerStart_Implementation(Player, IncomingName);
}

void ASimGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The clock on screen: sim day and season, from the kernel via the C API.
	// The sim is a WORLD subsystem: the clock belongs to the world, not the
	// game instance.
	if (UWorld* World = GetWorld())
	{
		if (USimWorldSubsystem* Sim = World->GetSubsystem<USimWorldSubsystem>())
		{
			(void)Sim;  // reads go through the static accessors (the C API)
			const int64 Day = USimWorldSubsystem::GetSimDay();
			if (Day != LastShownDay)
			{
				LastShownDay = Day;
				GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::White,
					FString::Printf(TEXT("Day %lld — %s"), Day, *USimWorldSubsystem::GetSimSeason()));
				UE_LOG(LogSimGameMode, Log, TEXT("Clock: day %lld — %s."), Day, *USimWorldSubsystem::GetSimSeason());
			}
		}
	}
}
