// SimAtmosphere.cpp — Stage V batch 5 Task 1: the atmosphere driver.
#include "SimAtmosphere.h"

#include "SimCityData.h"
#include "SimDayNight.h"
#include "SimMeshKit.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"
#include "sim/CApiWild.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
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

	TAutoConsoleVariable<int32> CVarZodiac(TEXT("sim.Zodiac"), 0,
		TEXT("sim.Zodiac 1 — draw the twelve zodiac figures among the stars (the D-025 astrology UI will switch it later)"));

	constexpr float kSkyLatitudeDeg = 31.f;  // the city's latitude: the pole stands this high in the north
	constexpr float kStarGain = 12.f;        // the stars' emissive at full night (night exposure floor EV 2)

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
	ASimAtmosphere* A = World->SpawnActor<ASimAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (A != nullptr)
	{
		A->BuildFx();
		A->BuildSky();
	}
	return A;
}

bool ASimAtmosphere::SmokeOn(const FString& Typology, float Hour, float Rain)
{
	if (Rain > 0.5f)
	{
		return false;  // the rain puts the fires out
	}
	if (Typology == TEXT("foundry") || Typology == TEXT("armourer"))
	{
		return true;  // the furnaces never go cold
	}
	if (Typology == TEXT("bakery") || Typology == TEXT("temple_kitchens") || Typology == TEXT("cookshop") || Typology == TEXT("potter"))
	{
		return Hour >= 5.f && Hour < 8.f;  // the ovens at dawn
	}
	return Hour >= 17.f && Hour < 20.f;  // every other hearth and fire: the evening meal
}

void ASimAtmosphere::BuildFx()
{
	auto Load = [](const TCHAR* Name) -> UStaticMesh*
	{
		const FString Pkg = FString::Printf(TEXT("/Game/Art/FX/%s"), Name);
		return FPackageName::DoesPackageExist(Pkg) ? LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *Pkg, Name)) : nullptr;
	};
	auto Make = [this](UStaticMesh* Mesh, const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this, Name);
		I->SetupAttachment(RootComponent);
		I->SetMobility(EComponentMobility::Movable);
		I->SetStaticMesh(Mesh);
		I->NumCustomDataFloats = 1;
		I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		I->SetCastShadow(false);
		I->SetBoundsScale(4.f);  // the material moves every card metres from its instance (falls, drifts, rises): never cull early
		I->RegisterComponent();
		AddInstanceComponent(I);
		return I;
	};
	UStaticMesh* Rain = Load(TEXT("SM_FX_Rain"));
	UStaticMesh* Dust = Load(TEXT("SM_FX_Dust"));
	UStaticMesh* Smoke = Load(TEXT("SM_FX_Smoke"));
	if (Rain == nullptr || Dust == nullptr || Smoke == nullptr)
	{
		UE_LOG(LogSimAtmosphere, Log, TEXT("atmosphere: the weather's cards are not imported — run tools/art/ue_make_fx_materials.py."));
		return;
	}
	// Rain: streaks in a 30 m box that rides with the camera; the material makes them fall.
	RainIsm = Make(Rain, TEXT("Rain"));
	for (int32 i = 0; i < RainCount; ++i)
	{
		RainIsm->AddInstance(FTransform(FVector((SimHash01(i, 1) - 0.5f) * 3000.f, (SimHash01(i, 2) - 0.5f) * 3000.f, SimHash01(i, 3) * 3000.f)));
	}
	// Dust: motes over the ground, blown 30 m across the box by the material.
	DustIsm = Make(Dust, TEXT("Dust"));
	for (int32 i = 0; i < DustCount; ++i)
	{
		DustIsm->AddInstance(FTransform(FVector(0.f, (SimHash01(i, 4) - 0.5f) * 3000.f, SimHash01(i, 5) * 800.f)));
	}
	// Smoke: puffs at every soot source the building grammar wrote (Content/Sim/smoke.csv).
	SmokeIsm = Make(Smoke, TEXT("Smoke"));
	TArray<FString> Lines;
	FFileHelper::LoadFileToStringArray(Lines, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Sim/smoke.csv")));
	for (int32 l = 1; l < Lines.Num(); ++l)
	{
		const TArray<FString> F = SimCityData::SplitCsv(Lines[l]);
		if (F.Num() < 4)
		{
			continue;
		}
		const FSimCityPlace* Place = SimCityData::Find(FName(*F[0]));
		SmokeTypology.Add(Place != nullptr ? Place->Typology : FString());
		SmokeLit.Add(false);
		const FVector At(FCString::Atof(*F[1]), FCString::Atof(*F[2]), FCString::Atof(*F[3]));
		for (int32 p = 0; p < PuffsPerSource; ++p)
		{
			const int32 I = SmokeIsm->AddInstance(FTransform(FRotator(0.f, 360.f * SimHash01(l, p, 6), 0.f), At, FVector(1.5f + 1.0f * SimHash01(l, p, 7))));
			SmokeIsm->SetCustomDataValue(I, 0, 0.f, false);
		}
	}
	RainIsm->SetVisibility(false);
	DustIsm->SetVisibility(false);
	UE_LOG(LogSimAtmosphere, Log, TEXT("atmosphere: %d rain streaks, %d dust motes, %d smoke sources."), RainCount, DustCount, SmokeTypology.Num());
}

