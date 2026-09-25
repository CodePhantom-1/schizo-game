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
UCLASS()
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
	 * Real minutes of engine time per sim day (parent architecture §4.2 default:
	 * 45). Settable so the acts' pacing and the vertical slice can differ.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Sim")
	float SimDaysPerRealMinute = 45.0f;

private:
	/** The opaque kernel world (sim/CApi.h). Owned; never null after Initialize. */
	struct SimWorld* SimHandle = nullptr;
	double SecondsSinceLastDay = 0.0;
};
