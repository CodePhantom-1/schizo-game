// SimGameMode.cpp — the grey-box street, built in code (PLACEHOLDER: content
// kits and authored maps replace this at the slice's content pass).
#include "SimGameMode.h"
#include "SimWorldSubsystem.h"

// The kernel's C API through SimRuntime's public include path.
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
	Box->SetActorLabel(FString::Printf(TEXT("GreyBox_%s"), *Color.ToString()));
}

void ASimGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// The street: a floor, the moon-gate walls, a lintel. Grey-box stone.
	SpawnBox(FVector(0, 0, -50), FVector(40, 12, 1), FLinearColor::Gray);      // the street floor
	SpawnBox(FVector(0, -600, 150), FVector(2, 1, 12), FLinearColor::White);   // gate wall, west
	SpawnBox(FVector(0, 600, 150), FVector(2, 1, 12), FLinearColor::White);    // gate wall, east
	SpawnBox(FVector(0, 0, 500), FVector(4, 14, 1), FLinearColor::White);      // the lintel over the gate
	SpawnBox(FVector(2500, 0, -50), FVector(40, 12, 1), FLinearColor::Gray);   // the street continues

	// A PlayerStart so the pawn has somewhere to be (the far side of the gate).
	if (World->SpawnActor<APlayerStart>(FVector(1500, 0, 100), FRotator::ZeroRotator) == nullptr)
	{
		UE_LOG(LogSimGameMode, Warning, TEXT("PlayerStart spawn failed — the pawn will start at the default."));
	}

	UE_LOG(LogSimGameMode, Log, TEXT("The grey-box street stands."));
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
