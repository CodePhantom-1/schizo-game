// SimPickup.cpp — the goods move through the C API (sim_world_give_item), then
// the actor destroys itself: the sim's inventory is now the carrier.
#include "SimPickup.h"

#include "SchizoGame.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
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
		UE_LOG(LogSchizoGame, Warning, TEXT("Pickup '%s' used before the sim world exists."), *ItemId);
		return;
	}
	const int32 Qty = FMath::Max(Quantity, 1);
	// TCHAR_TO_UTF8 temporaries live until the end of the full expression.
	const int NewCount = sim_world_give_item(Handle, "player", TCHAR_TO_UTF8(*ItemId), Qty);
	UE_LOG(LogSchizoGame, Log, TEXT("Picked up %d x %s (now holding %d)."), Qty, *ItemId, NewCount);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(3, 3.f, FColor::White,
			FString::Printf(TEXT("Picked up %s."), *ItemId));
	}
	// Destroy, not hide: nothing else in the slice needs the actor once its
	// goods sit in the player's kernel inventory.
	Destroy();
}
