// SimUiReviewTest.cpp — part 3 review fixes: settings discard on any close; first-button focus.
#include "Misc/AutomationTest.h"
#include "SimGameUserSettings.h"
#include "UI/SimShellSubsystem.h"
#include "../UI/SimScreens.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Widgets/Input/SButton.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimSettingsDiscard, "Sim.Ui.SettingsDiscardOnClose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSimSettingsDiscard::RunTest(const FString&)
{
	USimGameUserSettings* S = USimGameUserSettings::Get();
	if (!TestNotNull(TEXT("engine settings object"), S)) return false;
	USimShellSubsystem* Shell = NewObject<USimShellSubsystem>(NewObject<ULocalPlayer>(GEngine));
	const float Before = S->SubtitleScale;
	Shell->Open(ESimScreen::Pause);
	Shell->Open(ESimScreen::Settings);
	S->SubtitleScale = Before > 1.5f ? 1.f : 1.9f;  // an edit never applied
	Shell->CloseTop();                              // Esc, not Back
	TestEqual(TEXT("unapplied edit discarded"), S->SubtitleScale, Before);
	Shell->Open(ESimScreen::Settings);
	S->SubtitleScale = Before > 1.5f ? 1.f : 1.9f;
	Shell->CloseAll();                              // Main Menu / Continue path
	TestEqual(TEXT("discarded by CloseAll too"), S->SubtitleScale, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFirstFocusable, "Sim.Ui.FirstFocusable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSimFirstFocusable::RunTest(const FString&)
{
	USimShellSubsystem* Shell = NewObject<USimShellSubsystem>(NewObject<ULocalPlayer>(GEngine));
	for (ESimScreen Screen : {ESimScreen::MainMenu, ESimScreen::Pause, ESimScreen::SaveLoad, ESimScreen::Settings,
		ESimScreen::Journal, ESimScreen::Tablet})
	{
		TSharedPtr<SWidget> First = USimShellSubsystem::FindFirstFocusable(MakeScreen(Screen, *Shell));
		TestTrue(FString::Printf(TEXT("%s has a focusable first control"), USimShellSubsystem::ScreenName(Screen)),
			First.IsValid() && First->SupportsKeyboardFocus());
	}
	return true;
}

#endif
