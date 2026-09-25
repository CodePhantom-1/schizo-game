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
	// The Use trace runs on Visibility: a closed door must block it.
	LeafMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	LeafMesh->SetMobility(EComponentMobility::Movable);
}

void ASimDoor::BeginPlay()
{
	Super::BeginPlay();
	// The authored rotation IS the closed state; capture it once so repeated
	// open/close cycles return exactly there.
	ClosedYaw = GetActorRotation().Yaw;
}

FString ASimDoor::GetVerbLabel() const
{
	return bOpen ? TEXT("Close") : TEXT("Open");
}

void ASimDoor::Interact(APawn* /*Instigator*/)
{
	bOpen = !bOpen;

	// Snap, no sweep: the grey-box leaf teleports between closed and open
	// (plan.md's grey-box scope; the content pass animates it).
	const float TargetYaw = bOpen ? ClosedYaw + OpenYawDegrees : ClosedYaw;
	SetActorRotation(FRotator(0.f, TargetYaw, 0.f));

	// An open door lets you walk through; a closed one blocks. QueryOnly keeps
	// the leaf hittable by the Use trace while open, without stopping bodies.
	LeafMesh->SetCollisionEnabled(bOpen ? ECollisionEnabled::QueryOnly : ECollisionEnabled::QueryAndPhysics);
}
