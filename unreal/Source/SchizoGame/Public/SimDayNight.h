// SimDayNight.h — UE-4: the sun. Rotates a directional light from
// USimWorldSubsystem::GetSimHour() — sunrise ~6, sunset ~18, real darkness
// at night (the immersion rule), dim moonlight between.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimDayNight.generated.h"

UCLASS()
class SCHIZOGAME_API ASimDayNight : public AActor
{
	GENERATED_BODY()

public:
	ASimDayNight();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	/** Finds an actor tagged "SimSun"; spawns a directional light (+ a
	    SkyLight for night fill) if none exists. */
	void FindOrSpawnSun();

	UPROPERTY()
	TObjectPtr<class ADirectionalLight> Sun;

	UPROPERTY()
	TObjectPtr<class ASkyLight> Fill;

	float LastLoggedHour = -1.f;
};