void ASimAtmosphere::ApplyFx(const FSimAtmosphere& State, float Hour, FVector Camera)
{
	if (RainIsm != nullptr)
	{
		RainIsm->SetVisibility(State.Rain > 0.01f);
		RainIsm->SetWorldLocation(Camera + FVector(0.f, 0.f, -500.f));  // streaks from 25 m above the eye fall to 35 m below it
	}
	if (DustIsm != nullptr)
	{
		DustIsm->SetVisibility(State.Dust > 0.01f);
		DustIsm->SetWorldLocation(Camera + FVector(-1500.f, 0.f, -200.f));
	}
	if (SmokeIsm != nullptr)
	{
		bool bDirty = false;
		for (int32 s = 0; s < SmokeTypology.Num(); ++s)
		{
			const bool bOn = SmokeOn(SmokeTypology[s], Hour, State.Rain);
			if (bOn != SmokeLit[s])
			{
				SmokeLit[s] = bOn;
				for (int32 p = 0; p < PuffsPerSource; ++p)
				{
					SmokeIsm->SetCustomDataValue(s * PuffsPerSource + p, 0, bOn ? 1.f : 0.f, false);
				}
				bDirty = true;
			}
		}
		if (bDirty)
		{
			SmokeIsm->MarkRenderStateDirty();
		}
	}
}

float ASimAtmosphere::SiderealAngle(int32 Day, float Hour)
{
	return static_cast<float>(FMath::Fmod(Day * 360.9856 + Hour * 15.041, 360.0));
}

FVector ASimAtmosphere::CelestialPole()
{
	const float Phi = FMath::DegreesToRadians(kSkyLatitudeDeg);
	return FVector(0.f, -FMath::Cos(Phi), FMath::Sin(Phi));  // SimCompass: north is -Y
}

float ASimAtmosphere::NightFactor(float Hour, const FSimAtmosphere& State)
{
	// The sun's height on ASimDayNight's day circle (62 degrees at noon): the stars come out as it sinks below.
	const float SunZ = FMath::Sin((Hour - 6.f) / 12.f * PI) * FMath::Sin(FMath::DegreesToRadians(62.f));
	const float Dark = 1.f - FMath::SmoothStep(-0.2f, 0.02f, SunZ);
	return Dark * (1.f - State.Overcast) * (1.f - 0.85f * State.Dust) * (1.f - State.Fog);
}

