// SimBed.cpp — sleeping is one kernel verb: sim_world_rest advances the needs
// as asleep and rests the body's wounds for SleepHours game-hours. The engine's
// sub-day clock is NOT skipped (it is owned by USimWorldSubsystem, which this
// wave may not edit): the street's hour stands still while you sleep, a known
// slice gap for the coordinator to close with a SimRuntime SkipSimHours.
#include "SimBed.h"

#include "SchizoGame.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"

ASimBed::ASimBed()
{
	PrimaryActorTick.bCanEverTick = false;

	MatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MatMesh"));
	RootComponent = MatMesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		MatMesh->SetStaticMesh(Cube.Object);
	}
	MatMesh->SetWorldScale3D(FVector(2.0f, 0.9f, 0.1f));
}

void ASimBed::Interact(APawn* /*Instigator*/)
{
	SimWorld* Handle = USimWorldSubsystem::GetSimHandle();
	if (Handle == nullptr)
	{
		UE_LOG(LogSchizoGame, Warning, TEXT("Bed used before the sim world exists."));
		return;
	}
	const int Died = sim_world_rest(Handle, "player", SleepHours);
	UE_LOG(LogSchizoGame, Log, TEXT("Slept %d hours (rest returned %d); fatigue now %d."),
		SleepHours, Died, sim_world_fatigue(Handle, "player"));
	if (GEngine)
	{
		// rest returns 1 only if the sleeper bled out in the night.
		const FString Line = Died == 1
			? FString(TEXT("You sleep. You do not wake."))
			: FString::Printf(TEXT("You sleep %d hours."), SleepHours);
		GEngine->AddOnScreenDebugMessage(3, 3.f, FColor::White, Line);
	}
}
