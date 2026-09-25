// SimSavesTest.cpp — part 3 Task 2: the slot index behind the save/load list.
#include "Misc/AutomationTest.h"
#include "SimSaves.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimSavesSlotIndex, "Sim.Saves.SlotIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSimSavesSlotIndex::RunTest(const FString&)
{
	USimSaveGame* Old = NewObject<USimSaveGame>();
	Old->Label = TEXT("older");
	Old->SavedAt = FDateTime(2026, 1, 1);
	Old->Day = 3;
	USimSaveGame* New = NewObject<USimSaveGame>();
	New->Label = TEXT("newer");
	New->SavedAt = FDateTime(2026, 6, 1);
	New->Day = 40;
	TestTrue(TEXT("write old"), SimSaves::Detail::WriteSlot(TEXT("test_old"), Old));
	TestTrue(TEXT("write new"), SimSaves::Detail::WriteSlot(TEXT("test_new"), New));

	TArray<FSimSlotInfo> Slots = SimSaves::List();
	Slots.RemoveAll([](const FSimSlotInfo& S) { return !S.Slot.StartsWith(TEXT("test_")); });
	if (TestEqual(TEXT("both listed"), Slots.Num(), 2))
	{
		TestEqual(TEXT("newest first"), Slots[0].Slot, FString(TEXT("test_new")));
		TestEqual(TEXT("its label"), Slots[0].Label, FString(TEXT("newer")));
		TestEqual(TEXT("its day"), Slots[0].Day, int64(40));
	}
	TestTrue(TEXT("delete"), SimSaves::Delete(TEXT("test_new")));
	TestFalse(TEXT("gone from disk"), UGameplayStatics::DoesSaveGameExist(TEXT("test_new"), 0));
	Slots = SimSaves::List();
	TestFalse(TEXT("gone from the list"), Slots.ContainsByPredicate([](const FSimSlotInfo& S) { return S.Slot == TEXT("test_new"); }));
	TestFalse(TEXT("deleting twice fails"), SimSaves::Delete(TEXT("test_new")));
	SimSaves::Delete(TEXT("test_old"));
	return true;
}

#endif
