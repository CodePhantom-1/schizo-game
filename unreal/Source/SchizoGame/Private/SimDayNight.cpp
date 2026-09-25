// SimDayNight.cpp — see SimDayNight.h. INVENTED sky model (no canon
// source): the sun rides a tilted circle (rises east over the sea, peaks
// 62 degrees up in the south at noon, sets west), the moon the opposite one.
#include "SimDayNight.h"

#include "SimWorldSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimDayNight, Log, All);

namespace
{
	constexpr float kSunLux = 10.f;
	constexpr float kMoonLux = 0.4f;
	constexpr float kSunTiltDeg = 62.f;
	constexpr float kMoonTiltDeg = 55.f;
	// EV floor at night: higher = darker nights (the eye cannot adapt below it).
	constexpr float kNightExposureFloor = 2.0f;

	/** Unit vector toward a body on the tilted day circle; Phase 0 = rising in the east. */
	FVector BodyDir(float Phase, float TiltDeg)
	{
		const float T = FMath::DegreesToRadians(TiltDeg);
		return FVector(FMath::Cos(Phase), -FMath::Sin(Phase) * FMath::Cos(T), FMath::Sin(Phase) * FMath::Sin(T));
	}

	FLinearColor LerpC(const FLinearColor& A, const FLinearColor& B, float T)
	{
		return A + (B - A) * T;
	}
}

ASimDayNight::ASimDayNight()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ASimDayNight::BeginPlay()
{
	Super::BeginPlay();
	FindOrSpawnSky();
	ApplyHour(6.f);
}

bool ASimDayNight::IsSimNightHour(const float Hour)
{
	// INVENTED boundary (canon gives no day/night flag): dawn is the
	// gatekeeper's gates_open_at_dawn row (hour 6) and dusk sits just after
	// gates_close_at_dusk (hour 18) — the hours between are "day" for light.
	return Hour < 6.f || Hour >= 18.f;
}

