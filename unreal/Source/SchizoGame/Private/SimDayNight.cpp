// SimDayNight.cpp — see SimDayNight.h. INVENTED sun-path model (no canon
// source): the standard 24-hour linear sweep, 15 degrees per hour.
#include "SimDayNight.h"

#include "SimWorldSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimDayNight, Log, All);

namespace
{
constexpr float kNightIntensity = 0.05f;  // real darkness — moonlight only
constexpr float kDayIntensity = 8.f;
constexpr float kFillNightIntensity = 0.02f;
constexpr float kFillDayIntensity = 1.0f;
}  // namespace

ASimDayNight::ASimDayNight()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASimDayNight::BeginPlay()
{
	Super::BeginPlay();
	FindOrSpawnLights();
}

bool ASimDayNight::IsSimNightHour(const float Hour)
{
	// INVENTED boundary (canon gives no day/night flag): dawn is the
	// gatekeeper's gates_open_at_dawn row (hour 6) and dusk sits just after
	// gates_close_at_dusk (hour 18) — the hours between are "day" for light.
	return Hour < 6.f || Hour >= 18.f;
}

void ASimDayNight::FindOrSpawnLights()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// Prefer lights the level (or the street builder) already placed, so a
	// future authored map keeps its own sun; only spawn when none is tagged.
	TArray<AActor*> Tagged;
	UGameplayStatics::GetAllActorsWithTag(this, FName("SimSun"), Tagged);
	for (AActor* Actor : Tagged)
	{
		if (ADirectionalLight* Found = Cast<ADirectionalLight>(Actor))
		{
			Sun = Found;
			break;
		}
	}
	if (Sun == nullptr)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Sun = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(0.f, 0.f, 0.f), Params);
		if (Sun != nullptr)
		{
			Sun->Tags.Add(FName("SimSun"));
			if (UDirectionalLightComponent* Comp = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
			{
				Comp->SetAtmosphereSunLight(true);
			}
		}
	}

	// A sky light (only one an authored map already placed): so "real
	// darkness" is not a pure void under cover; its intensity still swings
	// hard between day and night below. We never SPAWN one: a captured-scene
	// SkyLight issues a CubemapCapture whose GPU work wedges the render
	// thread on this machine (RADV / RX 5700 XT) — see SimStreetBuilder.
	TArray<AActor*> SkyTagged;
	UGameplayStatics::GetAllActorsWithTag(this, FName("SimSkyLight"), SkyTagged);
	for (AActor* Actor : SkyTagged)
	{
		if (ASkyLight* Found = Cast<ASkyLight>(Actor))
		{
			Fill = Found;
			break;
		}
	}
}

void ASimDayNight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Sun == nullptr)
	{
		return;
	}

	const float Hour = USimWorldSubsystem::GetSimHourFor(this);
	if (Hour < 0.f)
	{
		return;  // the kernel world does not exist yet — nothing to reflect
	}

	// 15 degrees of pitch per hour (360/24): hour 0 = the rotation's zero
	// (sun straight up on the far side — below the horizon, midnight),
	// hour 6 = horizon (sunrise), hour 12 = straight down onto the street
	// (zenith, noon), hour 18 = horizon again (sunset).
	const float Pitch = Hour * 15.f - 90.f;
	Sun->SetActorRotation(FRotator(Pitch, 0.f, 0.f));

	const bool bNight = IsSimNightHour(Hour);
	if (UDirectionalLightComponent* Comp = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
	{
		Comp->SetIntensity(bNight ? kNightIntensity : kDayIntensity);
	}
	if (Fill != nullptr)
	{
		if (USkyLightComponent* SkyComp = Fill->GetLightComponent())
		{
			SkyComp->SetIntensity(bNight ? kFillNightIntensity : kFillDayIntensity);
		}
	}

	if (FMath::FloorToInt(Hour) != FMath::FloorToInt(LastLoggedHour))
	{
		LastLoggedHour = Hour;
		UE_LOG(LogSimDayNight, Log, TEXT("Sun: hour %.2f, pitch %.1f, %s."), Hour, Pitch,
			bNight ? TEXT("night (moonlight)") : TEXT("day"));
	}
}
