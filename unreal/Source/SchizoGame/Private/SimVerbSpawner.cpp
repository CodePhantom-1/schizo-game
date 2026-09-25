// SimVerbSpawner.cpp — the verb actors' entry into the level. Runs once, the
// first tick after begin-play: the street's doorway slots (if the kit has
// placed any) grow doors, and the grey-box street gains a well, a bed and
// satchel goods so every verb has something to act on today.
#include "SimVerbSpawner.h"

#include "SchizoGame.h"
#include "SimBed.h"
#include "SimDoor.h"
#include "SimPickup.h"
#include "SimWell.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	const FName SimDoorSlotTag(TEXT("SimDoorSlot"));

	// The PLACEHOLDER grey-box props (0 = off when the authored street lands).
	TAutoConsoleVariable<int32> CVarVerbDemoProps(
		TEXT("sim.VerbDemoProps"),
		1,
		TEXT("Spawn the grey-box well/bed/pickups on the street (debug placeholder until the authored map)."));
}

void USimVerbSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bSpawned)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (World == nullptr || !World->HasBegunPlay())
	{
		return;
	}
	bSpawned = true;

	SpawnDoorsAtSlots();
	SpawnDemoProps();
}

void USimVerbSpawner::SpawnDoorsAtSlots()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Slot = *It;
		if (Slot == nullptr || Slot->IsA<ASimDoor>())
		{
			continue;
		}

		bool bIsSlot = Slot->ActorHasTag(SimDoorSlotTag);
		FTransform SpawnTransform = Slot->GetActorTransform();
		if (!bIsSlot)
		{
			// Also check components — a doorway slot may be a component of a
			// larger street actor ("actor or component tagged SimDoorSlot").
			TInlineComponentArray<USceneComponent*> Components;
			Slot->GetComponents(Components);
			for (USceneComponent* Comp : Components)
			{
				if (Comp && Comp->ComponentHasTag(SimDoorSlotTag))
				{
					bIsSlot = true;
					SpawnTransform = Comp->GetComponentTransform();
					break;
				}
			}
		}
		if (!bIsSlot)
		{
			continue;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (World->SpawnActor<ASimDoor>(SpawnTransform.GetLocation(), SpawnTransform.Rotator(), Params) != nullptr)
		{
			++Count;
		}
	}
	if (Count > 0)
	{
		UE_LOG(LogSchizoGame, Log, TEXT("Verb spawner: %d door(s) at SimDoorSlot tags."), Count);
	}
}

void USimVerbSpawner::SpawnDemoProps()
{
	UWorld* World = GetWorld();
	if (World == nullptr || CVarVerbDemoProps.GetValueOnGameThread() == 0)
	{
		return;
	}

	// Each prop only when the street does not already have one: idempotent on
	// this pass, and quiet the day the authored map (or a merged street kit)
	// places its own.
	const TActorIterator<ASimWell> ExistingWell(World);
	if (!ExistingWell)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// The grey-box street's floor tops out at z=0 (SimGameMode's boxes);
		// the engine cylinder is 100 tall scaled 0.8, so its centre sits at 40.
		if (ASimWell* Well = World->SpawnActor<ASimWell>(FVector(700, -350, 40), FRotator::ZeroRotator, Params))
		{
#if WITH_EDITOR
			Well->SetActorLabel(TEXT("GreyBox_Well"));
#endif
			UE_LOG(LogSchizoGame, Log, TEXT("Verb spawner: the well stands (PLACEHOLDER grey-box)."));
		}
	}

	const TActorIterator<ASimBed> ExistingBed(World);
	if (!ExistingBed)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// The mat is a 100-cube scaled (2, 0.9, 0.1): 10 tall, centre at 5.
		if (ASimBed* Bed = World->SpawnActor<ASimBed>(FVector(1800, 350, 5), FRotator::ZeroRotator, Params))
		{
#if WITH_EDITOR
			Bed->SetActorLabel(TEXT("GreyBox_Bed"));
#endif
			UE_LOG(LogSchizoGame, Log, TEXT("Verb spawner: the bed lies (PLACEHOLDER grey-box)."));
		}
	}

	// Satchel goods between the spawn and the gate: grain for the quern-to-come,
	// bread and beer for the Eat/Quaff keys today.
	struct FDemoPickup
	{
		const TCHAR* ItemId;
		int32 Quantity;
		FVector Location;
	};
	const FDemoPickup DemoPickups[] = {
		{ TEXT("grain"), 2, FVector(1200, 200, 13) },
		{ TEXT("bread"), 1, FVector(1000, -150, 13) },
		{ TEXT("beer"), 1, FVector(1600, 150, 13) },
	};
	for (const FDemoPickup& Demo : DemoPickups)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ASimPickup* Pickup = World->SpawnActor<ASimPickup>(Demo.Location, FRotator::ZeroRotator, Params))
		{
			Pickup->ItemId = Demo.ItemId;
			Pickup->Quantity = Demo.Quantity;
#if WITH_EDITOR
			Pickup->SetActorLabel(FString::Printf(TEXT("GreyBox_Pickup_%s"), Demo.ItemId));
#endif
		}
	}
	UE_LOG(LogSchizoGame, Log, TEXT("Verb spawner: the grey-box goods lie on the street (PLACEHOLDER; sim.VerbDemoProps 0 hides them next run)."));
}

TStatId USimVerbSpawner::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USimVerbSpawner, STATGROUP_Tickables);
}

bool USimVerbSpawner::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
