// SimPlayerController.h — the Use verb and the player's hands (Phase 4 slice).
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

	/** HUD reads: the verb label of whatever's under the crosshair right now
	 * ("Open", "Drink", ""...), for the diegetic prompt. */
	UFUNCTION(BlueprintPure, Category = "Sim")
	FString GetLookVerbLabel() const { return CurrentVerbLabel; }

	/** HUD reads: Tab is held (show the carried-items overlay). */
	UFUNCTION(BlueprintPure, Category = "Sim")
	bool IsInventoryShown() const { return bInventoryHeld; }

	/** Resyncs the passive hourly needs tick after something else (a bed)
	 * has already advanced needs and the clock for a chunk of time — avoids
	 * double-applying hunger/thirst for the same hours. */
	void ResyncNeedsClock();

protected:
	/** The Use verb: trace from the camera; interact with what the street offers. */
	void OnUse();

	/** F: eat the best food the player is carrying. */
	void OnEat();

	/** G: drink the best drink (e.g. beer) the player is carrying. */
	void OnQuaff();

	void OnInventoryPressed();
	void OnInventoryReleased();

	/** Shared eye trace used by both Use and the HUD's per-frame look label. */
	bool TraceLook(struct FHitResult& OutHit) const;

	FString CurrentVerbLabel;
	bool bInventoryHeld = false;

	/** Absolute sim hour (day*24 + hour) as of the last passive needs tick;
	 * -1 until the first valid reading. */
	double LastAbsoluteHour = -1.0;
};
