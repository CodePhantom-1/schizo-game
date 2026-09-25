// SimGameUserSettings.cpp — A11: see the header.
#include "SimGameUserSettings.h"

#include "SimWorldSubsystem.h"
#include "sim/CApi.h"
#include "sim/CApiVerbs.h"

#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/SlateRenderer.h"

void USimGameUserSettings::Clamp()
{
	DayLengthMinutes = FMath::Clamp(DayLengthMinutes, 5.f, 240.f);
	NeedsSeverityPercent = FMath::Clamp(NeedsSeverityPercent, 25, 300);
	SubtitleScale = FMath::Clamp(SubtitleScale, 0.75f, 2.f);
	ColourVision = FMath::Min<uint8>(ColourVision, 3);
	MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.f, 1.f);
	EffectsVolume = FMath::Clamp(EffectsVolume, 0.f, 1.f);
	VoiceVolume = FMath::Clamp(VoiceVolume, 0.f, 1.f);
	MouseSensitivity = FMath::Clamp(MouseSensitivity, 0.2f, 3.f);
}

void USimGameUserSettings::LoadSettings(bool bForceReload)
{
	Super::LoadSettings(bForceReload);
	Clamp();
}

void USimGameUserSettings::ApplySimSettings(const UObject* WorldContext)
{
	Clamp();
	if (IConsoleVariable* DayLength = IConsoleManager::Get().FindConsoleVariable(TEXT("sim.DaysPerRealMinute")))
	{
		DayLength->Set(DayLengthMinutes, ECVF_SetByGameSetting);
	}
	if (SimWorld* Handle = WorldContext ? USimWorldSubsystem::GetSimHandleFor(WorldContext) : nullptr)
	{
		sim_world_set_needs_severity(Handle, NeedsSeverityPercent);
	}
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer() != nullptr)
	{
		const EColorVisionDeficiency Types[] = {EColorVisionDeficiency::NormalVision, EColorVisionDeficiency::Protanope,
			EColorVisionDeficiency::Deuteranope, EColorVisionDeficiency::Tritanope};
		FSlateApplication::Get().GetRenderer()->SetColorVisionDeficiencyType(Types[ColourVision], ColourVision ? 10 : 0, ColourVision != 0, false);
	}
}

USimGameUserSettings* USimGameUserSettings::Get()
{
	return GEngine ? Cast<USimGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}
