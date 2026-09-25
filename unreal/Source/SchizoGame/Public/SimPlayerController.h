// SimPlayerController.h — the Use verb and the player's hands (Phase 4 slice).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SimPlayerController.generated.h"

UCLASS()
class SCHIZOGAME_API ASimPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

protected:
	/** The Use verb: trace from the camera; interact with what the street offers. */
	void OnUse();
};
