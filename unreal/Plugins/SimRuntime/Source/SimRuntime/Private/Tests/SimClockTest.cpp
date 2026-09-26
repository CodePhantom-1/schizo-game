// SimClockTest.cpp — A4: the sim day defaults to 48 real minutes (D-024 §2).
#include "Misc/AutomationTest.h"
#include "SimWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimClockDefaultTest, "Sim.Clock.DefaultDayIs48Minutes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSimClockDefaultTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("real minutes per sim day"), GetDefault<USimWorldSubsystem>()->SimDaysPerRealMinute, 48.0f);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimClockRescale, "Sim.Clock.RescaleOnDayLengthChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSimClockRescale::RunTest(const FString&)
{
	// 18:00 on a 240-minute day, then the player picks a 5-minute day.
	double Secs = 240.0 * 60.0 * 18.0 / 24.0;
	double Last = 240.0 * 60.0;
	int32 Days = USimWorldSubsystem::StepClock(Secs, Last, 5.0 * 60.0, 0.0);
	TestEqual(TEXT("no days jump"), Days, 0);
	TestEqual(TEXT("still 18:00"), Secs / (5.0 * 60.0) * 24.0, 18.0, 1e-6);
	TestEqual(TEXT("remembers the new length"), Last, 300.0);
	// And back to a long day: no hour lost.
	Days = USimWorldSubsystem::StepClock(Secs, Last, 240.0 * 60.0, 0.0);
	TestEqual(TEXT("no days"), Days, 0);
	TestEqual(TEXT("still 18:00 on the long day"), Secs / (240.0 * 60.0) * 24.0, 18.0, 1e-6);
	// Ordinary time still turns the day.
	Days = USimWorldSubsystem::StepClock(Secs, Last, 240.0 * 60.0, 240.0 * 60.0 * 7.0 / 24.0);
	TestEqual(TEXT("one midnight crossed"), Days, 1);
	TestEqual(TEXT("01:00 next day"), Secs / (240.0 * 60.0) * 24.0, 1.0, 1e-6);
	return true;
}
#endif
