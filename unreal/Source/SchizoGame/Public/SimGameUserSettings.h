// SimGameUserSettings.h — A11: the player's settings, persisted per user
// (GameUserSettings.ini), clamped on load, applied to their systems.
// Each field names the system that reads it. Fields whose system is not
// built yet are stored now and read by that stage (completion-plan A11).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "SimGameUserSettings.generated.h"

UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class SCHIZOGAME_API USimGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	// --- gameplay -------------------------------------------------------------
	/** Real minutes per sim day, 5..240 (D-024 §2) -> sim.DaysPerRealMinute (now). */
	UPROPERTY(config) float DayLengthMinutes = 48.f;
	/** Needs climb, percent of the designed rate, 25..300 -> kernel (now). */
	UPROPERTY(config) int32 NeedsSeverityPercent = 100;
	/** Chronicle mode: rites' effects statistical and deniable -> Stage J7. */
	UPROPERTY(config) bool bChronicleMode = false;
	/** Quest markers -> Stage P8. */
	UPROPERTY(config) bool bGuidedMode = false;
	/** Fast travel (off by default, city-life §10) -> Stage Q6. */
	UPROPERTY(config) bool bFastTravel = false;
	/** Illness on/off -> Stage B3. */
	UPROPERTY(config) bool bDisease = true;
	/** Historical difficulty: death is final -> Stages B10/P12. */
	UPROPERTY(config) bool bHistoricalPermadeath = false;

	// --- accessibility --------------------------------------------------------
	/** The player's voice on/off (notes L1) -> Stage R6. */
	UPROPERTY(config) bool bPlayerVoice = true;
	/** Subtitles -> Stages P4/R5. */
	UPROPERTY(config) bool bSubtitles = true;
	/** Text size, 0.75..2 -> every screen's text (now). */
	UPROPERTY(config) float SubtitleScale = 1.f;
	/** 0 off, 1 protanope, 2 deuteranope, 3 tritanope -> Slate renderer (now). */
	UPROPERTY(config) uint8 ColourVision = 0;

	// --- audio (0..1) -> Stage R1 -------------------------------------------------
	UPROPERTY(config) float MasterVolume = 1.f;
	UPROPERTY(config) float MusicVolume = 0.8f;
	UPROPERTY(config) float EffectsVolume = 1.f;
	UPROPERTY(config) float VoiceVolume = 1.f;

	// --- controls -------------------------------------------------------------
	/** Mouse/stick look speed, 0.2..3 -> ASimCharacter (now). */
	UPROPERTY(config) float MouseSensitivity = 1.f;
	/** Invert the look's Y axis -> ASimCharacter (now). */
	UPROPERTY(config) bool bInvertY = false;

	/** Puts every field back in range (a hand-edited ini can hold anything). */
	void Clamp();
	/** Pushes the settings that have a system today (day length, needs severity, colour vision). */
	void ApplySimSettings(const UObject* WorldContext);

	virtual void LoadSettings(bool bForceReload = false) override;

	/** The engine's settings object, as ours (nullptr before the engine is up). */
	static USimGameUserSettings* Get();
};
