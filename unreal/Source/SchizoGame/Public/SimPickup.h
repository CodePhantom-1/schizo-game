// SimPickup.h — Use = carry (plan.md's Player track verb: carry). Adds ItemId
// x Quantity to the player's kernel inventory and vanishes: once the goods are
// in the sim, the actor has nothing left to be.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimInteractable.h"
#include "SimPickup.generated.h"

class UStaticMeshComponent;

UCLASS()
class SCHIZOGAME_API ASimPickup : public AActor, public ISimInteractable
{
	GENERATED_BODY()

public:
	ASimPickup();

	// -- ISimInteractable -----------------------------------------------------
	virtual FString GetVerbLabel() const override;
	virtual void Interact(APawn* Instigator) override;

	/** Canon item id (Content/Sim/canon/items.csv, e.g. "grain", "bread", "beer"). */
	UPROPERTY(EditAnywhere, Category = "Sim")
	FString ItemId = TEXT("grain");

	/** Units the pickup carries into the inventory. */
	UPROPERTY(EditAnywhere, Category = "Sim", meta = (ClampMin = "1"))
	int32 Quantity = 1;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	TObjectPtr<UStaticMeshComponent> PickupMesh = nullptr;
};
