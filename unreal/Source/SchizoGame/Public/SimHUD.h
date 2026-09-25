// SimHUD.h — minimal diegetic HUD (plan.md §6 item 6): the verb label of
// what you look at, need effects as words, and carried items while Tab is
// held. Canvas draw, no UMG asset.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SimHUD.generated.h"

UCLASS()
class SCHIZOGAME_API ASimHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;
};
