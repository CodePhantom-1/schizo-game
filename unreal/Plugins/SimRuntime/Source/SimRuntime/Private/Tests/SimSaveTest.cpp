// SimSaveTest.cpp — part 3 Task 1: the kernel world's lifecycle.
#include "Misc/AutomationTest.h"
#include "SimGameInstanceSubsystem.h"
#include "Engine/GameInstance.h"
#include "UObject/Package.h"
#include "sim/CApi.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimSaveRoundTrip, "Sim.Save.StringRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSimSaveRoundTrip::RunTest(const FString&)
{
	UGameInstance* GI = NewObject<UGameInstance>(GetTransientPackage());  // subsystems live within a game instance
	USimGameInstanceSubsystem* S = NewObject<USimGameInstanceSubsystem>(GI);
	TestTrue(TEXT("new world"), S->NewWorld(7));
	sim_world_advance_days(S->GetHandle(), 40);
	sim_world_give_item(S->GetHandle(), "player", "bread", 3);
	FString Saved;
	TestTrue(TEXT("save"), S->SaveToString(Saved));
	TestTrue(TEXT("non-empty"), Saved.Len() > 100);
	sim_world_advance_days(S->GetHandle(), 10);
	TestTrue(TEXT("load"), S->LoadFromString(Saved));
	TestEqual(TEXT("day restored"), sim_world_day(S->GetHandle()), int64(41));
	TestEqual(TEXT("bread restored"), sim_world_item_count(S->GetHandle(), "player", "bread"), 3);
	S->ConditionalBeginDestroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimSaveBadData, "Sim.Save.BadDataKeepsWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSimSaveBadData::RunTest(const FString&)
{
	UGameInstance* GI = NewObject<UGameInstance>(GetTransientPackage());  // subsystems live within a game instance
	USimGameInstanceSubsystem* S = NewObject<USimGameInstanceSubsystem>(GI);
	S->NewWorld(7);
	sim_world_advance_days(S->GetHandle(), 5);
	SimWorld* Before = S->GetHandle();
	AddExpectedError(TEXT("not a SchizoGame save"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("garbage refused"), S->LoadFromString(TEXT("hello, this is not a save")));
	TestTrue(TEXT("same world kept"), S->GetHandle() == Before);
	TestEqual(TEXT("its day kept"), sim_world_day(S->GetHandle()), int64(6));
	S->ConditionalBeginDestroy();
	return true;
}

#endif