void ASimAtmosphere::BuildSky()
{
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* Mat = FPackageName::DoesPackageExist(TEXT("/Game/Art/FX/M_Star"))
		? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/FX/M_Star.M_Star")) : nullptr;
	TArray<FString> Lines;
	FFileHelper::LoadFileToStringArray(Lines, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Sim/stars.csv")));
	if (Plane == nullptr || Mat == nullptr || Lines.Num() < 2)
	{
		UE_LOG(LogSimAtmosphere, Log, TEXT("atmosphere: no night sky — run tools/art/star_dome.py and ue_make_fx_materials.py."));
		return;
	}
	SkyRoot = NewObject<USceneComponent>(this, TEXT("Sky"));
	SkyRoot->SetupAttachment(RootComponent);
	SkyRoot->SetMobility(EComponentMobility::Movable);
	SkyRoot->RegisterComponent();
	AddInstanceComponent(SkyRoot);
	StarMat = UMaterialInstanceDynamic::Create(Mat, this);
	StarMat->SetScalarParameterValue(TEXT("Night"), 0.f);
	auto Make = [this, Plane](const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this, Name);
		I->SetupAttachment(SkyRoot);
		I->SetMobility(EComponentMobility::Movable);
		I->SetStaticMesh(Plane);
		I->SetMaterial(0, StarMat);
		I->NumCustomDataFloats = 4;
		I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		I->SetCastShadow(false);
		I->RegisterComponent();
		AddInstanceComponent(I);
		return I;
	};
	StarIsm = Make(TEXT("Stars"));
	ZodiacIsm = Make(TEXT("Zodiac"));
	MilkyIsm = Make(TEXT("MilkyWay"));
	// kind,hr,con,ra,dec,x,y,z,x2,y2,z2,size,r,g,b — unit vectors at sidereal angle 0, sizes in cm.
	for (int32 l = 1; l < Lines.Num(); ++l)
	{
		TArray<FString> F;
		Lines[l].ParseIntoArray(F, TEXT(","), false);
		if (F.Num() < 15)
		{
			continue;
		}
		auto V = [&F](int32 i) { return FVector(FCString::Atof(*F[i]), FCString::Atof(*F[i + 1]), FCString::Atof(*F[i + 2])); };
		const FVector A = V(5), B = V(8);
		const float Size = FCString::Atof(*F[11]) / 100.f;  // the plane is 1 m
		const bool bZodiac = F[0] == TEXT("zodiac");
		const TArray<float> Custom = {FCString::Atof(*F[12]), FCString::Atof(*F[13]), FCString::Atof(*F[14]), bZodiac ? 1.f : 0.f};
		UInstancedStaticMeshComponent* Into = bZodiac ? ZodiacIsm : F[0] == TEXT("milky") ? MilkyIsm : StarIsm;
		FTransform T;
		if (bZodiac)
		{
			// A thin card from star to star, facing the eye (its +Z toward the dome's centre).
			const FVector Mid = (A + B) * 0.5f * SkyRadius;
			T = FTransform(FRotationMatrix::MakeFromZX(-Mid, B - A).ToQuat(), Mid, FVector((B - A).Size() * SkyRadius / 100.f, Size, 1.f));
		}
		else
		{
			T = FTransform(FRotationMatrix::MakeFromZ(-A).ToQuat(), A * SkyRadius, FVector(Size, Size, 1.f));
		}
		const int32 I = Into->AddInstance(T);
		Into->SetCustomData(I, Custom, false);
		if (Into == StarIsm)
		{
			StarByHr.Add(FCString::Atoi(*F[1]), I);
		}
	}
	StarIsm->MarkRenderStateDirty();
	ZodiacIsm->MarkRenderStateDirty();
	MilkyIsm->MarkRenderStateDirty();
	UE_LOG(LogSimAtmosphere, Log, TEXT("atmosphere: %d stars, %d zodiac segments, %d Milky Way blobs."),
		StarIsm->GetInstanceCount(), ZodiacIsm->GetInstanceCount(), MilkyIsm->GetInstanceCount());
}

void ASimAtmosphere::ApplySky(const FSimAtmosphere& State, int32 Day, float Hour, FVector Camera)
{
	if (SkyRoot == nullptr)
	{
		return;
	}
	const float Night = NightFactor(Hour, State);
	const bool bShow = Night > 0.005f;
	StarIsm->SetVisibility(bShow);
	MilkyIsm->SetVisibility(bShow);
	ZodiacIsm->SetVisibility(bShow && CVarZodiac.GetValueOnGameThread() != 0);
	if (!bShow)
	{
		return;
	}
	StarMat->SetScalarParameterValue(TEXT("Night"), Night * kStarGain);
	// tools/art/star_dome.py bakes the sky at sidereal angle 0; the turn about the pole is +angle in UE's mirror
	// of the real sky's frame (SIDEREAL_SIGN, proved by tools/tests/test_art_stars.py).
	const FQuat Turn(CelestialPole(), FMath::DegreesToRadians(SiderealAngle(Day, Hour)));
	SkyRoot->SetWorldLocationAndRotation(Camera, Turn);
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
	FVector Cam = FVector::ZeroVector;
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager != nullptr)
		{
			Cam = PC->PlayerCameraManager->GetCameraLocation();
		}
	}
	ApplyFx(Live, Hour, Cam);
	ApplySky(Live, static_cast<int32>(FMath::Max<int64>(0, USimWorldSubsystem::GetSimDayFor(World))), Hour, Cam);
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
		UKismetMaterialLibrary::SetScalarParameterValue(World, WorldParams, TEXT("Rain"), Live.Rain);
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
