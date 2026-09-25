// SimMoonTest.cpp — the moon's place follows the kernel's month, smoothly.
#include "Misc/AutomationTest.h"
#include "SimDayNight.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimMoonOrbitTest, "Sim.Sky.MoonOrbit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSimMoonOrbitTest::RunTest(const FString& Parameters)
{
	// Conjunction at the new crescent, opposition at full (the kernel's day 15).
	TestEqual(TEXT("new crescent"), ASimDayNight::MoonOrbitAngle(1, 0.f), 0.f, 1e-4f);
	TestEqual(TEXT("full on day 15"), ASimDayNight::MoonOrbitAngle(15, 0.f), PI, 1e-4f);
	// No jump at any midnight: the last minute of a day meets the first of the next.
	for (int32 Dom = 1; Dom < 30; ++Dom)
	{
		const float Before = ASimDayNight::MoonOrbitAngle(Dom, 23.99f);
		const float After = ASimDayNight::MoonOrbitAngle(Dom + 1, 0.f);
		TestTrue(FString::Printf(TEXT("smooth into day %d"), Dom + 1), FMath::Abs(After - Before) < 0.01f);
	}
	// The month wraps: the end of day 30 meets day 1 again (a full turn).
	TestTrue(TEXT("wraps at month end"), FMath::Abs(ASimDayNight::MoonOrbitAngle(30, 23.99f) - 2.f * PI) < 0.01f);
	return true;
}

#endif
