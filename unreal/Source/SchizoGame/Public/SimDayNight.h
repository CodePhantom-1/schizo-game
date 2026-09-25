// SimDayNight.h — W6-C: the sun and sky on the sim clock. Rotates a
// directional light from USimWorldSubsystem::GetSimHour — sunrise ~6, zenith
// at 12, sunset ~18, real darkness at night (moonlight only, per the
// immersion rule). Purely a REFLECTION of the kernel's clock and schedule
// world: the kernel already closes the shops (schedule rows move residents
// home); this actor only makes the light agree.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimDayNight.generated.h"

class ADirectionalLight;
class ASkyLight;

UCLASS()
class SCHIZOGAME_API ASimDayNight : public AActor
{
	GENERATED_BODY()

public:
	ASimDayNight();

	virtual void Tick(float DeltaSeconds) override;

	/** True when the sim clock says night (hour < 6 or >= 18): the shops are
	    shut and the watch is out. Static so the HUD/doors can agree with the
	    sun without re-deriving it. */
	UFUNCTION(BlueprintPure, Category = "Sim")
	static bool IsSimNightHour(float Hour);

protected:
	virtual void BeginPlay() override;

private:
	/** Finds actors tagged SimSun / SimSkyLight in the level; spawns a
	    directional light (+ a sky light for night fill) where none exists. */
	void FindOrSpawnLights();

	UPROPERTY()
	TObjectPtr<ADirectionalLight> Sun;

	UPROPERTY()
	TObjectPtr<ASkyLight> Fill;

	float LastLoggedHour = -1.f;
};
