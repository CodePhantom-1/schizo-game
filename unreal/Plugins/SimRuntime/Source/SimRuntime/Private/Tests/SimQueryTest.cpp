// SimQueryTest.cpp — A7: the UI's query surface returns real data.
#include "Misc/AutomationTest.h"
#include "SimGameInstanceSubsystem.h"
#include "SimQueryLibrary.h"
#include "sim/CApi.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimQueryTest, "Sim.Query.QuestsJournalPeople",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSimQueryTest::RunTest(const FString& Parameters)
{
	SimWorld* W = sim_world_create(TCHAR_TO_UTF8(*USimGameInstanceSubsystem::CanonDir()), 42);
	if (!TestNotNull(TEXT("world"), W)) return false;
	const TArray<FSimQuestInfo> Available = SimQuery::GetQuests(W, ESimQuestList::Available);
	TestTrue(TEXT("quests available"), Available.Num() > 30);
	TestEqual(TEXT("accept"), SimQuery::AcceptQuest(W, TEXT("the_outsiders_first_days")), 0);
	const TArray<FSimQuestInfo> Active = SimQuery::GetQuests(W, ESimQuestList::Active);
	if (TestEqual(TEXT("one active"), Active.Num(), 1))
	{
		TestEqual(TEXT("its title"), Active[0].Title, FString(TEXT("The Outsider's First Days")));
		TestEqual(TEXT("its kind"), Active[0].Kind, FString(TEXT("systemic")));
		TestEqual(TEXT("its act"), Active[0].Act, FString(TEXT("opening")));
	}
	TestEqual(TEXT("journal has the acceptance"), SimQuery::GetJournal(W, TEXT("the_outsiders_first_days")).Num(), 1);
	TestTrue(TEXT("the captain speaks"), SimQuery::GetDialogue(W, TEXT("the captain of the prisoner transport")).Num() > 0);
	char Id[128] = {};
	sim_world_npc_id(W, 0, Id, sizeof(Id));
	TestFalse(TEXT("npc name is not its id"), SimQuery::GetNpcName(W, UTF8_TO_TCHAR(Id)).Contains(TEXT("_")));
	sim_world_set_drought(W, 3);
	sim_world_advance_days(W, 400);
	const TArray<FSimEventInfo> Recent = SimQuery::GetRecentEvents(W, 5);
	TestTrue(TEXT("some events, at most 5"), Recent.Num() > 0 && Recent.Num() <= 5);
	if (Recent.Num() >= 2) TestTrue(TEXT("newest first"), Recent[0].Day >= Recent[1].Day);
	TestEqual(TEXT("null world: empty"), SimQuery::GetQuests(nullptr, ESimQuestList::Active).Num(), 0);
	sim_world_destroy(W);
	return true;
}

#endif
