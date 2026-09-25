// SimBed.cpp — sleeping applies the kernel's asleep-needs curve and skips the
// engine's sub-day clock forward (USimWorldSubsystem::SkipSimHours — small
// hook added alongside this file; see report).
#include "SimBed.h"

#include "SchizoGame.h"
#include "SimPlayerController.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
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

void ASimBed::Interact(APawn* Instigator)
{
	SimWorld* Handle = USimWorldSubsystem::GetSimHandle();
	if (Handle == nullptr)
	{
		UE_LOG(LogSchizoGame, Warning, TEXT("Bed used before the sim world exists."));
		return;
	}
	sim_world_advance_needs(Handle, "player", SleepHours, /*sleeping=*/1);
	USimWorldSubsystem::SkipSimHours(static_cast<float>(SleepHours));
	// The controller's own passive hourly tick must not re-apply the hours
	// we just slept through (they're already accounted for above).
	if (Instigator != nullptr)
	{
		if (ASimPlayerController* PC = Cast<ASimPlayerController>(Instigator->GetController()))
		{
			PC->ResyncNeedsClock();
		}
	}
	UE_LOG(LogSchizoGame, Log, TEXT("Slept %d hours; fatigue now %d."),
		SleepHours, sim_world_fatigue(Handle, "player"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(3, 3.f, FColor::White,
			FString::Printf(TEXT("You sleep for %d hours."), SleepHours));
	}
}
