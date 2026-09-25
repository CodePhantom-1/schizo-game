// SimDoor.h — a door that opens and closes on Use (plan.md's Player track
// verb: open). Grey-box: the leaf is a scaled engine cube that snaps between
// its closed yaw and closed yaw + OpenYawDegrees.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimInteractable.h"
#include "SimDoor.generated.h"

class UStaticMeshComponent;

UCLASS()
class SCHIZOGAME_API ASimDoor : public AActor, public ISimInteractable
{
	GENERATED_BODY()

public:
	ASimDoor();

	virtual void BeginPlay() override;

	// -- ISimInteractable -----------------------------------------------------
	virtual FString GetVerbLabel() const override;
	virtual void Interact(APawn* Instigator) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	TObjectPtr<UStaticMeshComponent> LeafMesh = nullptr;

	/** Yaw the leaf swings through when opened (from its authored rotation). */
	UPROPERTY(EditAnywhere, Category = "Sim")
	float OpenYawDegrees = 90.f;

	/** The yaw the leaf was authored at (closed). Captured at BeginPlay. */
	float ClosedYaw = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Sim")
	bool bOpen = false;
};
