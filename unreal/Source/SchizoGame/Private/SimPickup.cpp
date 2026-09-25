// SimPickup.cpp — gives ItemId x Quantity to the player, then removes itself.
#include "SimPickup.h"

#include "SchizoGame.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ASimPickup::ASimPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	RootComponent = PickupMesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		PickupMesh->SetStaticMesh(Cube.Object);
	}
	PickupMesh->SetWorldScale3D(FVector(0.25f));
}

FString ASimPickup::GetVerbLabel() const
{
	return FString::Printf(TEXT("Pick up %s"), *ItemId);
}

void ASimPickup::Interact(APawn* /*Instigator*/)
{
	SimWorld* Handle = USimWorldSubsystem::GetSimHandle();
	if (Handle == nullptr)
	{
		UE_LOG(LogSchizoGame, Warning, TEXT("Pickup used before the sim world exists."));
		return;
	}
	const int NewCount = sim_world_give_item(Handle, "player", TCHAR_TO_UTF8(*ItemId), Quantity);
	UE_LOG(LogSchizoGame, Log, TEXT("Picked up %d x %s (now holding %d)."), Quantity, *ItemId, NewCount);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(3, 3.f, FColor::White,
			FString::Printf(TEXT("Picked up %s."), *ItemId));
	}
	// Hides it: destroy rather than merely hide — nothing else in the slice
	// needs the actor to persist once its item is in the player's inventory.
	Destroy();
}
