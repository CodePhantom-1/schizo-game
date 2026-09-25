// SimUiTest.cpp — part 3 Task 3: the screen stack and the toast queue (pure logic).
#include "Misc/AutomationTest.h"
#include "UI/SimScreenStack.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimUiScreenStack, "Sim.Ui.ScreenStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSimUiScreenStack::RunTest(const FString&)
{
	FSimScreenStack S;
	TestTrue(TEXT("starts empty"), S.IsEmpty());
	TestFalse(TEXT("empty: no pause"), S.WantsPause());
	S.Push(ESimScreen::Pause);
	S.Push(ESimScreen::Settings);
	S.Push(ESimScreen::Settings);
	TestEqual(TEXT("re-push top is a no-op"), S.Num(), 2);
	TestTrue(TEXT("pauses"), S.WantsPause());
	TestFalse(TEXT("PopIf wrong screen"), S.PopIf(ESimScreen::Journal));
	TestTrue(TEXT("PopIf top"), S.PopIf(ESimScreen::Settings));
	TestTrue(TEXT("pop"), S.Pop());
	TestFalse(TEXT("pop empty"), S.Pop());
	TestFalse(TEXT("input free again"), S.BlocksGameInput());
	S.Push(ESimScreen::Tablet);
	TestFalse(TEXT("reading a tablet does not pause the world"), S.WantsPause());
	TestTrue(TEXT("but it takes the input"), S.BlocksGameInput());
	S.Push(ESimScreen::Journal);
	TestTrue(TEXT("a menu above it pauses"), S.WantsPause());
	TestTrue(TEXT("top is the journal"), S.Top().IsSet() && S.Top().GetValue() == ESimScreen::Journal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimUiToasts, "Sim.Ui.Toasts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSimUiToasts::RunTest(const FString&)
{
	FSimToastQueue Q;
	for (int32 i = 0; i < 4; ++i)
	{
		Q.Add(FText::AsNumber(i), 3.f);
	}
	TestEqual(TEXT("at most 3"), Q.Num(), 3);
	TestTrue(TEXT("the oldest dropped"), Q.Items()[0].Text.EqualTo(FText::AsNumber(1)));
	Q.Tick(1.f);
	TestEqual(TEXT("still there"), Q.Num(), 3);
	Q.Tick(2.1f);
	TestEqual(TEXT("expired"), Q.Num(), 0);
	return true;
}

#endif
