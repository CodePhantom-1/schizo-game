// SimCalendarTest.cpp — A14: the calendar reaches the engine intact.
#include "Misc/AutomationTest.h"
#include "SimGameInstanceSubsystem.h"
#include "sim/CApi.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCalendarTest, "Sim.Calendar.MoonAndMonth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSimCalendarTest::RunTest(const FString& Parameters)
{
	const FString Canon = USimGameInstanceSubsystem::CanonDir();
	SimWorld* W = sim_world_create(TCHAR_TO_UTF8(*Canon), 42);
	if (!TestNotNull(TEXT("world from the staged canon"), W))
	{
		return false;
	}
	char Buf[128] = {};
	sim_world_month_name(W, Buf, sizeof(Buf));
	TestEqual(TEXT("month 1"), FString(UTF8_TO_TCHAR(Buf)), FString(TEXT("Rains-Coming")));
	sim_world_advance_days(W, 14);
	sim_world_moon_phase(W, Buf, sizeof(Buf));
	TestEqual(TEXT("day 15 is full"), FString(UTF8_TO_TCHAR(Buf)), FString(TEXT("full")));
	TestEqual(TEXT("full = 100"), sim_world_moon_illumination(W), 100);
	sim_world_destroy(W);
	return true;
}

#endif
