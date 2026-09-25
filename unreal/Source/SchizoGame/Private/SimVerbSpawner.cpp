// SimVerbSpawner.cpp — the verb actors' entry into the level. Runs once, the
// first tick after begin-play: the street's doorway slots (if the kit has
// placed any) grow doors, and the grey-box street gains a well, a bed and
// satchel goods so every verb has something to act on today.
#include "SimVerbSpawner.h"

#include "SchizoGame.h"
#include "SimBed.h"
#include "SimDoor.h"
#include "SimPickup.h"
#include "SimStreetBuilder.h"
#include "SimWell.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	// The slot anchor is an APPROACH point: 1 m outside the wall, on the
	// ground (SimStreetBuilder's contract). The door leaf belongs ON the wall
	// face — and the engine cube is pivot-centred, so +100 in z lifts the
	// 2 m leaf off the ground instead of half-burying it.
	constexpr float kApproachOffset = 80.f;  // GRID 100 - WALL_THICK/2 20
	constexpr float kLeafHalfHeight = 100.f;
	const bool bGateOpensAlongX = true;  // the gate's opening spans X (street runs Y through it)

	int32 Count = 0;
	TArray<FSimDoorSlotInfo> Slots;
	ASimStreetBuilder::GetAllDoorSlots(Slots);
	if (Slots.Num() > 0)
	{
		for (const FSimDoorSlotInfo& Slot : Slots)
		{
			const FRotator SlotRot(0.f, Slot.OutwardYawDeg, 0.f);
			const FVector Outward = SlotRot.Vector();
			FVector Location = Slot.Location - Outward * kApproachOffset;
			Location.Z += kLeafHalfHeight;
			FRotator LeafRot = SlotRot;
			float LeafWidthScale = 1.f;
			if (Slot.PlaceId == TEXT("moon_gate_place"))
			{
				// The gate straddles the street axis: the opening spans X, so
				// the leaf swings across it — quarter-turn the yaw and double
				// the width to fill the 2 m gap.
				LeafRot.Yaw += bGateOpensAlongX ? 90.f : -90.f;
				LeafWidthScale = 2.f;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ASimDoor* Door = World->SpawnActor<ASimDoor>(Location, LeafRot, Params);
			if (Door != nullptr)
			{
				if (LeafWidthScale != 1.f)
				{
					Door->SetActorScale3D(FVector(0.1f, LeafWidthScale, 2.f));
				}
				++Count;
			}
		}
	}
	else
	{
		// No registry (pre-street world): fall back to the tagged anchors,
		// same wall-face math from the anchor's own transform.
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
			const FRotator SlotRot = SpawnTransform.Rotator();
			FVector Location = SpawnTransform.GetLocation() - SlotRot.Vector() * kApproachOffset;
			Location.Z += kLeafHalfHeight;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (World->SpawnActor<ASimDoor>(Location, SlotRot, Params) != nullptr)
			{
				++Count;
			}
		}
	}
	if (Count > 0)
	{
		UE_LOG(LogSchizoGame, Log, TEXT("Verb spawner: %d door(s) hung on their doorways."), Count);
	}
}

namespace
{
	// The demo props are engine cubes/cylinders; without this they wear the
	// engine's checkerboard grid. Flat mud-and-clay colours until the kit
	// gives them real meshes.
	void ApplyPropColor(AActor* Prop, const FColor& Color)
	{
		if (Prop == nullptr)
		{
			return;
		}
		if (UStaticMeshComponent* Mesh = Prop->FindComponentByClass<UStaticMeshComponent>())
		{
			if (UMaterial* Base = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial")))
			{
				if (UMaterialInstanceDynamic* Mic = Mesh->CreateAndSetMaterialInstanceDynamic(0))
				{
					Mic->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(Color));
				}
			}
		}
	}
}  // namespace

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
		// Prefer the street's own well place (where the water-carriers walk);
		// the gate-side spot is only the pre-street fallback.
		FVector WellLocation(700.f, -350.f, 40.f);
		FVector StreetWell;
		if (ASimStreetBuilder::GetPlaceLocation(TEXT("street_well_place"), StreetWell))
		{
			WellLocation = StreetWell + FVector(0.f, 0.f, 40.f);
		}
		if (ASimWell* Well = World->SpawnActor<ASimWell>(WellLocation, FRotator::ZeroRotator, Params))
		{
			ApplyPropColor(Well, FColor(150, 143, 128));  // worn limestone rim
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
			ApplyPropColor(Bed, FColor(126, 100, 62));  // a reed mat, roughly
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
			const FColor PickupColor = FCString::Strcmp(Demo.ItemId, TEXT("grain")) == 0
				? FColor(196, 168, 90)   // straw-gold grain
				: FCString::Strcmp(Demo.ItemId, TEXT("bread")) == 0
					? FColor(176, 128, 72)  // baked crust
					: FColor(112, 76, 32);  // beer in a dark jar
			ApplyPropColor(Pickup, PickupColor);
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
