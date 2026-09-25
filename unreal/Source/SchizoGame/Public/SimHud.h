// SimHud.h — the player's state on screen (plan.md Phase 6 Player track):
// the crosshair's verb prompt, the body's needs as bars and numbers, the named
// conditions, the purse, and — while Tab is held — the carried goods. Canvas
// draw, no UMG asset: every number comes from the kernel's C API; the only
// strings here are short UI labels (the repo's no-prose rule).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SimHud.generated.h"

UCLASS()
class SCHIZOGAME_API ASimHud : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

protected:
	/** One needs row: label, bar (value is 0..100 from the kernel), number. */
	void DrawNeedRow(const FString& Label, int Value, float X, float& Y);
};
