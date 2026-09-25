// SimGameInstanceSubsystem.h — the OWNER of the kernel world (audit U10, D-020).
// The sim lives as long as the game instance, so it survives map travel
// (OpenLevel, seamless travel, entering a house map). USimWorldSubsystem stays
// the per-map driver and the public facade: it creates the world lazily on the
// first begun-play tick, advances the clock, and serves every getter. Game
// code keeps calling USimWorldSubsystem; this class is plumbing.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SimGameInstanceSubsystem.generated.h"

UCLASS()
class SIMRUNTIME_API USimGameInstanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	// The per-map driver creates the world and advances its clock.
	friend class USimWorldSubsystem;

public:
	virtual void Deinitialize() override;

	/** The live kernel world, or nullptr before creation. Never cache it. */
	struct SimWorld* GetHandle() const { return SimHandle; }

	/** Engine seconds elapsed since the last sim-day tick (the sub-day clock). */
	double GetSecondsSinceLastDay() const { return SecondsSinceLastDay; }

	/** Absolute path of the staged canon (Content/Sim/canon). */
	static FString CanonDir();

	// --- the world's lifecycle (A10/A12) -------------------------------------
	/** Replaces the world with a fresh one from the canon. False (logged) keeps the old world. */
	bool NewWorld(uint64 Seed);
	/** The kernel snapshot as text (sim_world_save_to_buffer). False before the world exists. */
	bool SaveToString(FString& Out) const;
	/** Replaces the world with a saved one. False (bad data, logged) keeps the current world. */
	bool LoadFromString(const FString& Data);
	/** Restores the sub-day clock (engine seconds since the last day tick) after a load. */
	void SetSecondsSinceLastDay(double Seconds);
	/** The owner of the world that WorldContext lives in, or nullptr. */
	static USimGameInstanceSubsystem* Get(const UObject* WorldContext);

private:
	/**
	 * Creates the world from the staged canon. Fails loudly (error log, no
	 * retries) when the canon folder is missing: the kernel skips missing
	 * tables, so without this check a wrong path gives an empty world.
	 */
	bool CreateWorld();
	/** The staged canon exists (a sentinel table); logs and latches bCanonFailed when not. */
	bool CanonReady();

	/** The opaque kernel world (sim/CApi.h). Owned; destroyed in Deinitialize. */
	struct SimWorld* SimHandle = nullptr;
	bool bCanonFailed = false;  // one-shot: canon missing — stop retrying creation
	double SecondsSinceLastDay = 0.0;
};
