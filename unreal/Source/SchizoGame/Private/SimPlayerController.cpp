// SimPlayerController.cpp — the Use verb (touch, read, knock, pray: the verb
// set starts here — game-design §8, "you can touch almost everything").
#include "SimPlayerController.h"

#include "SchizoGame.h"
#include "SimInteractable.h"
#include "SimTablet.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

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

	// Priority-ordered known canon ids (db/canon/items.csv, foods.csv) — the
	// C API has no "list my inventory" call, only item_count(id), so "best
	// food/drink" picks the first of these the actor is carrying.
	// ponytail: a fixed shortlist, not a real inventory query; widen this list
	// (or add a kernel enumerate-inventory call) if content grows past it.
	const TCHAR* kFoodPriority[] = { TEXT("bread"), TEXT("grain"), TEXT("wheat"), TEXT("fish") };
	const TCHAR* kDrinkPriority[] = { TEXT("beer") };
}

void ASimPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent != nullptr)
	{
		// Legacy bindings from Config/DefaultInput.ini — the slice's pawn moves
		// with the engine's own bindings; Use is ours.
		InputComponent->BindAction("Use", IE_Pressed, this, &ASimPlayerController::OnUse);
		InputComponent->BindAction("Eat", IE_Pressed, this, &ASimPlayerController::OnEat);
		InputComponent->BindAction("Quaff", IE_Pressed, this, &ASimPlayerController::OnQuaff);
		InputComponent->BindAction("Inventory", IE_Pressed, this, &ASimPlayerController::OnInventoryPressed);
		InputComponent->BindAction("Inventory", IE_Released, this, &ASimPlayerController::OnInventoryReleased);
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

	// The diegetic prompt: what's under the crosshair right now.
	FHitResult LookHit;
	if (TraceLook(LookHit) && LookHit.GetActor() != nullptr)
	{
		if (ISimInteractable* Interactable = Cast<ISimInteractable>(LookHit.GetActor()))
		{
			CurrentVerbLabel = Interactable->GetVerbLabel();
		}
		else if (LookHit.GetActor()->IsA<ASimTablet>())
		{
			CurrentVerbLabel = TEXT("Read");
		}
		else
		{
			CurrentVerbLabel.Empty();
		}
	}
	else
	{
		CurrentVerbLabel.Empty();
	}

	// Needs over time: every whole game hour that passes, advance the
	// kernel's hunger/thirst/fatigue for the player (plan.md §6 item 4).
	const int64 Day = USimWorldSubsystem::GetSimDay();
	const float Hour = USimWorldSubsystem::GetSimHour();
	if (Day >= 0 && Hour >= 0.f)
	{
		const double AbsoluteHour = static_cast<double>(Day) * 24.0 + Hour;
		if (LastAbsoluteHour < 0.0)
		{
			LastAbsoluteHour = AbsoluteHour;
		}
		else if (const double DeltaHours = AbsoluteHour - LastAbsoluteHour; DeltaHours >= 1.0)
		{
			const int WholeHours = static_cast<int>(DeltaHours);
			if (SimWorld* Handle = USimWorldSubsystem::GetSimHandle())
			{
				sim_world_advance_needs(Handle, "player", WholeHours, /*sleeping=*/0);
			}
			LastAbsoluteHour += WholeHours;
		}
	}
}

void ASimPlayerController::ResyncNeedsClock()
{
	const int64 Day = USimWorldSubsystem::GetSimDay();
	const float Hour = USimWorldSubsystem::GetSimHour();
	if (Day >= 0 && Hour >= 0.f)
	{
		LastAbsoluteHour = static_cast<double>(Day) * 24.0 + Hour;
	}
}

bool ASimPlayerController::TraceLook(FHitResult& OutHit) const
{
	UWorld* World = GetWorld();
	APawn* Pawn = GetPawn();
	if (World == nullptr || Pawn == nullptr)
	{
		return false;
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
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SimUse), false);
	return World->LineTraceSingleByChannel(OutHit, Eyes, End, ECC_Visibility, Params);
}

void ASimPlayerController::OnUse()
{
	APawn* Pawn = GetPawn();
	if (Pawn == nullptr)
	{
		return;
	}

	FHitResult Hit;
	UE_LOG(LogSchizoGame, Log, TEXT("Use fired from '%s'."), *GetName());
	if (TraceLook(Hit) && Hit.GetActor() != nullptr)
	{
		UE_LOG(LogSchizoGame, Log, TEXT("Use hit '%s' at (%.0f,%.0f,%.0f)."),
			*Hit.GetActor()->GetName(), Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
		if (ISimInteractable* Interactable = Cast<ISimInteractable>(Hit.GetActor()))
		{
			Interactable->Interact(Pawn);
		}
		else if (ASimTablet* Tablet = Cast<ASimTablet>(Hit.GetActor()))
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

void ASimPlayerController::OnEat()
{
	SimWorld* Handle = USimWorldSubsystem::GetSimHandle();
	if (Handle == nullptr)
	{
		return;
	}
	for (const TCHAR* Food : kFoodPriority)
	{
		const FTCHARToUTF8 Utf8Food(Food);
		if (sim_world_item_count(Handle, "player", Utf8Food.Get()) > 0)
		{
			const int Result = sim_world_eat(Handle, "player", Utf8Food.Get());
			UE_LOG(LogSchizoGame, Log, TEXT("Ate %s (result %d); hunger now %d."),
				Food, Result, sim_world_hunger(Handle, "player"));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(4, 3.f, FColor::White,
					FString::Printf(TEXT("You eat %s."), Food));
			}
			return;
		}
	}
	UE_LOG(LogSchizoGame, Log, TEXT("Nothing to eat."));
}

void ASimPlayerController::OnQuaff()
{
	SimWorld* Handle = USimWorldSubsystem::GetSimHandle();
	if (Handle == nullptr)
	{
		return;
	}
	for (const TCHAR* Drink : kDrinkPriority)
	{
		const FTCHARToUTF8 Utf8Drink(Drink);
		if (sim_world_item_count(Handle, "player", Utf8Drink.Get()) > 0)
		{
			const int Result = sim_world_drink(Handle, "player", Utf8Drink.Get());
			UE_LOG(LogSchizoGame, Log, TEXT("Drank %s (result %d); thirst now %d."),
				Drink, Result, sim_world_thirst(Handle, "player"));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(4, 3.f, FColor::White,
					FString::Printf(TEXT("You drink %s."), Drink));
			}
			return;
		}
	}
	UE_LOG(LogSchizoGame, Log, TEXT("Nothing to drink."));
}

void ASimPlayerController::OnInventoryPressed()
{
	bInventoryHeld = true;
}

void ASimPlayerController::OnInventoryReleased()
{
	bInventoryHeld = false;
}
