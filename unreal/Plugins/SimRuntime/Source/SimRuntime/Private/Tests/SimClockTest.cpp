// SimClockTest.cpp — A4: the sim day defaults to 48 real minutes (D-024 §2).
#include "Misc/AutomationTest.h"
#include "SimWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimClockDefaultTest, "Sim.Clock.DefaultDayIs48Minutes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSimClockDefaultTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("real minutes per sim day"), GetDefault<USimWorldSubsystem>()->SimDaysPerRealMinute, 48.0f);
	return true;
}

#endif
