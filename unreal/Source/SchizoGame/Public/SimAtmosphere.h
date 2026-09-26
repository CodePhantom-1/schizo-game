// SimAtmosphere.h — Stage V batch 5: the weather made visible. Reads the kernel (weather, drought, season,
// the hour, today's festival), maps it to one target state (a pure function, tested), eases the live
// state toward it (60 s; wetness rises with the rain and dries over two in-game hours), and writes it to
// MPC_World (Wet, Dust, Festival, Shimmer) and to Tommy's ASimDayNight (SetWeatherBlend). Clear weather
// with no drought is all zeros: the world looks exactly as it did.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimAtmosphere.generated.h"

class ASimDayNight;
class UInstancedStaticMeshComponent;
class UMaterialParameterCollection;

struct FSimAtmosphere
{
	float Dust = 0.f;      // 0..1: haze thickness and ochre (a sandstorm is 1)
	float Rain = 0.f;      // 0..1: falling rain
	float Overcast = 0.f;  // 0..1: cloud cover, a dimmer sun
	float Fog = 0.f;       // 0..1: marsh fog (the haze x4)
	float Wet = 0.f;       // 0..1: wet surfaces (darker, glossier)
	float Wither = 0.f;    // 0..1: the drought's browning (ASimScatter writes MPC_World.Wither)
	float Shimmer = 0.f;   // 0..1: heat shimmer strength
	bool bFestival = false;
	FName Weather;
};

UCLASS()
class SCHIZOGAME_API ASimAtmosphere : public AActor
{
	GENERATED_BODY()

public:
	ASimAtmosphere();

	static ASimAtmosphere* BuildAtmosphere(UWorld* World);

	/** The state a weather id (weather.csv), drought stage, season, hour and festival call for. Pure. */
	static FSimAtmosphere Target(FName WeatherId, int32 Drought, const FString& Season, float Hour, bool bFestival);

	/** Eases Live toward Target over Dt real seconds (time constant 60 s); Wet dries over two in-game hours. */
	static void Ease(FSimAtmosphere& Live, const FSimAtmosphere& Target, float Dt, float GameHours);

	/** sim.Weather <id> (a visual override of the kernel's weather; "auto" clears it). */
	static FName WeatherOverride;

	const FSimAtmosphere& GetLive() const { return Live; }

	/** Is a smoke source of this place typology lit at this hour (ovens at dawn, homes and fires at dusk, the
	 *  foundry and armourer all day; nothing in the rain)? Pure. */
	static bool SmokeOn(const FString& Typology, float Hour, float Rain);

	/** Builds the rain and dust fields and the smoke puffs (Content/Sim/smoke.csv); nothing if not imported. */
	void BuildFx();
	/** Shows and places the effects for a state, an hour and the camera (Tick; tests call it directly). */
	void ApplyFx(const FSimAtmosphere& State, float Hour, FVector Camera);

	static constexpr int32 RainCount = 1500;
	static constexpr int32 DustCount = 800;
	static constexpr int32 PuffsPerSource = 5;
	UInstancedStaticMeshComponent* GetRain() const { return RainIsm; }
	UInstancedStaticMeshComponent* GetDust() const { return DustIsm; }
	UInstancedStaticMeshComponent* GetSmoke() const { return SmokeIsm; }
	int32 NumSmokeSources() const { return SmokeTypology.Num(); }
	bool IsSmokeLit(int32 Source) const { return SmokeLit.IsValidIndex(Source) && SmokeLit[Source]; }

	virtual void Tick(float DeltaSeconds) override;

private:
	void Apply();

	FSimAtmosphere Live;
	float LastHour = -1.f;
	bool bFirst = true;
	UPROPERTY() TObjectPtr<UMaterialParameterCollection> WorldParams;
	UPROPERTY() TObjectPtr<ASimDayNight> Sky;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RainIsm;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> DustIsm;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> SmokeIsm;
	TArray<FString> SmokeTypology;
	TArray<bool> SmokeLit;
};
