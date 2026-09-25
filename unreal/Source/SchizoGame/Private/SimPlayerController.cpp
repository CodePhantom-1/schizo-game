// SimPlayerController.cpp — the Use verb (touch, read, knock, pray: the verb
// set starts here — game-design §8, "you can touch almost everything").
#include "SimPlayerController.h"

#include "SchizoGame.h"
#include "SimTablet.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// Debug: drive the Use verb headless (sim.ForceUsePending 1) — the input
	// map is keyboard-first, but the smoke has no hands on keys.
	TAutoConsoleVariable<int32> CVarForceUsePending(
		TEXT("sim.ForceUsePending"),
		0,
		TEXT("Set to 1 to fire the Use verb on the next controller tick (debug)."));
}

void ASimPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent != nullptr)
	{
		// Legacy bindings from Config/DefaultInput.ini — the slice's pawn moves
		// with the engine's own bindings; Use is ours.
		InputComponent->BindAction("Use", IE_Pressed, this, &ASimPlayerController::OnUse);
	}
}

void ASimPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogSchizoGame, Log, TEXT("Sim controller live on pawn '%s'."),
		GetPawn() ? *GetPawn()->GetName() : TEXT("<no pawn>"));
}

void ASimPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (CVarForceUsePending.GetValueOnGameThread() != 0)
	{
		// Wait for the pawn: the exec fires before the pawn spawns, and the
		// one-shot must not be wasted on an empty controller.
		if (GetPawn() == nullptr)
		{
			return;
		}
		CVarForceUsePending->Set(0, ECVF_SetByConsole);
		OnUse();
	}
}

void ASimPlayerController::OnUse()
{
	UWorld* World = GetWorld();
	APawn* Pawn = GetPawn();
	if (World == nullptr || Pawn == nullptr)
	{
		return;
	}

	FVector Eyes;
	FRotator EyesRot;
	if (UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>())
	{
		Eyes = Camera->GetComponentLocation();
		EyesRot = Camera->GetComponentRotation();
	}
	else
	{
		Eyes = Pawn->GetActorLocation() + FVector(0, 0, 40);
		EyesRot = Pawn->GetActorRotation();
	}

	const FVector End = Eyes + EyesRot.Vector() * 300.f;
	UE_LOG(LogSchizoGame, Log, TEXT("Use fired from '%s': eye (%.0f,%.0f,%.0f), facing %.0f."),
		*GetName(), Eyes.X, Eyes.Y, Eyes.Z, EyesRot.Yaw);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SimUse), false);
	if (World->LineTraceSingleByChannel(Hit, Eyes, End, ECC_Visibility, Params))
	{
		UE_LOG(LogSchizoGame, Log, TEXT("Use hit '%s' at (%.0f,%.0f,%.0f)."),
			*Hit.GetActor()->GetName(), Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
		if (ASimTablet* Tablet = Cast<ASimTablet>(Hit.GetActor()))
		{
			Tablet->Interact();
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(2, 3.f, FColor::Silver,
				*Hit.GetActor()->GetActorLabel());
		}
	}
	else
	{
		UE_LOG(LogSchizoGame, Log, TEXT("Use hit nothing within 300 units."));
	}
}
