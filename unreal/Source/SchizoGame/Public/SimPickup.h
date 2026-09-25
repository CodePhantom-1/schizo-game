// SimPickup.h — Use = add to inventory, then vanish (plan.md §6 verb: carry).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimInteractable.h"
#include "SimPickup.generated.h"

UCLASS()
class SCHIZOGAME_API ASimPickup : public AActor, public ISimInteractable
{
	GENERATED_BODY()

public:
	ASimPickup();

	virtual FString GetVerbLabel() const override;
	virtual void Interact(APawn* Instigator) override;

	/** Canon item id (db/canon/items.csv, e.g. "grain", "bread"). */
	UPROPERTY(EditAnywhere, Category = "Sim")
	FString ItemId = TEXT("grain");

	UPROPERTY(EditAnywhere, Category = "Sim")
	int32 Quantity = 1;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	class UStaticMeshComponent* PickupMesh = nullptr;
};
