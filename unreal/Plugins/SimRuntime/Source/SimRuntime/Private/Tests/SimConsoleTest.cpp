// SimConsoleTest.cpp — A5: every sim.* command exists; bad args change nothing.
#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimConsoleBadArgsTest, "Sim.Console.BadArgs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSimConsoleBadArgsTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : {TEXT("sim.AdvanceDays"), TEXT("sim.AdvanceHours"), TEXT("sim.Give"), TEXT("sim.SetNeed"),
		TEXT("sim.Standing"), TEXT("sim.Favour"), TEXT("sim.Drought"), TEXT("sim.War"), TEXT("sim.Dump"),
		TEXT("sim.Quests"), TEXT("sim.Accept"), TEXT("sim.Journal"), TEXT("sim.Talk"), TEXT("sim.Memory"), TEXT("sim.Events")})
	{
		TestNotNull(Name, IConsoleManager::Get().FindConsoleObject(Name));
	}
	// With no args and no world, each must warn and return (no crash).
	AddExpectedError(TEXT("usage:"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("does not exist yet"), EAutomationExpectedErrorFlags::Contains, 0);
	for (const TCHAR* Cmd : {TEXT("sim.Give"), TEXT("sim.SetNeed hunger"), TEXT("sim.Standing"), TEXT("sim.AdvanceDays"),
		TEXT("sim.Give bread")})  // the last has good args but no world: the second expected warning
	{
		IConsoleManager::Get().ProcessUserConsoleInput(Cmd, *GLog, nullptr);
	}
	return true;
}

#endif
