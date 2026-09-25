// SimInteractable.h — the common interaction contract for the player's verbs
// (plan.md Phase 6 Player track: "the interaction verb set: touch, sit, eat,
// drink, carry, open, knock, pray, work"). Anything the player controller's
// Use trace can act on implements this: doors, wells, pickups, beds. The
// controller never downcasts to a concrete type — it casts to this interface
// and calls through it, so a new verb actor needs no controller change.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SimInteractable.generated.h"

class APawn;

UINTERFACE(BlueprintType)
class SCHIZOGAME_API USimInteractable : public UInterface
{
	GENERATED_BODY()
};

class SCHIZOGAME_API ISimInteractable
{
	GENERATED_BODY()

public:
	/** Short verb label for the HUD's crosshair prompt ("Open", "Drink", "Pick up grain"...). */
	virtual FString GetVerbLabel() const { return TEXT("Use"); }

	/** Fired by the player's Use trace. Instigator is the pawn that used it. */
	virtual void Interact(APawn* Instigator) = 0;
};
