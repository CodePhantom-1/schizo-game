// SimRebind.cpp — A9: see the header; plus the Controls tab's rebind list.
#include "UI/SimRebind.h"

#include "UI/SimScreens.h"
#include "UI/SimUiWidgets.h"

#include "GameFramework/InputSettings.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SInputKeySelector.h"
#include "Widgets/SBoxPanel.h"

namespace
{
	/** The project's own mappings, straight from Config/DefaultInput.ini's +ActionMappings /
	 *  +AxisMappings lines (not the player's saved ones, not the engine's layered cache). */
	void ReadDefaults(TArray<FInputActionKeyMapping>& Actions, TArray<FInputAxisKeyMapping>& Axes)
	{
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultInput.ini")));
		for (const FString& Line : Lines)
		{
			FString Value;
			if (Line.Split(TEXT("+ActionMappings="), nullptr, &Value) && Line.StartsWith(TEXT("+")))
			{
				FInputActionKeyMapping M;
				FInputActionKeyMapping::StaticStruct()->ImportText(*Value, &M, nullptr, PPF_None, GLog, TEXT("DefaultInput"));
				if (M.Key.IsValid()) Actions.Add(M);
			}
			else if (Line.Split(TEXT("+AxisMappings="), nullptr, &Value) && Line.StartsWith(TEXT("+")))
			{
				FInputAxisKeyMapping M;
				FInputAxisKeyMapping::StaticStruct()->ImportText(*Value, &M, nullptr, PPF_None, GLog, TEXT("DefaultInput"));
				if (M.Key.IsValid()) Axes.Add(M);
			}
		}
	}

	void Commit(UInputSettings* Input)
	{
		Input->SaveKeyMappings();
		Input->ForceRebuildKeymaps();
	}
}

namespace SimRebind
{
	TArray<FBinding> List()
	{
		// Every name the project defines, so an action that lost its key still shows (unbound).
		TArray<FInputActionKeyMapping> DefActions;
		TArray<FInputAxisKeyMapping> DefAxes;
		ReadDefaults(DefActions, DefAxes);
		const UInputSettings* Input = GetDefault<UInputSettings>();
		TArray<FBinding> Out;
		for (const FInputActionKeyMapping& M : Input->GetActionMappings())
		{
			Out.Add({M.ActionName, false, 1.f, M.Key});
		}
		for (const FInputAxisKeyMapping& M : Input->GetAxisMappings())
		{
			if (!M.Key.IsGamepadKey() && M.Key != EKeys::MouseX && M.Key != EKeys::MouseY)
			{
				Out.Add({M.AxisName, true, M.Scale, M.Key});
			}
		}
		for (const FInputActionKeyMapping& D : DefActions)
		{
			if (!Out.ContainsByPredicate([&](const FBinding& B) { return !B.bAxis && B.Name == D.ActionName; }))
			{
				Out.Add({D.ActionName, false, 1.f, FKey()});
			}
		}
		// A movement direction that lost its key stays listed (unbound), so it can be given one.
		for (const FInputAxisKeyMapping& D : DefAxes)
		{
			if (D.Key.IsGamepadKey() || D.Key == EKeys::MouseX || D.Key == EKeys::MouseY)
			{
				continue;
			}
			if (!Out.ContainsByPredicate([&](const FBinding& B) { return B.bAxis && B.Name == D.AxisName && FMath::IsNearlyEqual(B.Scale, D.Scale); }))
			{
				Out.Add({D.AxisName, true, D.Scale, FKey()});
			}
		}
		Out.Sort([](const FBinding& A, const FBinding& B)
		{
			return A.Name.LexicalLess(B.Name) || (A.Name == B.Name && A.Scale > B.Scale);
		});
		return Out;
	}

