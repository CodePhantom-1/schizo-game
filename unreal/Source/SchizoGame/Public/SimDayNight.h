// SimDayNight.h — W6-C: the sun and sky on the sim clock. Rotates a
// directional light from USimWorldSubsystem::GetSimHour — sunrise ~6, zenith
// at 12, sunset ~18, real darkness at night (moonlight only, per the
// immersion rule). Purely a REFLECTION of the kernel's clock and schedule
// world: the kernel already closes the shops (schedule rows move residents
// home); this actor only makes the light agree.
//
// Ambient: a captured SkyLight wedges the GPU on this machine (DECISIONS.md),
// so a second, dim, shadowless directional ("SimSkyFill") stands in for sky
// bounce, and an exponential height fog stands in for the sky itself — no
// cubemap captures anywhere.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimDayNight.generated.h"

class ADirectionalLight;
class AExponentialHeightFog;
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
	/** Finds actors tagged SimSun / SimSkyLight / SimSkyFill in the level;
	    spawns the sun, the shadowless fill and the fog where none exist. */
	void FindOrSpawnLights();

	UPROPERTY()
	TObjectPtr<ADirectionalLight> Sun;

	/** A sky light the level already placed (kept for authored maps); never
	    spawned here — see the header comment. */
	UPROPERTY()
	TObjectPtr<ASkyLight> Fill;

	UPROPERTY()
	TObjectPtr<ADirectionalLight> SkyFill;

	UPROPERTY()
	TObjectPtr<AExponentialHeightFog> Haze;

	float LastLoggedHour = -1.f;
};
