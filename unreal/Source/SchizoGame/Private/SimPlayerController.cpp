// SimPlayerController.cpp — the verb set starts here (game-design §8, "you
// can touch almost everything"): Use from the eyes, eat and drink from the
// satchel, the body's needs riding the street's clock. Everything the player
// does that the kernel knows goes through sim/CApi.h.
#include "SimPlayerController.h"
#include "SimSaves.h"

#include "SchizoGame.h"
#include "SimHud.h"
#include "SimInteractable.h"
#include "SimTablet.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
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

void ASimPlayerController::OnQuickSave()
{
	SimSaves::Save(this, SimSaves::QuickSlot, TEXT("Quicksave"));
}

void ASimPlayerController::OnQuickLoad()
{
	SimSaves::Load(this, SimSaves::QuickSlot);
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
		InputComponent->BindAction("QuickSave", IE_Pressed, this, &ASimPlayerController::OnQuickSave);
		InputComponent->BindAction("QuickLoad", IE_Pressed, this, &ASimPlayerController::OnQuickLoad);
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

	EnsureSimHud();

	// The diegetic prompt: what is under the crosshair right now.
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

	// Needs over time: every whole game hour the street's clock passes, the
	// kernel advances hunger/thirst/fatigue for the player at its documented
	// rates (Needs.cpp: +2/+3/+4 per hour awake). The engine owns the clock,
	// so this follows it — hour by hour, never by frame fraction.
	const int64 Day = USimWorldSubsystem::GetSimDayFor(this);
	const float Hour = USimWorldSubsystem::GetSimHourFor(this);
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
			if (SimWorld* Handle = USimWorldSubsystem::GetSimHandleFor(this))
			{
				sim_world_advance_needs(Handle, "player", WholeHours, /*sleeping=*/0);
			}
			LastAbsoluteHour += WholeHours;
		}
	}
}

void ASimPlayerController::EnsureSimHud()
{
	// Every tick, not once: the engine's own HUD spawn may land after our
	// BeginPlay in some flows, and it draws nothing — swap it for ours again.
	if (MyHUD != nullptr && MyHUD->IsA<ASimHud>())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	if (MyHUD != nullptr)
	{
		MyHUD->Destroy();
		MyHUD = nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MyHUD = World->SpawnActor<ASimHud>(ASimHud::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (MyHUD != nullptr)
	{
		UE_LOG(LogSchizoGame, Log, TEXT("Verb HUD up ('%s')."), *MyHUD->GetName());
	}
}

bool ASimPlayerController::TraceLook(FHitResult& OutHit) const
{
	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (World == nullptr || ControlledPawn == nullptr)
	{
		return false;
	}

	FVector Eyes;
	FRotator EyesRot;
	if (UCameraComponent* Camera = ControlledPawn->FindComponentByClass<UCameraComponent>())
	{
		Eyes = Camera->GetComponentLocation();
		EyesRot = Camera->GetComponentRotation();
	}
	else
	{
		Eyes = ControlledPawn->GetActorLocation() + FVector(0, 0, 40);
		EyesRot = ControlledPawn->GetActorRotation();
	}

	const FVector End = Eyes + EyesRot.Vector() * 500.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SimUse), false);
	// The protagonist has a body now: the shoulder camera's sightline starts
	// behind it, so the trace must step over its own capsule and meshes.
	Params.AddIgnoredActor(ControlledPawn);
	return World->LineTraceSingleByChannel(OutHit, Eyes, End, ECC_Visibility, Params);
}

void ASimPlayerController::OnUse()
{
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn == nullptr)
	{
		return;
	}

	UE_LOG(LogSchizoGame, Log, TEXT("Use fired from '%s'."), *GetName());
	FHitResult Hit;
	if (TraceLook(Hit) && Hit.GetActor() != nullptr)
	{
		UE_LOG(LogSchizoGame, Log, TEXT("Use hit '%s' at (%.0f,%.0f,%.0f)."),
			*Hit.GetActor()->GetName(), Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
		if (ISimInteractable* Interactable = Cast<ISimInteractable>(Hit.GetActor()))
		{
			Interactable->Interact(ControlledPawn);
		}
		else if (ASimTablet* Tablet = Cast<ASimTablet>(Hit.GetActor()))
		{
			Tablet->Interact();
		}
		else
		{
			// GetName, not GetActorLabel: labels are editor-only (Game target).
			if (GEngine != nullptr)
			{
				GEngine->AddOnScreenDebugMessage(2, 3.f, FColor::Silver,
					*Hit.GetActor()->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogSchizoGame, Log, TEXT("Use hit nothing within 500 units."));
	}
}

void ASimPlayerController::OnEat()
{
	SimWorld* Handle = USimWorldSubsystem::GetSimHandleFor(this);
	if (Handle == nullptr)
	{
		return;
	}
	// The kernel ranks what is carried (Needs' category table) — no shortlist
	// on this side, and nothing to eat when the satchel holds no food.
	char Best[64] = {};
	if (sim_world_best_food(Handle, "player", Best, sizeof(Best)) <= 0 || Best[0] == '\0')
	{
		UE_LOG(LogSchizoGame, Log, TEXT("Nothing to eat."));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(4, 3.f, FColor::Silver, TEXT("Nothing to eat."));
		}
		return;
	}
	const FString Item(UTF8_TO_TCHAR(Best));
	const int Result = sim_world_eat(Handle, "player", Best);
	UE_LOG(LogSchizoGame, Log, TEXT("Ate %s (result %d); hunger now %d."),
		*Item, Result, sim_world_hunger(Handle, "player"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(4, 3.f, FColor::White,
			Result == 0 ? *FString::Printf(TEXT("You eat %s."), *Item) : TEXT("That is not food."));
	}
}

void ASimPlayerController::OnQuaff()
{
	SimWorld* Handle = USimWorldSubsystem::GetSimHandleFor(this);
	if (Handle == nullptr)
	{
		return;
	}
	// As OnEat, for thirst. "water" is never answered here — it is the well's
	// virtual id, drawn at the well, not held.
	char Best[64] = {};
	if (sim_world_best_drink(Handle, "player", Best, sizeof(Best)) <= 0 || Best[0] == '\0')
	{
		UE_LOG(LogSchizoGame, Log, TEXT("Nothing to drink."));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(4, 3.f, FColor::Silver, TEXT("Nothing to drink."));
		}
		return;
	}
	const FString Item(UTF8_TO_TCHAR(Best));
	const int Result = sim_world_drink(Handle, "player", Best);
	UE_LOG(LogSchizoGame, Log, TEXT("Drank %s (result %d); thirst now %d."),
		*Item, Result, sim_world_thirst(Handle, "player"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(4, 3.f, FColor::White,
			Result == 0 ? *FString::Printf(TEXT("You drink %s."), *Item) : TEXT("That is not drinkable."));
	}
}

void ASimPlayerController::OnInventoryPressed()
{
	bInventoryHeld = true;
}

void ASimPlayerController::OnInventoryReleased()
{
	bInventoryHeld = false;
}
