// SimGameMode.h — the grey-box street (Phase 4 slice, first step).
// Builds the City of the Moon's gate street in code — no assets yet: the
// floor, the moon gate's walls, and the sim clock on screen. Everything here
// is placeholder grey-box; the architecture kits replace it at content waves.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SimGameMode.generated.h"

UCLASS()
class SCHIZOGAME_API ASimGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASimGameMode();
	virtual void BeginPlay() override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName = TEXT("")) override;

protected:
	/** Spawns one scaled engine cube (grey-box stone). */
	void SpawnBox(const FVector& Location, const FVector& Scale, const FLinearColor& Color);

	/** The sim day the clock shows (to avoid re-reading the C API every line). */
	int64 LastShownDay = -1;

	/** The street's PlayerStart: the only start spot this game recognises. */
	UPROPERTY()
	class APlayerStart* StreetStart = nullptr;
};