void ASimDayNight::FindOrSpawnSky()
{
	auto Make = [this](auto* Comp)
	{
		Comp->SetupAttachment(RootComponent);
		Comp->SetMobility(EComponentMobility::Movable);
		return Comp;
	};

	// --- the physical sky ---------------------------------------------------
	Atmosphere = Make(NewObject<USkyAtmosphereComponent>(this, TEXT("Atmosphere")));
	Atmosphere->RegisterComponent();
	// A dusty, drought-thick lower sky: more aerosol (Mie) than the default.
	Atmosphere->SetMieScatteringScale(0.012f);
	Atmosphere->SetMieAnisotropy(0.82f);
	Atmosphere->SetMieExponentialDistribution(2.2f);
	// Strong aerial perspective: far ranges fade to blue-grey silhouettes.
	Atmosphere->SetAerialPespectiveViewDistanceScale(7.f);
	Atmosphere->SetHeightFogContribution(1.f);

	// --- sun and moon ---------------------------------------------------------
	Sun = Make(NewObject<UDirectionalLightComponent>(this, TEXT("Sun")));
	Sun->SetAtmosphereSunLight(true);
	Sun->SetAtmosphereSunLightIndex(0);
	Sun->SetForwardShadingPriority(2);  // priorities clamp at 0: sun 2 > moon 1 > fill 0, never a tie
	Sun->SetIntensity(kSunLux);
	Sun->SetLightSourceAngle(0.9f);
	Sun->DynamicShadowDistanceMovableLight = 30000.f;
	Sun->bCastCloudShadows = true;
	Sun->SetVolumetricScatteringIntensity(1.4f);
	Sun->RegisterComponent();
	Sun->ComponentTags.Add(FName("SimSun"));

	Moon = Make(NewObject<UDirectionalLightComponent>(this, TEXT("Moon")));
	Moon->SetAtmosphereSunLight(true);
	Moon->SetAtmosphereSunLightIndex(1);
	Moon->SetForwardShadingPriority(1);
	Moon->SetIntensity(kMoonLux);
	Moon->SetLightColor(FLinearColor(0.62f, 0.74f, 1.f));
	Moon->SetLightSourceAngle(2.2f);
	Moon->DynamicShadowDistanceMovableLight = 15000.f;
	Moon->SetVolumetricScatteringIntensity(0.8f);
	Moon->RegisterComponent();

#if PLATFORM_LINUX
	// RADV/AMD: no captured sky light, no clouds (both hang the GPU there).
	// The shadowless fill from overhead stands in for sky bounce.
	SkyFill = Make(NewObject<UDirectionalLightComponent>(this, TEXT("SkyFill")));
	SkyFill->SetCastShadows(false);
	SkyFill->SetForwardShadingPriority(0);
	SkyFill->SetIntensity(1.6f);
	SkyFill->RegisterComponent();
	SkyFill->SetWorldRotation(FRotator(-65.f, 35.f, 0.f));
#else
	// --- ambient: the sky itself, captured every frame -----------------------
	Fill = NewObject<USkyLightComponent>(this, TEXT("SkyLight"));
	Fill->SetupAttachment(RootComponent);
	Fill->SetMobility(EComponentMobility::Movable);
	Fill->SourceType = ESkyLightSourceType::SLS_CapturedScene;
	Fill->bRealTimeCapture = true;
	Fill->bLowerHemisphereIsBlack = false;
	Fill->SetLowerHemisphereColor(FLinearColor(0.09f, 0.065f, 0.045f));
	Fill->SetIntensity(1.0f);
	Fill->RegisterComponent();

	// --- clouds: sparse fair-weather clouds over a land in drought ----------
	Clouds = NewObject<UVolumetricCloudComponent>(this, TEXT("Clouds"));
	Clouds->SetupAttachment(RootComponent);
	Clouds->SetLayerBottomAltitude(4.5f);
	Clouds->SetLayerHeight(6.f);
	if (UMaterialInterface* CloudMat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst")))
	{
		Clouds->SetMaterial(CloudMat);
	}
	Clouds->RegisterComponent();
#endif

	// --- the drought haze: warm dust, god rays through it ----------------------
	Haze = NewObject<UExponentialHeightFogComponent>(this, TEXT("Haze"));
	Haze->SetupAttachment(RootComponent);
	Haze->SetFogDensity(0.022f);
	Haze->SetFogHeightFalloff(0.09f);
	Haze->SetFogMaxOpacity(0.97f);
	Haze->SetStartDistance(600.f);
	Haze->SetDirectionalInscatteringExponent(7.f);
	Haze->SetVolumetricFog(true);
	Haze->SetVolumetricFogScatteringDistribution(0.72f);
	Haze->SetVolumetricFogAlbedo(FColor(255, 236, 206));
	Haze->SetVolumetricFogExtinctionScale(0.9f);
	Haze->SetVolumetricFogDistance(9000.f);
	Haze->RegisterComponent();
	Haze->SetWorldLocation(FVector(0.f, 0.f, -300.f));

	// --- the grade: warm highlights, cool shadows; bloom, vignette, grain -----
	Grade = NewObject<UPostProcessComponent>(this, TEXT("Grade"));
	Grade->SetupAttachment(RootComponent);
	Grade->bUnbound = true;
	FPostProcessSettings& P = Grade->Settings;
	P.bOverride_AutoExposureMethod = true;
	P.AutoExposureMethod = AEM_Histogram;
	P.bOverride_AutoExposureMinBrightness = true;
	P.AutoExposureMinBrightness = -0.6f;  // nights stay dark (moonlit), not lifted to dusk
	P.bOverride_AutoExposureMaxBrightness = true;
	P.AutoExposureMaxBrightness = 3.f;
	P.bOverride_AutoExposureBias = true;
	P.AutoExposureBias = 0.35f;
	P.bOverride_AutoExposureSpeedUp = true;
	P.AutoExposureSpeedUp = 2.5f;
	P.bOverride_AutoExposureSpeedDown = true;
	P.AutoExposureSpeedDown = 1.5f;
	P.bOverride_BloomIntensity = true;
	P.BloomIntensity = 0.75f;
	P.bOverride_VignetteIntensity = true;
	P.VignetteIntensity = 0.5f;
	P.bOverride_FilmGrainIntensity = true;
	P.FilmGrainIntensity = 0.16f;
	P.bOverride_ColorSaturation = true;
	P.ColorSaturation = FVector4(1.f, 1.f, 1.f, 1.02f);
	P.bOverride_ColorContrast = true;
	P.ColorContrast = FVector4(1.08f, 1.08f, 1.08f, 1.06f);
	P.bOverride_ColorGainHighlights = true;
	P.ColorGainHighlights = FVector4(1.02f, 1.0f, 0.96f, 1.f);
	P.bOverride_ColorOffsetShadows = true;
	P.ColorOffsetShadows = FVector4(-0.004f, 0.002f, 0.012f, 0.f);
	P.bOverride_AmbientOcclusionIntensity = true;
	P.AmbientOcclusionIntensity = 0.7f;
#if !PLATFORM_LINUX
	P.bOverride_DynamicGlobalIlluminationMethod = true;
	P.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
	P.bOverride_ReflectionMethod = true;
	P.ReflectionMethod = EReflectionMethod::Lumen;
#endif
	Grade->RegisterComponent();
}

void ASimDayNight::ApplyHour(float Hour)
{
	const float SunPhase = (Hour - 6.f) / 12.f * PI;
	const FVector S = BodyDir(SunPhase, kSunTiltDeg);
	// The moon's place in the sky follows its phase (A14): with the sun at the
	// new crescent (day 1), opposite it at full (day 15). Illumination scales
	// its light; a dark-moon night stays dark (real darkness, city-life §8).
	int32 Year = 1, Month = 1, Dom = 15;
	USimWorldSubsystem::GetSimDateFor(this, Year, Month, Dom);
	const float MoonOffset = 2.f * PI * static_cast<float>(Dom - 1) / 28.f;
	const int32 Illum = USimWorldSubsystem::GetSimMoonIlluminationFor(this);
	const float MoonLit = Illum < 0 ? 1.f : FMath::Max(0.05f, Illum / 100.f);
	const FVector M = BodyDir(SunPhase + MoonOffset, kMoonTiltDeg);

	const float Sz = static_cast<float>(S.Z), Mz = static_cast<float>(M.Z);
	const float SunUp = FMath::SmoothStep(-0.06f, 0.1f, Sz);
	const float MoonUp = FMath::SmoothStep(-0.05f, 0.15f, Mz) * (1.f - SunUp);

	Sun->SetWorldRotation((-S).Rotation());
	Sun->SetIntensity(kSunLux * SunUp);
	Sun->SetCastShadows(SunUp > 0.01f);
	Moon->SetWorldRotation((-M).Rotation());
	Moon->SetIntensity(kMoonLux * MoonUp * MoonLit);
	Moon->SetCastShadows(MoonUp > 0.2f);

	// Haze: warm dust by day, a golden glow at the horizon hours, deep blue by night.
	const float Day = FMath::SmoothStep(-0.18f, 0.3f, Sz);
	const float Golden = FMath::SmoothStep(-0.1f, 0.05f, Sz) * (1.f - FMath::SmoothStep(0.1f, 0.35f, Sz));
	FLinearColor FogC = LerpC(FLinearColor(0.010f, 0.016f, 0.034f), FLinearColor(0.34f, 0.29f, 0.22f), Day);
	FogC = LerpC(FogC, FLinearColor(0.42f, 0.24f, 0.12f), Golden * 0.7f);
	Haze->SetFogInscatteringColor(FogC);
	Haze->SetDirectionalInscatteringColor(LerpC(FLinearColor(0.02f, 0.03f, 0.06f), FLinearColor(0.9f, 0.55f, 0.28f), Day));
	if (Fill != nullptr)
	{
		Fill->SetIntensity(FMath::Lerp(1.6f, 1.0f, Day));
	}
	if (SkyFill != nullptr)
	{
		SkyFill->SetIntensity(FMath::Lerp(0.08f, 1.6f, Day));
	}
	// Real darkness (city-life §8): by night the eye may not adapt all the way
	// up — the exposure floor rises so a moonlit street reads as night, not
	// as a dim noon. By day Tommy's range stands (-0.6 .. 3).
	if (Grade != nullptr)
	{
		Grade->Settings.AutoExposureMinBrightness = FMath::Lerp(kNightExposureFloor, -0.6f, Day);
	}

	if (FMath::FloorToInt(Hour) != FMath::FloorToInt(LastLoggedHour))
	{
		LastLoggedHour = Hour;
		UE_LOG(LogSimDayNight, Log, TEXT("Sky: hour %.2f, sun elevation %.0f deg, moon %.0f deg."), Hour,
			FMath::RadiansToDegrees(FMath::Asin(S.Z)), FMath::RadiansToDegrees(FMath::Asin(M.Z)));
	}
}

void ASimDayNight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Hour = USimWorldSubsystem::GetSimHourFor(this);
	if (Hour < 0.f)
	{
		return;  // the kernel world does not exist yet — nothing to reflect
	}
	ApplyHour(Hour);
}
