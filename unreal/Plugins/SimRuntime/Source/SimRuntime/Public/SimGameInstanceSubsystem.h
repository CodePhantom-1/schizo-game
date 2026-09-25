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

private:
	/**
	 * Creates the world from the staged canon. Fails loudly (error log, no
	 * retries) when the canon folder is missing: the kernel skips missing
	 * tables, so without this check a wrong path gives an empty world.
	 */
	bool CreateWorld();

	/** The opaque kernel world (sim/CApi.h). Owned; destroyed in Deinitialize. */
	struct SimWorld* SimHandle = nullptr;
	bool bCanonFailed = false;  // one-shot: canon missing — stop retrying creation
	double SecondsSinceLastDay = 0.0;
};
