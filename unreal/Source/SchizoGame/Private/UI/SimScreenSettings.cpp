// SimScreenSettings.cpp — A11: the Settings screen. Edits USimGameUserSettings
// in place; Apply clamps, applies and saves; Back discards by reloading the
// saved config. Tabs: Gameplay, Graphics, Audio, Controls, Accessibility.
#include "UI/SimScreens.h"

#include "SimGameUserSettings.h"
#include "UI/SimShellSubsystem.h"
#include "UI/SimUiStyle.h"
#include "UI/SimUiWidgets.h"

#include "Engine/LocalPlayer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SimUi"

namespace
{
	FText Percent(float V) { return FText::AsPercent(V); }
	FText Minutes(float V) { return FText::Format(LOCTEXT("MinutesPerDay", "{0} min per day"), FText::AsNumber(FMath::RoundToInt(V))); }
	FText Times(float V) { return FText::FromString(FString::Printf(TEXT("×%.2f"), V)); }

	TSharedRef<SWidget> Gameplay(USimGameUserSettings* S)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Slider(LOCTEXT("DayLength", "Length of a day"), 5.f, 240.f, 1.f,
				[S] { return S->DayLengthMinutes; }, [S](float V) { S->DayLengthMinutes = V; }, Minutes)]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Slider(LOCTEXT("NeedsSeverity", "Hunger, thirst and tiredness"), 25.f, 300.f, 5.f,
				[S] { return float(S->NeedsSeverityPercent); }, [S](float V) { S->NeedsSeverityPercent = FMath::RoundToInt(V); },
				[](float V) { return Percent(V / 100.f); })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Choice(LOCTEXT("Magic", "The gods"),
				{LOCTEXT("Mythic", "Mythic: rites work"), LOCTEXT("Chronicle", "Chronicle: rites deniable")},
				[S] { return S->bChronicleMode ? 1 : 0; }, [S](int32 I) { S->bChronicleMode = I == 1; })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Check(LOCTEXT("Guided", "Quest markers (guided mode)"),
				[S] { return S->bGuidedMode; }, [S](bool B) { S->bGuidedMode = B; })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Check(LOCTEXT("FastTravel", "Fast travel"),
				[S] { return S->bFastTravel; }, [S](bool B) { S->bFastTravel = B; })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Check(LOCTEXT("Disease", "Illness"),
				[S] { return S->bDisease; }, [S](bool B) { S->bDisease = B; })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Check(LOCTEXT("Permadeath", "Historical: death is final"),
				[S] { return S->bHistoricalPermadeath; }, [S](bool B) { S->bHistoricalPermadeath = B; })];
	}

	TSharedRef<SWidget> Graphics(USimGameUserSettings* S)
	{
		static const FIntPoint Resolutions[] = {{1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
		TArray<FText> ResNames;
		for (const FIntPoint& R : Resolutions)
		{
			ResNames.Add(FText::FromString(FString::Printf(TEXT("%d × %d"), R.X, R.Y)));
		}
		static const float Limits[] = {30.f, 60.f, 120.f, 0.f};
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Choice(LOCTEXT("Resolution", "Resolution"), ResNames,
				[S] {
					const FIntPoint Cur = S->GetScreenResolution();
					for (int32 I = 0; I < UE_ARRAY_COUNT(Resolutions); ++I) if (Resolutions[I] == Cur) return I;
					return 0;
				},
				[S](int32 I) { S->SetScreenResolution(Resolutions[I]); })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Choice(LOCTEXT("WindowMode", "Window"),
				{LOCTEXT("Fullscreen", "Fullscreen"), LOCTEXT("Borderless", "Borderless"), LOCTEXT("Windowed", "Windowed")},
				[S] { return int32(S->GetFullscreenMode()); }, [S](int32 I) { S->SetFullscreenMode(EWindowMode::ConvertIntToWindowMode(I)); })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Choice(LOCTEXT("Quality", "Quality"),
				{LOCTEXT("Low", "Low"), LOCTEXT("Medium", "Medium"), LOCTEXT("High", "High"), LOCTEXT("Epic", "Epic")},
				[S] { return FMath::Clamp(S->GetOverallScalabilityLevel(), 0, 3); }, [S](int32 I) { S->SetOverallScalabilityLevel(I); })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Choice(LOCTEXT("FrameLimit", "Frame limit"),
				{FText::AsNumber(30), FText::AsNumber(60), FText::AsNumber(120), LOCTEXT("Unlimited", "Unlimited")},
				[S] {
					for (int32 I = 0; I < UE_ARRAY_COUNT(Limits); ++I) if (FMath::IsNearlyEqual(S->GetFrameRateLimit(), Limits[I])) return I;
					return 3;
				},
				[S](int32 I) { S->SetFrameRateLimit(Limits[I]); })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Check(LOCTEXT("VSync", "Vertical sync"),
				[S] { return S->IsVSyncEnabled(); }, [S](bool B) { S->SetVSyncEnabled(B); })];
	}

	TSharedRef<SWidget> AudioPage(USimGameUserSettings* S)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Slider(LOCTEXT("Master", "Master"), 0.f, 1.f, 0.05f, [S] { return S->MasterVolume; }, [S](float V) { S->MasterVolume = V; }, Percent)]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Slider(LOCTEXT("Music", "Music"), 0.f, 1.f, 0.05f, [S] { return S->MusicVolume; }, [S](float V) { S->MusicVolume = V; }, Percent)]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Slider(LOCTEXT("Effects", "Sounds of the world"), 0.f, 1.f, 0.05f, [S] { return S->EffectsVolume; }, [S](float V) { S->EffectsVolume = V; }, Percent)]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Slider(LOCTEXT("Voices", "Voices"), 0.f, 1.f, 0.05f, [S] { return S->VoiceVolume; }, [S](float V) { S->VoiceVolume = V; }, Percent)];
	}

	TSharedRef<SWidget> Controls(USimGameUserSettings* S)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Slider(LOCTEXT("Sensitivity", "Look sensitivity"), 0.2f, 3.f, 0.05f,
				[S] { return S->MouseSensitivity; }, [S](float V) { S->MouseSensitivity = V; }, Times)]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Check(LOCTEXT("InvertY", "Invert look up/down"),
				[S] { return S->bInvertY; }, [S](bool B) { S->bInvertY = B; })]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[MakeRebindList()];
	}

	TSharedRef<SWidget> Accessibility(USimGameUserSettings* S)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Check(LOCTEXT("Subtitles", "Subtitles"), [S] { return S->bSubtitles; }, [S](bool B) { S->bSubtitles = B; })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Slider(LOCTEXT("TextSize", "Text size"), 0.75f, 2.f, 0.05f,
				[S] { return S->SubtitleScale; }, [S](float V) { S->SubtitleScale = V; }, Times)]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Choice(LOCTEXT("ColourVision", "Colour vision"),
				{LOCTEXT("CvOff", "Standard"), LOCTEXT("Protan", "Protanopia"), LOCTEXT("Deutan", "Deuteranopia"), LOCTEXT("Tritan", "Tritanopia")},
				[S] { return int32(S->ColourVision); }, [S](int32 I) { S->ColourVision = uint8(I); })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Check(LOCTEXT("PlayerVoice", "The player's voice"),
				[S] { return S->bPlayerVoice; }, [S](bool B) { S->bPlayerVoice = B; })];
	}
}

