// SimWorldSubsystem.h — the sim's engine facade and per-map driver.
// The kernel world (one sim::WorldState behind the kernel's C API) is OWNED by
// USimGameInstanceSubsystem, so it survives map travel (audit U10, D-020).
// This world subsystem creates it lazily when the first game world begins
// play, ticks it from the engine clock, and serves every getter. It is the
// only engine object allowed to advance the world — everything else reads.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimWorldSubsystem.generated.h"

/**
 * The simulation's engine home. Binding rules (plan v2 §6, D-010):
 *  - the world is created from the shipped canon (Content/Sim/canon) and one seed;
 *    a missing canon folder is an error (logged), never a silent empty world;
 *  - one sim day advances every SimDaysPerRealMinute REAL MINUTES of engine
 *    time (the name is historical: the value is minutes per day, not days per
 *    minute — renaming it would drop existing ini overrides);
 *  - play begins at StartHour on day 1 (default 06:00, the morning of arrival);
 *  - readers (player, NPCs, UI) call the getters; NOTHING writes module state
 *    except the kernel's own tick — the kernel's rule is the engine's rule.
 *
 * The static getters resolve "the" play world through
 * GEngine->GetCurrentPlayWorld(), which is right for a single-player run. With
 * several PIE instances (listen server + clients) use the ...For variants,
 * which read the sim of the caller's own world.
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
	/** Current sim day (1-based; day 1 = the morning the prisoner arrives). -1 before the world exists. */
	UFUNCTION(BlueprintPure, Category = "Sim")
	static int64 GetSimDay();

	/** Season id for today: rains, sowing, harvest, vintage (D-015). Empty before the world exists. */
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
	 * REAL MINUTES of engine time per sim day (parent architecture §4.2
	 * default: 45 — one sim day every 45 real minutes). The name reads
	 * backwards; it is kept so existing ini overrides still apply. Overridable
	 * at runtime by the console variable sim.DaysPerRealMinute (same unit,
	 * debug pacing; 0 = use this property). Changing the pace mid-day rescales
	 * the current hour at once (GetSimHour is elapsed / seconds-per-day).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Sim")
	float SimDaysPerRealMinute = 45.0f;

	/** Hour of day 1 at which play begins (the prisoner lands in the morning). 0..<24. */
	UPROPERTY(Config, EditAnywhere, Category = "Sim")
	float StartHour = 6.0f;

	/**
	 * The live kernel world of the current play world, for game code that
	 * calls the C API (sim/CApi.h) directly — needs, inventory, crafting,
	 * schedules, save/load. nullptr before the world exists. Never destroy it,
	 * and never cache it: fetch it at each use. It dies when the game instance
	 * shuts down (PIE stop, quit) and will be replaced by save/load. The
	 * handle is mutable: C API writers (give_item, craft, rites...) act on the
	 * live world — only game-rule code should call them.
	 */
	static struct SimWorld* GetSimHandle();

	/**
	 * Hour of the current sim day, 0.0 (midnight) .. <24.0, from the engine
	 * time elapsed since the last day tick. The engine owns the sub-day clock;
	 * NPC schedules, needs and the sun read this. Play begins at StartHour.
	 * -1 before the world exists.
	 */
	UFUNCTION(BlueprintPure, Category = "Sim")
	static float GetSimHour();

	/** Debug/act-scripting: advance the world N days immediately (the hour is unchanged). */
	UFUNCTION(BlueprintCallable, Category = "Sim")
	static void AdvanceSimDays(int32 Days);

	/**
	 * Sleep/time-skip: move the sub-day clock forward by game-hours (unlike
	 * AdvanceSimDays the hour moves, through the same day-wrap the tick uses).
	 * Kernel needs are NOT advanced here — the caller owns that (sim_world_rest
	 * already charged them asleep); use GetSimHourFor after to read the new time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sim")
	static void SkipSimHours(float Hours);

	// --- world-context variants (multi-PIE safe; audit U9) --------------------
	// Same contracts as the functions above, but they read the sim of the
	// world that WorldContextObject lives in instead of the current play world.

	UFUNCTION(BlueprintPure, Category = "Sim", meta = (WorldContext = "WorldContextObject"))
	static int64 GetSimDayFor(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Sim", meta = (WorldContext = "WorldContextObject"))
	static FString GetSimSeasonFor(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Sim", meta = (WorldContext = "WorldContextObject"))
	static int64 GetSimPriceFor(const UObject* WorldContextObject, const FString& CityId, const FString& ItemId);

	UFUNCTION(BlueprintCallable, Category = "Sim", meta = (WorldContext = "WorldContextObject"))
	static void SetSimDroughtFor(const UObject* WorldContextObject, int32 Stage);

	UFUNCTION(BlueprintPure, Category = "Sim", meta = (WorldContext = "WorldContextObject"))
	static int32 GetSimDroughtFor(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Sim", meta = (WorldContext = "WorldContextObject"))
	static float GetSimHourFor(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Sim", meta = (WorldContext = "WorldContextObject"))
	static void AdvanceSimDaysFor(const UObject* WorldContextObject, int32 Days);

	/** SkipSimHours() for the caller's own world. */
	UFUNCTION(BlueprintCallable, Category = "Sim", meta = (WorldContext = "WorldContextObject"))
	static void SkipSimHoursFor(const UObject* WorldContextObject, float Hours);

	/** GetSimHandle() for the caller's own world. Same rules: never destroy, never cache. */
	static struct SimWorld* GetSimHandleFor(const UObject* WorldContextObject);

	// --- the calendar (A14, D-024 §7) ---------------------------------------
	/** Today's date. False (outputs untouched) before the world exists. */
	UFUNCTION(BlueprintPure, Category = "Sim|Calendar", meta = (WorldContext = "WorldContextObject"))
	static bool GetSimDateFor(const UObject* WorldContextObject, int32& Year, int32& Month, int32& DayOfMonth);

	/** Month name (months.csv); empty before the world exists or when months are unnamed. */
	UFUNCTION(BlueprintPure, Category = "Sim|Calendar", meta = (WorldContext = "WorldContextObject"))
	static FString GetSimMonthNameFor(const UObject* WorldContextObject);

	/** "new" | "waxing" | "full" | "waning"; empty before the world exists. */
	UFUNCTION(BlueprintPure, Category = "Sim|Calendar", meta = (WorldContext = "WorldContextObject"))
	static FString GetSimMoonPhaseFor(const UObject* WorldContextObject);

	/** 0 (new) .. 100 (full); -1 before the world exists. */
	UFUNCTION(BlueprintPure, Category = "Sim|Calendar", meta = (WorldContext = "WorldContextObject"))
	static int32 GetSimMoonIlluminationFor(const UObject* WorldContextObject);

	/** "favourable" | "unfavourable" | "" (calendar_days.csv). */
	UFUNCTION(BlueprintPure, Category = "Sim|Calendar", meta = (WorldContext = "WorldContextObject"))
	static FString GetSimDayOmenFor(const UObject* WorldContextObject);

	/** Today's named day of the month (e.g. the eššešu of Nanna), or empty. */
	UFUNCTION(BlueprintPure, Category = "Sim|Calendar", meta = (WorldContext = "WorldContextObject"))
	static FString GetSimDayObservanceFor(const UObject* WorldContextObject);

private:
	float SecondsPerDay() const;
};
