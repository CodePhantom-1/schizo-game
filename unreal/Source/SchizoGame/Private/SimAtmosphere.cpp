// SimAtmosphere.cpp — Stage V batch 5 Task 1: the atmosphere driver.
#include "SimAtmosphere.h"

#include "SimDayNight.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"
#include "sim/CApiWild.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimAtmosphere, Log, All);

FName ASimAtmosphere::WeatherOverride = NAME_None;

namespace
{
	constexpr float EaseSeconds = 60.f;    // weather blends over a real minute (Review Focus 1)
	constexpr float DryGameHours = 2.f;    // wet ground dries over two in-game hours (Review Focus 3)

	FAutoConsoleCommand CmdWeather(TEXT("sim.Weather"),
		TEXT("sim.Weather <clear|hot|scorching|sandstorm|rain|fog|auto> — show that weather (a visual override; auto = the kernel's)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			ASimAtmosphere::WeatherOverride = (Args.Num() == 0 || Args[0] == TEXT("auto")) ? NAME_None : FName(*Args[0]);
			UE_LOG(LogSimAtmosphere, Log, TEXT("sim.Weather: %s"), ASimAtmosphere::WeatherOverride.IsNone() ? TEXT("the kernel's") : *ASimAtmosphere::WeatherOverride.ToString());
		}));

	float Approach(float From, float To, float Dt, float Tau)
	{
		return To + (From - To) * FMath::Exp(-Dt / Tau);
	}
}

ASimAtmosphere::ASimAtmosphere()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ASimAtmosphere* ASimAtmosphere::BuildAtmosphere(UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<ASimAtmosphere> It(World); It; ++It)
	{
		return *It;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ASimAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, P);
}

FSimAtmosphere ASimAtmosphere::Target(FName WeatherId, int32 Drought, const FString& Season, float Hour, bool bFestival)
{
	FSimAtmosphere T;
	T.Weather = WeatherId;
	T.bFestival = bFestival;
	const FString W = WeatherId.ToString();
	if (W == TEXT("hot")) { T.Dust = 0.12f; T.Shimmer = 0.4f; }
	else if (W == TEXT("scorching")) { T.Dust = 0.2f; T.Shimmer = 1.f; }
	else if (W == TEXT("sandstorm")) { T.Dust = 1.f; T.Overcast = 0.3f; }
	else if (W == TEXT("rain")) { T.Rain = 1.f; T.Overcast = 0.7f; }
	else if (W == TEXT("fog"))
	{
		// Marsh fog, thickest at dawn (it lifts by mid-morning).
		const float Dawn = 1.f - FMath::SmoothStep(8.f, 11.f, Hour);
		T.Overcast = 0.5f;
		T.Fog = FMath::Max(0.5f, Dawn);
	}
	// The drought, visible even on a clear day: the browning and a dusty sky.
	const float D = FMath::Clamp(Drought / 4.f, 0.f, 1.f);
	T.Wither = D;
	T.Dust = FMath::Max(T.Dust, 0.3f * D);
	T.Wet = T.Rain;  // the ground's target; Ease dries it slowly
	return T;
}

void ASimAtmosphere::Ease(FSimAtmosphere& L, const FSimAtmosphere& T, float Dt, float GameHours)
{
	L.Dust = Approach(L.Dust, T.Dust, Dt, EaseSeconds);
	L.Rain = Approach(L.Rain, T.Rain, Dt, EaseSeconds);
	L.Overcast = Approach(L.Overcast, T.Overcast, Dt, EaseSeconds);
	L.Fog = Approach(L.Fog, T.Fog, Dt, EaseSeconds);
	L.Shimmer = Approach(L.Shimmer, T.Shimmer, Dt, EaseSeconds);
	L.Wither = Approach(L.Wither, T.Wither, Dt, EaseSeconds);
	// Wet: soaks as fast as the rain comes; dries linearly over two in-game hours once it stops.
	if (T.Wet > L.Wet)
	{
		L.Wet = Approach(L.Wet, T.Wet, Dt, EaseSeconds);
	}
	else
	{
		L.Wet = FMath::Max(T.Wet, L.Wet - FMath::Max(0.f, GameHours) / DryGameHours);
	}
	L.bFestival = T.bFestival;
	L.Weather = T.Weather;
}

void ASimAtmosphere::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	const float Hour = USimWorldSubsystem::GetSimHourFor(World);
	if (Hour < 0.f)
	{
		return;  // no kernel world yet
	}
	SimWorld* H = USimWorldSubsystem::GetSimHandleFor(World);
	char Buf[64] = {};
	if (H != nullptr)
	{
		sim_world_weather(H, Buf, sizeof(Buf));
	}
	const FName Weather = !WeatherOverride.IsNone() ? WeatherOverride : FName(Buf[0] ? UTF8_TO_TCHAR(Buf) : TEXT("clear"));
	const int32 Drought = FMath::Max(0, USimWorldSubsystem::GetSimDroughtFor(World));
	const bool bFestival = H != nullptr && sim_world_is_festival(H) == 1;
	const FSimAtmosphere T = Target(Weather, Drought, USimWorldSubsystem::GetSimSeasonFor(World), Hour, bFestival);
	float GameHours = LastHour < 0.f ? 0.f : Hour - LastHour;
	if (GameHours < 0.f)
	{
		GameHours += 24.f;  // midnight
	}
	LastHour = Hour;
	if (bFirst)
	{
		Live = T;  // the world opens in its weather, not easing into it
		bFirst = false;
	}
	else
	{
		Ease(Live, T, DeltaSeconds, GameHours > 6.f ? 0.f : GameHours);  // a skipped night is not a drying spell
	}
	Apply();
}

void ASimAtmosphere::Apply()
{
	UWorld* World = GetWorld();
	if (WorldParams == nullptr && FPackageName::DoesPackageExist(TEXT("/Game/Art/Scatter/MPC_World")))
	{
		WorldParams = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Art/Scatter/MPC_World.MPC_World"));
	}
	if (WorldParams != nullptr)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(World, WorldParams, TEXT("Wet"), Live.Wet);
		UKismetMaterialLibrary::SetScalarParameterValue(World, WorldParams, TEXT("Dust"), Live.Dust);
		UKismetMaterialLibrary::SetScalarParameterValue(World, WorldParams, TEXT("Festival"), Live.bFestival ? 1.f : 0.f);
		UKismetMaterialLibrary::SetScalarParameterValue(World, WorldParams, TEXT("Shimmer"), Live.Shimmer);
	}
	if (Sky == nullptr)
	{
		for (TActorIterator<ASimDayNight> It(World); It; ++It)
		{
			Sky = *It;
			break;
		}
	}
	if (Sky != nullptr)
	{
		Sky->SetWeatherBlend(Live.Dust, Live.Rain, Live.Overcast, Live.Fog);
	}
}
