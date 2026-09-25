// SimWorldSubsystem.h — PLACEHOLDER (authored pre-editor; first compile at Phase 3 bring-up).
// Owns THE world: one sim::WorldState behind the kernel's C API, created when
// the game world begins, ticked from the engine clock. This is the only engine
// object allowed to advance the world — everything else reads.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimWorldSubsystem.generated.h"

/**
 * The simulation's engine home. Binding rules (plan v2 §6, D-010):
 *  - the world is created from the shipped canon (Content/Sim/canon) and one seed;
 *  - one sim day advances per SimDaysPerRealMinute of engine time;
 *  - readers (player, NPCs, UI) call the getters; NOTHING writes module state
 *    except the kernel's own tick — the kernel's rule is the engine's rule.
 */
UCLASS(Config = Engine)
class SIMRUNTIME_API USimWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- lifecycle ---------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// --- readers (UI, NPC, player) ------------------------------------------
	/** Current sim day (1-based; day 1 = the morning the prisoner arrives). */
	UFUNCTION(BlueprintPure, Category = "Sim")
	static int64 GetSimDay();

	/** Season id for today: rains, sowing, harvest, vintage (D-015). */
	UFUNCTION(BlueprintPure, Category = "Sim")
	static FString GetSimSeason();

	/** Silver-grain price of an item in a city's market (canon ids). */
	UFUNCTION(BlueprintPure, Category = "Sim")
	static int64 GetSimPrice(const FString& CityId, const FString& ItemId);

	// --- act-scripting surfaces (the clock's masters) ------------------------
	/** The drought that is breaking the world: 0 = none, rises through the acts. */
	UFUNCTION(BlueprintCallable, Category = "Sim")
	static void SetSimDrought(int32 Stage);

	UFUNCTION(BlueprintPure, Category = "Sim")
	static int32 GetSimDrought();

	/**
	 * Real minutes of engine time per sim day (default: 24). Overridable at
	 * runtime by the console variable sim.DaysPerRealMinute (debug pacing;
	 * 0 = use this property).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Sim")
	float SimDaysPerRealMinute = 24.0f;

	/**
	 * Sim hour (0..24) a NEW world starts at, so a fresh game begins in the
	 * morning rather than at midnight. Ignored on a loaded save — the save's
	 * own hour (sidecar, see GetSimHour) wins. Overridable by console variable
	 * sim.StartHour (negative = use this property).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Sim")
	float SimStartHour = 7.0f;

	/**
	 * The live kernel world of the current play world, for game code that
	 * calls the C API (sim/CApi.h) directly — needs, inventory, crafting,
	 * schedules, save/load. nullptr before the world exists. Never destroy it.
	 */
	static struct SimWorld* GetSimHandle();

	/**
	 * Hour of the current sim day, 0.0 (midnight) .. <24.0, from the engine
	 * time elapsed since the last day tick. The engine owns the sub-day clock;
	 * NPC schedules, needs and the sun read this. -1 before the world exists.
	 */
	UFUNCTION(BlueprintPure, Category = "Sim")
	static float GetSimHour();

	/** Debug/act-scripting: advance the world N days immediately. */
	UFUNCTION(BlueprintCallable, Category = "Sim")
	static void AdvanceSimDays(int32 Days);

	// --- needs (hunger/thirst/fatigue, 0..100; -1 = no world yet) -----------
	UFUNCTION(BlueprintPure, Category = "Sim|Needs")
	static int32 GetSimHunger(const FString& Actor);
	UFUNCTION(BlueprintPure, Category = "Sim|Needs")
	static int32 GetSimThirst(const FString& Actor);
	UFUNCTION(BlueprintPure, Category = "Sim|Needs")
	static int32 GetSimFatigue(const FString& Actor);
	/** Semicolon-joined named effects at the actor's current thresholds (e.g. "hungry;parched"). */
	UFUNCTION(BlueprintPure, Category = "Sim|Needs")
	static FString GetSimNeedEffects(const FString& Actor);
	/** Advances one actor's needs by Hours game-hours; the day tick itself stays daily. */
	UFUNCTION(BlueprintCallable, Category = "Sim|Needs")
	static void AdvanceSimNeeds(const FString& Actor, int32 Hours, bool bSleeping);
	/** sim_world_eat return codes: 0 ok, -1 null args, -2 not held, -3 not edible. */
	UFUNCTION(BlueprintCallable, Category = "Sim|Needs")
	static int32 EatSimItem(const FString& Actor, const FString& Item);
	/** sim_world_drink return codes: 0 ok, -1 null args, -2 not held, -3 not drinkable ("water" always available). */
	UFUNCTION(BlueprintCallable, Category = "Sim|Needs")
	static int32 DrinkSimItem(const FString& Actor, const FString& Item);

	// --- inventory ------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "Sim|Inventory")
	static int32 GetSimItemCount(const FString& Actor, const FString& Item);
	/** Adds Qty (may be negative, clamped at 0) to the actor's count; returns the resulting count. */
	UFUNCTION(BlueprintCallable, Category = "Sim|Inventory")
	static int32 GiveSimItem(const FString& Actor, const FString& Item, int32 Qty);

	// --- crafting ---------------------------------------------------------------
	/** sim_world_craft return codes: 0 ok, -1 bad args, -2 unknown recipe, -3 missing station, -4 missing inputs. */
	UFUNCTION(BlueprintCallable, Category = "Sim")
	static int32 CraftSim(const FString& Actor, const FString& Recipe, const FString& StationsSemicolonList, int32 Times);

	// --- schedule -----------------------------------------------------------------
	/** The task in force for Role at Hour (0..23) today; empty if no schedule rows. */
	UFUNCTION(BlueprintPure, Category = "Sim")
	static FString GetSimTaskAt(const FString& Role, int32 Hour);

	// --- population -----------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "Sim")
	static int32 GetSimNpcCount();

	// --- save / load ------------------------------------------------------------
	/** Saves to Saved/SaveGames/<Slot>.simsave (kernel snapshot + the sub-day hour sidecar). */
	UFUNCTION(BlueprintCallable, Category = "Sim")
	static bool SaveSimGame(const FString& Slot);
	/** Loads Saved/SaveGames/<Slot>.simsave, replacing the current world. False on any failure (world unchanged). */
	UFUNCTION(BlueprintCallable, Category = "Sim")
	static bool LoadSimGame(const FString& Slot);

private:
	/** The opaque kernel world (sim/CApi.h). Owned; never null after Initialize. */
	struct SimWorld* SimHandle = nullptr;
	bool bCanonFailed = false;  // one-shot: canon missing — stop retrying creation
	double SecondsSinceLastDay = 0.0;
	float SecondsPerDay() const;
	static FString SaveSlotPath(const FString& Slot);
};
