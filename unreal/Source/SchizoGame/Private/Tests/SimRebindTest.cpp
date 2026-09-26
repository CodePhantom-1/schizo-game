// SimRebindTest.cpp — part 3 Task 6: a rebound key leaves the action that had it.
#include "Misc/AutomationTest.h"
#include "UI/SimRebind.h"
#include "GameFramework/InputSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<FKey> KeysOf(FName Action)
	{
		TArray<FKey> Keys;
		for (const FInputActionKeyMapping& M : GetDefault<UInputSettings>()->GetActionMappings())
		{
			if (M.ActionName == Action) Keys.Add(M.Key);
		}
		return Keys;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimRebindMovesKey, "Sim.Input.RebindMovesKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSimRebindMovesKey::RunTest(const FString&)
{
	UInputSettings* Input = GetMutableDefault<UInputSettings>();
	const TArray<FInputActionKeyMapping> SavedActions = Input->GetActionMappings();
	const TArray<FInputAxisKeyMapping> SavedAxes = Input->GetAxisMappings();

	SimRebind::ResetToDefaults();  // start from the project's keys
	TestTrue(TEXT("Use is E by default"), KeysOf(TEXT("Use")).Contains(EKeys::E));
	TestTrue(TEXT("Eat is F by default"), KeysOf(TEXT("Eat")).Contains(EKeys::F));

	TestTrue(TEXT("rebind"), SimRebind::Rebind(TEXT("Use"), false, 1.f, EKeys::E, EKeys::F));
	TestTrue(TEXT("Use is now F"), KeysOf(TEXT("Use")).Contains(EKeys::F));
	TestFalse(TEXT("Use lost E"), KeysOf(TEXT("Use")).Contains(EKeys::E));
	TestFalse(TEXT("Eat lost F (no double binding)"), KeysOf(TEXT("Eat")).Contains(EKeys::F));

	const TArray<SimRebind::FBinding> List = SimRebind::List();
	TestTrue(TEXT("the list shows Eat as unbound"), List.ContainsByPredicate(
		[](const SimRebind::FBinding& B) { return B.Name == TEXT("Eat") && !B.Key.IsValid(); }));

	TestTrue(TEXT("axis rebind"), SimRebind::Rebind(TEXT("MoveForward"), true, 1.f, EKeys::W, EKeys::Up));
	TestTrue(TEXT("forward is Up"), GetDefault<UInputSettings>()->GetAxisMappings().ContainsByPredicate(
		[](const FInputAxisKeyMapping& M) { return M.AxisName == TEXT("MoveForward") && M.Key == EKeys::Up && M.Scale > 0.f; }));

	// A second rebind of the same action, as the (possibly stale) UI would call it:
	// the action ends with one keyboard key, never two.
	TestTrue(TEXT("rebind Jump to E"), SimRebind::Rebind(TEXT("Jump"), false, 1.f, EKeys::SpaceBar, EKeys::E));
	TestTrue(TEXT("rebind Jump again, with a stale old key"), SimRebind::Rebind(TEXT("Jump"), false, 1.f, EKeys::SpaceBar, EKeys::Q));
	TestEqual(TEXT("Jump has one key"), KeysOf(TEXT("Jump")).Num(), 1);
	TestTrue(TEXT("and it is Q"), KeysOf(TEXT("Jump")).Contains(EKeys::Q));
	// A movement direction that loses its key stays in the list, unbound, so it can be given one.
	TestTrue(TEXT("Up (forward, since the axis rebind above) to Use"), SimRebind::Rebind(TEXT("Use"), false, 1.f, EKeys::F, EKeys::Up));
	TestTrue(TEXT("MoveForward (+) listed unbound"), SimRebind::List().ContainsByPredicate(
		[](const SimRebind::FBinding& B) { return B.bAxis && B.Name == TEXT("MoveForward") && B.Scale > 0.f && !B.Key.IsValid(); }));

	SimRebind::ResetToDefaults();
	TestTrue(TEXT("reset: Use is E"), KeysOf(TEXT("Use")).Contains(EKeys::E));
	TestTrue(TEXT("reset: Eat is F"), KeysOf(TEXT("Eat")).Contains(EKeys::F));

	// Leave no trace: the player's own bindings back as they were.
	for (const FInputActionKeyMapping& M : TArray<FInputActionKeyMapping>(Input->GetActionMappings())) Input->RemoveActionMapping(M, false);
	for (const FInputAxisKeyMapping& M : TArray<FInputAxisKeyMapping>(Input->GetAxisMappings())) Input->RemoveAxisMapping(M, false);
	for (const FInputActionKeyMapping& M : SavedActions) Input->AddActionMapping(M, false);
	for (const FInputAxisKeyMapping& M : SavedAxes) Input->AddAxisMapping(M, false);
	Input->SaveKeyMappings();
	Input->ForceRebuildKeymaps();
	return true;
}

#endif
