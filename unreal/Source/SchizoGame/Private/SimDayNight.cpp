// SimDayNight.cpp — see SimDayNight.h.
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
constexpr float kNightIntensity = 0.05f;   // real darkness — moonlight only
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
	FindOrSpawnSun();
}

void ASimDayNight::FindOrSpawnSun()
{
	TArray<AActor*> Tagged;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("SimSun"), Tagged);
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
		Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(0.f, 0.f, 0.f), Params);
		if (Sun != nullptr)
		{
			Sun->Tags.Add(FName("SimSun"));
			if (UDirectionalLightComponent* Comp = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
			{
				Comp->SetAtmosphereSunLight(true);
			}
		}
	}

	// A SkyLight so "real darkness" isn't a pure void indoors/under cover;
	// its intensity still swings hard between day and night below.
	TArray<AActor*> SkyTagged;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("SimSkyLight"), SkyTagged);
	for (AActor* Actor : SkyTagged)
	{
		if (ASkyLight* Found = Cast<ASkyLight>(Actor))
		{
			Fill = Found;
			break;
		}
	}
	if (Fill == nullptr)
	{
		Fill = GetWorld()->SpawnActor<ASkyLight>();
		if (Fill != nullptr)
		{
			Fill->Tags.Add(FName("SimSkyLight"));
		}
	}
}

void ASimDayNight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Sun == nullptr) return;

	const float Hour = USimWorldSubsystem::GetSimHour();
	if (Hour < 0.f) return;  // kernel not up yet

	// 15 degrees of pitch per hour (360/24): hour 0 = sun straight up
	// (below the horizon on the far side, midnight), hour 6 = horizon
	// (sunrise), hour 12 = straight down (zenith, noon), hour 18 = horizon
	// again (sunset). INVENTED mapping — canon gives no sun-path model.
	const float Pitch = Hour * 15.f - 90.f;
	Sun->SetActorRotation(FRotator(Pitch, 0.f, 0.f));

	const bool bNight = Hour < 6.f || Hour >= 18.f;
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
