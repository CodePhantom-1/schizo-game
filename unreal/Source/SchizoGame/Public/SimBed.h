// SimBed.h — Use = sleep, advancing needs and skipping the clock (plan.md §6
// verb: sit/sleep).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimInteractable.h"
#include "SimBed.generated.h"

UCLASS()
class SCHIZOGAME_API ASimBed : public AActor, public ISimInteractable
{
	GENERATED_BODY()

public:
	ASimBed();

	virtual FString GetVerbLabel() const override { return TEXT("Sleep"); }
	virtual void Interact(APawn* Instigator) override;

	/** Hours slept per Use (a night's sleep, plan.md default). */
	UPROPERTY(EditAnywhere, Category = "Sim")
	int32 SleepHours = 8;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	class UStaticMeshComponent* MatMesh = nullptr;
};
