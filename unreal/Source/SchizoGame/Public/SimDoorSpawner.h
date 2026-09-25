// SimDoorSpawner.h — spawns an ASimDoor at every actor/component tagged
// "SimDoorSlot" (plan.md §6 item 2). A world subsystem so it needs no wiring
// into SimGameMode: it self-activates once the level's actors exist.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimDoorSpawner.generated.h"

UCLASS()
class SCHIZOGAME_API USimDoorSpawner : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool bSpawned = false;
};
