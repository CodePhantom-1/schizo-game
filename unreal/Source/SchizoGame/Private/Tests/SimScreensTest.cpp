// SimScreensTest.cpp — part 3 Task 5: every screen builds a real widget, with no world.
#include "Misc/AutomationTest.h"
#include "UI/SimShellSubsystem.h"
#include "../UI/SimScreens.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimScreensBuild, "Sim.Ui.ScreensBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSimScreensBuild::RunTest(const FString&)
{
	ULocalPlayer* LP = NewObject<ULocalPlayer>(GEngine);  // local players live within the engine; the shell within one
	USimShellSubsystem* Shell = NewObject<USimShellSubsystem>(LP);
	for (ESimScreen S : {ESimScreen::MainMenu, ESimScreen::Pause, ESimScreen::SaveLoad, ESimScreen::Settings,
		ESimScreen::Journal, ESimScreen::Tablet, ESimScreen::Loading})
	{
		TSharedRef<SWidget> W = MakeScreen(S, *Shell);
		TestTrue(FString::Printf(TEXT("%s is a real widget tree"), USimShellSubsystem::ScreenName(S)),
			W->GetChildren()->Num() > 0);
	}
	return true;
}

#endif
