// SimDoorSpawner.cpp — one-shot scan for the "SimDoorSlot" tag (actors placed
// by the street agent's kit) once the level has begun play.
#include "SimDoorSpawner.h"

#include "SchizoGame.h"
#include "SimDoor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	const FName SimDoorSlotTag(TEXT("SimDoorSlot"));
}

void USimDoorSpawner::Tick(float DeltaTime)
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
			// Also check components — "actor or component tagged SimDoorSlot".
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
	UE_LOG(LogSchizoGame, Log, TEXT("SimDoorSpawner: spawned %d door(s) at SimDoorSlot tags."), Count);
}

TStatId USimDoorSpawner::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USimDoorSpawner, STATGROUP_Tickables);
}

bool USimDoorSpawner::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
