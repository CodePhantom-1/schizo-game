// SimWell.h — Use = drink water (plan.md's Player track verb: drink). Water is
// the kernel's virtual always-drinkable id (CApi.h: sim_world_drink's "water"
// never touches the inventory) — the well is where the id lives in the world.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimInteractable.h"
#include "SimWell.generated.h"

class UStaticMeshComponent;

UCLASS()
class SCHIZOGAME_API ASimWell : public AActor, public ISimInteractable
{
	GENERATED_BODY()

public:
	ASimWell();

	// -- ISimInteractable -----------------------------------------------------
	virtual FString GetVerbLabel() const override { return TEXT("Drink"); }
	virtual void Interact(APawn* Instigator) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	TObjectPtr<UStaticMeshComponent> WellMesh = nullptr;
};