	bool Rebind(FName Name, bool bAxis, float Scale, FKey OldKey, FKey NewKey)
	{
		if (!NewKey.IsValid() || Name.IsNone())
		{
			return false;
		}
		UInputSettings* Input = GetMutableDefault<UInputSettings>();
		// The new key leaves every other mapping first: one key, one meaning.
		for (const FInputActionKeyMapping& M : TArray<FInputActionKeyMapping>(Input->GetActionMappings()))
		{
			if (M.Key == NewKey) Input->RemoveActionMapping(M, false);
		}
		for (const FInputAxisKeyMapping& M : TArray<FInputAxisKeyMapping>(Input->GetAxisMappings()))
		{
			if (M.Key == NewKey) Input->RemoveAxisMapping(M, false);
		}
		// The action (or axis direction) keeps one key per device class: a new keyboard/mouse key
		// replaces its keyboard/mouse key(s), whatever OldKey the (possibly stale) caller names;
		// a gamepad binding is left alone, and the other way round.
		const bool bPad = NewKey.IsGamepadKey();
		if (bAxis)
		{
			for (const FInputAxisKeyMapping& M : TArray<FInputAxisKeyMapping>(Input->GetAxisMappings()))
			{
				if (M.AxisName == Name && FMath::IsNearlyEqual(M.Scale, Scale) && M.Key.IsGamepadKey() == bPad)
				{
					Input->RemoveAxisMapping(M, false);
				}
			}
			Input->AddAxisMapping(FInputAxisKeyMapping(Name, NewKey, Scale), false);
		}
		else
		{
			for (const FInputActionKeyMapping& M : TArray<FInputActionKeyMapping>(Input->GetActionMappings()))
			{
				if (M.ActionName == Name && M.Key.IsGamepadKey() == bPad)
				{
					Input->RemoveActionMapping(M, false);
				}
			}
			Input->AddActionMapping(FInputActionKeyMapping(Name, NewKey), false);
		}
		Commit(Input);
		return true;
	}

	void ResetToDefaults()
	{
		TArray<FInputActionKeyMapping> Actions;
		TArray<FInputAxisKeyMapping> Axes;
		ReadDefaults(Actions, Axes);
		if (Actions.Num() == 0)
		{
			return;  // no project defaults found: keep what the player has
		}
		UInputSettings* Input = GetMutableDefault<UInputSettings>();
		for (const FInputActionKeyMapping& M : TArray<FInputActionKeyMapping>(Input->GetActionMappings())) Input->RemoveActionMapping(M, false);
		for (const FInputAxisKeyMapping& M : TArray<FInputAxisKeyMapping>(Input->GetAxisMappings())) Input->RemoveAxisMapping(M, false);
		for (const FInputActionKeyMapping& M : Actions) Input->AddActionMapping(M, false);
		for (const FInputAxisKeyMapping& M : Axes) Input->AddAxisMapping(M, false);
		Commit(Input);
	}
}

TSharedRef<SWidget> MakeRebindList(TFunction<void()> OnChanged)
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (const SimRebind::FBinding& B : SimRebind::List())
	{
		const FText Label = B.bAxis
			? FText::Format(NSLOCTEXT("SimUi", "AxisDir", "{0} ({1})"), FText::FromName(B.Name),
				B.Scale > 0.f ? NSLOCTEXT("SimUi", "Plus", "+") : NSLOCTEXT("SimUi", "Minus", "−"))
			: FText::FromName(B.Name);
		Box->AddSlot().AutoHeight()[SimUi::Row(Label,
			SNew(SInputKeySelector)
			.SelectedKey(FInputChord(B.Key))
			.Font(SimUiStyle::Body())
			.AllowModifierKeys(false)
			.AllowGamepadKeys(true)
			.EscapeCancelsSelection(true)
			.KeySelectionText(NSLOCTEXT("SimUi", "PressKey", "Press a key…"))
			.NoKeySpecifiedText(NSLOCTEXT("SimUi", "Unbound", "(unbound)"))
			.OnKeySelected_Lambda([B, OnChanged](const FInputChord& Chord)
			{
				if (SimRebind::Rebind(B.Name, B.bAxis, B.Scale, B.Key, Chord.Key) && OnChanged) OnChanged();  // show what moved
			}))];
	}
	Box->AddSlot().AutoHeight().Padding(0.f, 8.f)[SimUi::SmallButton(NSLOCTEXT("SimUi", "ResetKeys", "Reset to defaults"),
		[OnChanged] { SimRebind::ResetToDefaults(); if (OnChanged) OnChanged(); })];
	return Box;
}
