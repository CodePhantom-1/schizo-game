// SimWell.cpp — drinking at the well goes straight through the kernel's C API
// so the sim stays the single source of truth for thirst.
#include "SimWell.h"

#include "SchizoGame.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"

ASimWell::ASimWell()
{
	PrimaryActorTick.bCanEverTick = false;

	WellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WellMesh"));
	RootComponent = WellMesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		WellMesh->SetStaticMesh(Cylinder.Object);
	}
	WellMesh->SetWorldScale3D(FVector(1.0f, 1.0f, 0.8f));
}

void ASimWell::Interact(APawn* /*Instigator*/)
{
	SimWorld* Handle = USimWorldSubsystem::GetSimHandle();
	if (Handle == nullptr)
	{
		UE_LOG(LogSchizoGame, Warning, TEXT("Well used before the sim world exists."));
		return;
	}
	// "water": the always-drinkable virtual id — no inventory is touched.
	const int Result = sim_world_drink(Handle, "player", "water");
	if (Result == 0)
	{
		UE_LOG(LogSchizoGame, Log, TEXT("Drank water at the well; thirst now %d."),
			sim_world_thirst(Handle, "player"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(3, 3.f, FColor::White, TEXT("You drink from the well."));
		}
	}
	else
	{
		UE_LOG(LogSchizoGame, Warning, TEXT("Drinking at the well refused: %d."), Result);
	}
}
