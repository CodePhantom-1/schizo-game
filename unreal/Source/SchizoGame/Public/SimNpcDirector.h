// SimNpcDirector.h — W6-C: the street lives. Spawns and despawns one ASimNpc
// per kernel npc that the kernel's own schedule puts ON THIS STREET, and
// points each actor at its current kernel place every sim hour. The kernel
// is the truth (npc_place_at is already per-person resolved: role rows +
// person rows + home/work through people.csv, bent by season and festival);
// this actor only mirrors it — never the reverse.
//
// Who is visible is decided HERE from three kernel facts per npc:
//   npc_schedule_at >= 0   the npc has a day plan at all
//   npc_fate == ""         not slain / captive / sold east
//   npc_place_at != ""     placed this hour (a places.csv id, "" = off street)
// plus the place resolver below (a place the street does not know is off the
// street — that is how npcs of other cities stay away without a city read).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimNpcDirector.generated.h"

class ASimNpc;

/**
 * The street's place registry seam (decoupling from W6-B's street builder,
 * whose API this worktree cannot see). Map a kernel place id (places.csv,
 * e.g. "moon_gate_place") to a world position on the built street.
 *
 * Contract: return true and fill OutLocation when the registry knows the
 * place; return false when it does not (the director then treats the npc as
 * off this street and despawns its actor). Called on the game thread, once
 * per npc per sim hour (and once per RefreshAll) — cheap and stateless is
 * enough. Implemented by W6-B's registry and registered with
 * ASimNpcDirector::SetPlaceResolver (the coordinator wires this after merge).
 */
class ISimPlaceResolver
{
public:
	virtual ~ISimPlaceResolver() = default;

	/** Place id -> street position. OutLocation is the walking target. */
	virtual bool ResolvePlaceLocation(const FString& PlaceId, FVector& OutLocation) const = 0;
};

/** Delegate form of the same seam, for glue that prefers binding over
    subclassing (return false = unknown place, same contract). */
DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(
	bool, FSimResolvePlace, const FString&, PlaceId, UPARAM(ref) FVector&, OutLocation);

UCLASS()
class SCHIZOGAME_API ASimNpcDirector : public AActor
{
	GENERATED_BODY()

public:
	ASimNpcDirector();

	virtual void Tick(float DeltaSeconds) override;

	/** This world's director (there is one; the game mode spawns it). Nullptr until BeginPlay or if none. */
	static ASimNpcDirector* GetInstance(const UObject* WorldContextObject);

	/** Wire the street's registry (W6-B): kernel place id -> street position. Nullptr clears. The raw pointer is not owned. */
	void SetPlaceResolver(ISimPlaceResolver* Resolver);

	/** Bindable alternative to the interface (same contract per call). */
	FSimResolvePlace& GetPlaceResolverDelegate() { return PlaceResolverDelegate; }

	/** The pre-street hash fallback (step 4 of ResolvePlaceLocation): on
	    until the real street is wired, off after — when it is off, a place
	    id nothing knows means OFF THE STREET (despawn), not a hash spot. */
	void SetHashPlaceFallbackEnabled(const bool bEnabled) { bHashPlaceFallback = bEnabled; }

	/** Force a full re-read next Tick: call after wiring a resolver or after the street spawns/changes its place actors. */
	UFUNCTION(BlueprintCallable, Category = "Sim")
	void RefreshAll();

	/** How many npc actors currently stand in the world. */
	UFUNCTION(BlueprintPure, Category = "Sim")
	int32 GetVisibleNpcCount() const { return NpcActorsById.Num(); }

protected:
	virtual void BeginPlay() override;

private:
	/** One full kernel pass: reads every npc's schedule/place/task/fate, spawns what is now on the street, walks the rest, despawns what left. No-ops until the kernel world exists. */
	void RefreshFromKernel();

	/**
	 * The seam, resolved in order (first hit wins):
	 *  1. the registered ISimPlaceResolver (SetPlaceResolver);
	 *  2. the bound FSimResolvePlace delegate;
	 *  3. an actor tagged Place:<place_id> (a street may simply tag its markers);
	 *  4. while bHashPlaceFallback is on (the pre-street default): a
	 *     deterministic hash of the id onto the grey-box street's extent, so
	 *     the slice is populated before the real street lands (same id, same
	 *     spot, every run).
	 * A place nothing resolves is OFF THIS STREET (false) once the fallback
	 * is off — that is how the wired street keeps strangers away.
	 */
	bool ResolvePlaceLocation(const FString& PlaceId, FVector& OutLocation) const;

	/** A small deterministic ring around a place point, so residents who share
	    one place (three watchmen at the gate post) stand apart, not stacked. */
	FVector ClusterOffsetFor(const FString& NpcId) const;

	/** The player pawn's location, or the street origin before it spawns. */
	FVector RelevanceCentre() const;

	/** Kernel npc id -> its actor (spawns are ours, despawns are ours). */
	UPROPERTY()
	TMap<FString, TObjectPtr<ASimNpc>> NpcActorsById;

	/** Step 4 of the seam: hash unknown ids onto the grey-box extent while no
	    street exists. The coordinator switches this OFF when wiring W6-B's
	    registry (then unknown = off street). */
	UPROPERTY(EditAnywhere, Category = "Sim")
	bool bHashPlaceFallback = true;

	ISimPlaceResolver* PlaceResolver = nullptr;  // not owned, not a UObject

	UPROPERTY()
	FSimResolvePlace PlaceResolverDelegate;

	/** Change key (day * 24 + hour) of the last applied refresh. */
	int64 LastAppliedKey = -1;
	/** Bumped by RefreshAll so Tick re-reads even at an unchanged hour. */
	int32 ForceRefreshCookie = 0;
	int32 LastCookie = -1;
};