TSharedRef<SWidget> MakeSettingsScreen(USimShellSubsystem& Shell)
{
	TWeakObjectPtr<USimShellSubsystem> W(&Shell);
	USimGameUserSettings* S = USimGameUserSettings::Get();
	if (S == nullptr)
	{
		S = GetMutableDefault<USimGameUserSettings>();  // no engine settings object (tests): edit the defaults
	}
	TSharedRef<SWidgetSwitcher> Pages = SNew(SWidgetSwitcher)
		+ SWidgetSwitcher::Slot()[Gameplay(S)]
		+ SWidgetSwitcher::Slot()[Graphics(S)]
		+ SWidgetSwitcher::Slot()[AudioPage(S)]
		+ SWidgetSwitcher::Slot()[Controls(S)]
		+ SWidgetSwitcher::Slot()[Accessibility(S)];
	const FText Tabs[] = {LOCTEXT("TabGameplay", "Gameplay"), LOCTEXT("TabGraphics", "Graphics"), LOCTEXT("TabAudio", "Audio"),
		LOCTEXT("TabControls", "Controls"), LOCTEXT("TabAccess", "Accessibility")};
	TSharedRef<SHorizontalBox> TabRow = SNew(SHorizontalBox);
	for (int32 I = 0; I < UE_ARRAY_COUNT(Tabs); ++I)
	{
		TabRow->AddSlot().AutoWidth()[SimUi::SmallButton(Tabs[I], [Pages, I]() { Pages->SetActiveWidgetIndex(I); })];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[SimUi::Title(LOCTEXT("SettingsTitle", "Settings"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[TabRow]
		+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SBox).MinDesiredWidth(760.f).MinDesiredHeight(360.f)[SNew(SScrollBox) + SScrollBox::Slot()[Pages]]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[SimUi::SmallButton(LOCTEXT("Apply", "Apply"), [W, S]()
				{
					S->Clamp();
					S->ApplySettings(false);
					S->ApplySimSettings(W.IsValid() ? W->GetLocalPlayer() : nullptr);
					S->SaveSettings();
					if (W.IsValid())
					{
						W->Toast(LOCTEXT("Applied", "Settings applied."));
						W->Refresh();  // text size may have changed
					}
				})]
			+ SHorizontalBox::Slot().AutoWidth()[SimUi::SmallButton(LOCTEXT("Back", "Back"), [W, S]()
				{
					S->LoadSettings(true);  // discard what was not applied
					if (W.IsValid()) W->CloseTop();
				})]
		];
}

#undef LOCTEXT_NAMESPACE
