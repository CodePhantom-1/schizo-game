// SimVerbSpawner.h — one-shot placement for the verb actors once the level has
// begun play: a door at every actor or component tagged "SimDoorSlot" (the
// street kit's doorway slots), and — until the authored street replaces the
// grey-box — a PLACEHOLDER well, bed and pickups by the street's start, so
// the verbs are playable the moment this wave lands. A world subsystem so it
// needs no wiring into SimGameMode (not this wave's file to edit).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimVerbSpawner.generated.h"

UCLASS()
class SCHIZOGAME_API USimVerbSpawner : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** New Game (A12): the PLACEHOLDER goods back on the street (taken ones return). */
	void ResetDemoPickups();

private:
	/** The three PLACEHOLDER satchel goods (grain, bread, beer). */
	void SpawnDemoPickups();

	/** Doors at every actor/component tagged "SimDoorSlot" (the street kit's contract). */
	void SpawnDoorsAtSlots();

	/** PLACEHOLDER grey-box props (well, bed, pickups) when the street has none. */
	void SpawnDemoProps();

	bool bSpawned = false;
};
