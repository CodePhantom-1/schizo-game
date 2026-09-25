// SimInteractable.h — the common interaction contract (plan.md §6, UE-2 track).
// Anything the player's Use trace can act on implements this: doors, wells,
// pickups, beds. The player controller never downcasts to a concrete type —
// it casts to this interface and calls through it.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SimInteractable.generated.h"

UINTERFACE(BlueprintType)
class SCHIZOGAME_API USimInteractable : public UInterface
{
	GENERATED_BODY()
};

class SCHIZOGAME_API ISimInteractable
{
	GENERATED_BODY()

public:
	/** Short verb label for the HUD crosshair ("Open", "Drink", "Eat bread"...). */
	virtual FString GetVerbLabel() const { return TEXT("Use"); }

	/** Fired by the player's Use trace. Instigator is the pawn that used it. */
	virtual void Interact(APawn* Instigator) = 0;
};
