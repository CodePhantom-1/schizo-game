// SimGameMode.cpp — the grey-box street, built in code (PLACEHOLDER: content
// kits and authored maps replace this at the slice's content pass).
#include "SimGameMode.h"
#include "SimWorldSubsystem.h"

// The kernel's C API through SimRuntime's public include path.
#include "SimPlayerController.h"
#include "SimTablet.h"
#include "sim/CApi.h"

#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
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

void ASimGameMode::SpawnBox(const FVector& Location, const FVector& Scale, const FLinearColor& Color)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	AStaticMeshActor* Box = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
	if (Box == nullptr)
	{
		return;
	}
	Box->SetMobility(EComponentMobility::Movable);
	if (UStaticMeshComponent* Mesh = Box->GetStaticMeshComponent())
	{
		// Runtime load (ConstructorHelpers only works inside constructors).
		if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			Mesh->SetStaticMesh(Cube);
		}
		Mesh->SetWorldScale3D(Scale);
		Mesh->SetMobility(EComponentMobility::Movable);
	}
#if WITH_EDITOR
	// Actor labels are editor-only: the Game target has no SetActorLabel.
	Box->SetActorLabel(FString::Printf(TEXT("GreyBox_%s"), *Color.ToString()));
#endif
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
	// The street: a floor, the moon-gate walls, a lintel. Grey-box stone.
	SpawnBox(FVector(0, 0, -50), FVector(40, 12, 1), FLinearColor::Gray);      // the street floor
	SpawnBox(FVector(0, -600, 150), FVector(2, 1, 12), FLinearColor::White);   // gate wall, west
	SpawnBox(FVector(0, 600, 150), FVector(2, 1, 12), FLinearColor::White);    // gate wall, east
	SpawnBox(FVector(0, 0, 500), FVector(4, 14, 1), FLinearColor::White);      // the lintel over the gate
	SpawnBox(FVector(2500, 0, -50), FVector(40, 12, 1), FLinearColor::Gray);   // the street continues

	// The first readable thing: a clay tablet by the street's start (notes L198).
	if (World->SpawnActor<ASimTablet>(FVector(1300, 0, 140), FRotator(0, 90, 0)) == nullptr)
	{
		UE_LOG(LogSimGameMode, Warning, TEXT("tablet spawn failed"));
	}

	// A PlayerStart so the pawn has somewhere to be: facing the gate — and the
	// tablet, which sits ahead of the spawn.
	StreetStart = World->SpawnActor<APlayerStart>(FVector(1500, 0, 100), FRotator(0, 180, 0));
	if (StreetStart == nullptr)
	{
		UE_LOG(LogSimGameMode, Warning, TEXT("PlayerStart spawn failed — the pawn will start at the default."));
	}

	UE_LOG(LogSimGameMode, Log, TEXT("The grey-box street stands (v2)."));

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
