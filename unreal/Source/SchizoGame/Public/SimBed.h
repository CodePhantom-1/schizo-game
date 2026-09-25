// SimBed.h — Use = sleep (plan.md's Player track verb: sit/sleep). The body
// goes to the kernel: sim_world_rest applies the asleep needs curve (fatigue
// drains, hunger/thirst still climb) and rests wounds.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimInteractable.h"
#include "SimBed.generated.h"

class UStaticMeshComponent;

UCLASS()
class SCHIZOGAME_API ASimBed : public AActor, public ISimInteractable
{
	GENERATED_BODY()

public:
	ASimBed();

	// -- ISimInteractable -----------------------------------------------------
	virtual FString GetVerbLabel() const override { return TEXT("Sleep"); }
	virtual void Interact(APawn* Instigator) override;

	/** Hours slept per Use (a night's sleep; rest 8+ hours speeds the day's healing). */
	UPROPERTY(EditAnywhere, Category = "Sim", meta = (ClampMin = "1", ClampMax = "12"))
	int32 SleepHours = 8;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	TObjectPtr<UStaticMeshComponent> MatMesh = nullptr;
};
