// SimDoor.h — a door that opens/closes on Use (plan.md §6 verb: open).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimInteractable.h"
#include "SimDoor.generated.h"

UCLASS()
class SCHIZOGAME_API ASimDoor : public AActor, public ISimInteractable
{
	GENERATED_BODY()

public:
	ASimDoor();

	// -- ISimInteractable -----------------------------------------------------
	virtual FString GetVerbLabel() const override;
	virtual void Interact(APawn* Instigator) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	class UStaticMeshComponent* LeafMesh = nullptr;

	/** Yaw the leaf swings through when opened. */
	UPROPERTY(EditAnywhere, Category = "Sim")
	float OpenYawDegrees = 90.f;

	UPROPERTY(VisibleAnywhere, Category = "Sim")
	bool bOpen = false;
};
