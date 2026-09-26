// SimDayNight.h — W6-C: the sun, moon and sky on the sim clock. Purely a
// REFLECTION of the kernel's clock (USimWorldSubsystem::GetSimHour): sunrise
// ~6 over the sea in the east, the sun arcs through the south, sunset ~18
// over the fields in the west; a pale moon crosses the night.
//
// D-023 carries the look on light, fog and colour, so this actor owns the
// whole atmosphere: a physical sky (SkyAtmosphere), one sun and one moon
// (atmosphere lights 0 and 1, the sun wins forward shading), a real-time
// captured SkyLight for ambient, volumetric clouds, a warm drought haze with
// volumetric fog (god rays through dust), and an unbound post-process volume
// with the grade (warm highlights, cool shadows, bloom, vignette, grain).
//
// Linux/AMD (RADV on the designer's RX 5700 XT) wedges on the captured
// SkyLight and on volumetric clouds (DECISIONS.md), so on Linux those two
// are skipped and a shadowless fill light stands in, as before.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimDayNight.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UVolumetricCloudComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;

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

	/**
	 * The moon's angle ahead of the sun, radians 0..2pi, for a day of the
	 * kernel's lunar month (1..DaysPerMonth) and hour: 0 at the new crescent
	 * (day 1, 00:00), pi at full (the kernel's middle day), back to 2pi at the
	 * month's end. Continuous through every midnight (no nightly jump).
	 */
	static float MoonOrbitAngle(int32 DayOfMonth, float Hour, int32 DaysPerMonth = 30);

	/**
	 * V-B5: the weather over Tommy's clear-day look (ASimAtmosphere drives it). Each 0..1; all 0 = his values
	 * exactly. Dust thickens the haze (x8) and turns it ochre; Rain and Overcast grey it and dim the sun; Fog
	 * thickens it (x4). The haze never starts nearer than 3 m (the player's own feet stay clear).
	 */
	void SetWeatherBlend(float Dust, float Rain, float Overcast, float Fog = 0.f);

protected:
	virtual void BeginPlay() override;

private:
	void FindOrSpawnSky();
	void ApplyHour(float Hour);

	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> SkyFill;  // Linux only (no captured sky light there)
	UPROPERTY() TObjectPtr<USkyLightComponent> Fill;
	UPROPERTY() TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY() TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Haze;
	UPROPERTY() TObjectPtr<UPostProcessComponent> Grade;
	float WeatherDust = 0.f, WeatherRain = 0.f, WeatherOvercast = 0.f, WeatherFog = 0.f;

	float LastLoggedHour = -1.f;
};
