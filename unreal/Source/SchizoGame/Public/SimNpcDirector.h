// SimNpcDirector.h — UE-4: spawns one ASimNpc per City-of-the-Moon resident
// with a schedule role, and every sim hour asks the kernel for that
// resident's task and derives where it should be standing. The task->place
// mapping is INVENTED glue, documented in
// docs/proposals/invented-ledger-npc-movement.md.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimNpcDirector.generated.h"

class ASimNpc;

UCLASS()
class SCHIZOGAME_API ASimNpcDirector : public AActor
{
	GENERATED_BODY()

public:
	ASimNpcDirector();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	/** Reads every kernel npc, keeps the City-of-the-Moon residents with a
	    role, and spawns an ASimNpc for each. No-ops if the kernel isn't up
	    yet (Tick retries until it is). */
	void SpawnResidentsIfReady();

	/** This hour's destination + task text for one npc (the invented glue). */
	FVector ResolveDestination(const ASimNpc& Npc, int32 Hour, FString& OutScheduleId, FString& OutTaskText) const;

	/** A place (or, failing that, any stable string) -> a world location.
	    First choice: a `Place:<id>` tagged actor (the street agent's future
	    registry). Fallback: a deterministic hash position along the street
	    line, so the same id always lands in the same spot. */
	FVector ResolvePlaceLocation(const FString& PlaceOrFallbackKey) const;

	UPROPERTY()
	TArray<TObjectPtr<ASimNpc>> Residents;

	int32 LastAppliedHour = -1;
};
