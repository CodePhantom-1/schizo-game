// SimWell.cpp — drinking is always available from a well (Needs.hpp: "water"
// is the virtual always-drinkable id, no inventory needed).
#include "SimWell.h"

#include "SchizoGame.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

#include "Components/StaticMeshComponent.h"
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
	const int Result = sim_world_drink(Handle, "player", "water");
	UE_LOG(LogSchizoGame, Log, TEXT("Drank water at the well (result %d); thirst now %d."),
		Result, sim_world_thirst(Handle, "player"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(3, 3.f, FColor::White, TEXT("You drink from the well."));
	}
}
