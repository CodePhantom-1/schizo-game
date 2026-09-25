// SimSettingsTest.cpp — part 3 Task 4: settings clamp and apply.
#include "Misc/AutomationTest.h"
#include "SimGameUserSettings.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimSettingsClampAndApply, "Sim.Settings.ClampAndApply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSimSettingsClampAndApply::RunTest(const FString&)
{
	USimGameUserSettings* S = NewObject<USimGameUserSettings>();
	S->DayLengthMinutes = 0.f;
	S->NeedsSeverityPercent = 900;
	S->SubtitleScale = 9.f;
	S->ColourVision = 7;
	S->MouseSensitivity = -1.f;
	S->Clamp();
	TestEqual(TEXT("day length"), S->DayLengthMinutes, 5.f);
	TestEqual(TEXT("severity"), S->NeedsSeverityPercent, 300);
	TestEqual(TEXT("text scale"), S->SubtitleScale, 2.f);
	TestEqual(TEXT("colour vision"), int32(S->ColourVision), 3);
	TestEqual(TEXT("mouse"), S->MouseSensitivity, 0.2f);

	IConsoleVariable* DayLength = IConsoleManager::Get().FindConsoleVariable(TEXT("sim.DaysPerRealMinute"));
	if (TestNotNull(TEXT("the day-length cvar exists"), DayLength))
	{
		const float Before = DayLength->GetFloat();
		S->ColourVision = 0;
		S->DayLengthMinutes = 30.f;
		S->ApplySimSettings(nullptr);
		TestEqual(TEXT("applied to the clock"), DayLength->GetFloat(), 30.f);
		DayLength->Set(Before, ECVF_SetByGameSetting);
	}
	return true;
}

#endif
