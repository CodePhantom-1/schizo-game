// SimWell.h — Use = drink water (plan.md §6 verb: drink).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimInteractable.h"
#include "SimWell.generated.h"

UCLASS()
class SCHIZOGAME_API ASimWell : public AActor, public ISimInteractable
{
	GENERATED_BODY()

public:
	ASimWell();

	virtual FString GetVerbLabel() const override { return TEXT("Drink"); }
	virtual void Interact(APawn* Instigator) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	class UStaticMeshComponent* WellMesh = nullptr;
};
