// SimDoor.cpp — rotates the leaf and toggles blocking collision on Use.
#include "SimDoor.h"

#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ASimDoor::ASimDoor()
{
	PrimaryActorTick.bCanEverTick = false;

	LeafMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeafMesh"));
	RootComponent = LeafMesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		LeafMesh->SetStaticMesh(Cube.Object);
	}
	LeafMesh->SetWorldScale3D(FVector(0.1f, 1.0f, 2.0f));
	LeafMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	LeafMesh->SetCollisionResponseToAllChannels(ECR_Block);
	LeafMesh->SetMobility(EComponentMobility::Movable);
}

FString ASimDoor::GetVerbLabel() const
{
	return bOpen ? TEXT("Close") : TEXT("Open");
}

void ASimDoor::Interact(APawn* /*Instigator*/)
{
	bOpen = !bOpen;

	const FRotator ClosedRotation = GetActorRotation();
	const FRotator TargetRotation = bOpen
		? FRotator(ClosedRotation.Pitch, ClosedRotation.Yaw + OpenYawDegrees, ClosedRotation.Roll)
		: FRotator(ClosedRotation.Pitch, ClosedRotation.Yaw - OpenYawDegrees, ClosedRotation.Roll);
	SetActorRotation(TargetRotation);

	// Open doors let you walk through; closed doors block (no swept collision
	// animation for the slice — the leaf snaps, plan.md's grey-box scope).
	LeafMesh->SetCollisionEnabled(bOpen ? ECollisionEnabled::QueryOnly : ECollisionEnabled::QueryAndPhysics);
}
