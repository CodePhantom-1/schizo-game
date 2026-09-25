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

	float LastLoggedHour = -1.f;
};
