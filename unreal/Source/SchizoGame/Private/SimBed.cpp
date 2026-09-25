// SimBed.cpp — sleeping is one kernel verb: sim_world_rest advances the needs
// as asleep and rests the body's wounds for SleepHours game-hours, then the
// engine's sub-day clock skips with the sleeper (USimWorldSubsystem::
// SkipSimHoursFor); the controller is told the hours are pre-charged so they
// are not also taken as awake time.
#include "SimBed.h"

#include "SchizoGame.h"
#include "SimPlayerController.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInterface.h"
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
	// D-023 flat colour (tools/art/ue_make_style_materials.py), not the engine checker.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StyleMat(TEXT("/Game/Art/Style/MI_Bed.MI_Bed"));
	if (StyleMat.Succeeded())
	{
		MatMesh->SetMaterial(0, StyleMat.Object);
	}
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
	// The clock skips with the sleeper; midnights crossed advance the world
	// through the subsystem's own day-wrap. The controller must not re-charge
	// these hours at awake rates on top of the kernel's sleeping rates.
	USimWorldSubsystem::SkipSimHoursFor(this, static_cast<float>(SleepHours));
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (ASimPlayerController* SimPC = Cast<ASimPlayerController>(PC))
		{
			SimPC->NotifySimHoursPreCharged(static_cast<double>(SleepHours));
		}
	}
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
