// SimPlayerController.h — the player's verbs and the body's clock (Phase 6
// Player track). Use (E) traces from the eyes and talks to whatever the
// street offers through ISimInteractable; Eat (F) and Quaff (G) consume the
// best of what is carried, on the kernel's own judgement; Tab holds up the
// carried goods. Every whole game-hour the street's clock passes, the
// player's needs advance through the C API — the sim stays the single source
// of truth for the body.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SimPlayerController.generated.h"

UCLASS()
class SCHIZOGAME_API ASimPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	/** HUD reads: the verb label of whatever is under the crosshair right now
	 * ("Open", "Drink", ""...), for the diegetic prompt. */
	UFUNCTION(BlueprintPure, Category = "Sim")
	FString GetLookVerbLabel() const { return CurrentVerbLabel; }

	/** HUD reads: Tab is held (show the carried-goods overlay). */
	UFUNCTION(BlueprintPure, Category = "Sim")
	bool IsInventoryShown() const { return bInventoryHeld; }

	/** The kernel already charged these hours (sleep charged them asleep):
	 * the clock jump must not charge them again as awake time. No-op before
	 * the first baseline read (the first read anchors to the post-skip hour
	 * instead of charging from the pre-sleep one). */
	void NotifySimHoursPreCharged(double Hours)
	{
		if (LastAbsoluteHour < 0.0)
		{
			return;
		}
		LastAbsoluteHour += Hours;
	}

	/** A load moved the clock: forget the last needs reading so nothing is charged for the jump. */
	void ResetNeedsClock() { LastAbsoluteHour = -1.0; }

protected:
	/** The Use verb: trace from the camera; interact with what the street offers. */
	void OnUse();

	/** F: eat the best food carried (the kernel ranks it — sim_world_best_food). */
	void OnEat();

	/** G: drink the best drink carried (beer; water is drawn at the well, never held). */
	void OnQuaff();

	/** Tab held/released: the carried-goods overlay. */
	void OnInventoryPressed();
	void OnInventoryReleased();

	/** F5 / F9 (A10): the quicksave slot. */
	void OnQuickSave();
	void OnQuickLoad();

	/** Esc / the gamepad menu button: the pause menu (A8). */
	void OnPausePressed();

	/** J: the journal (A8/P10). */
	void OnJournal();

	/** Shared eye trace used by Use and the per-frame look label. */
	bool TraceLook(struct FHitResult& OutHit) const;

	/** Owns the verb HUD (ASimHud): the game mode's default HUD draws nothing,
	 * and this wave may not edit SimGameMode to set HUDClass — so the controller
	 * lazily swaps its own in, engine-order-proof by re-checking every tick. */
	void EnsureSimHud();

	FString CurrentVerbLabel;
	bool bInventoryHeld = false;

	/** Absolute sim hour (day*24 + hour) as of the last passive needs tick;
	 * negative until the first valid reading. */
	double LastAbsoluteHour = -1.0;
};
