// SimScreenStack.h — A8: which screens are open, what they block, and the
// toast queue. Pure logic (no Slate), so it is unit-tested; the shell
// subsystem (SimShellSubsystem.h) turns it into widgets, input and pause.
#pragma once

#include "CoreMinimal.h"

enum class ESimScreen : uint8
{
	MainMenu,
	Pause,
	SaveLoad,
	Settings,
	Journal,
	Tablet,
	Loading
};

class FSimScreenStack
{
public:
	/** Opens S on top; opening the screen already on top does nothing. */
	void Push(ESimScreen S)
	{
		if (Screens.Num() == 0 || Screens.Last() != S)
		{
			Screens.Add(S);
		}
	}
	/** Closes the top screen; false when nothing is open. */
	bool Pop() { return Screens.Num() > 0 && (Screens.Pop(EAllowShrinking::No), true); }
	/** Closes S only if it is on top. */
	bool PopIf(ESimScreen S) { return Screens.Num() > 0 && Screens.Last() == S && Pop(); }
	void Clear() { Screens.Reset(); }
	TOptional<ESimScreen> Top() const { return Screens.Num() > 0 ? TOptional<ESimScreen>(Screens.Last()) : TOptional<ESimScreen>(); }
	bool Contains(ESimScreen S) const { return Screens.Contains(S); }
	bool IsEmpty() const { return Screens.Num() == 0; }
	int32 Num() const { return Screens.Num(); }
	/** The world stops while any menu is open; reading a tablet or loading does not stop it. */
	bool WantsPause() const
	{
		return Screens.ContainsByPredicate([](ESimScreen S) { return S != ESimScreen::Tablet && S != ESimScreen::Loading; });
	}
	/** Any open screen takes the keyboard and mouse from the game. */
	bool BlocksGameInput() const { return Screens.Num() > 0; }

private:
	TArray<ESimScreen> Screens;
};

/** Short, non-blocking notices ("Saved", "Loaded"): at most three, each for a while. */
class FSimToastQueue
{
public:
	struct FItem
	{
		FText Text;
		float Remaining = 0.f;
	};
	void Add(const FText& Text, float Seconds)
	{
		if (Items_.Num() == MaxItems)
		{
			Items_.RemoveAt(0);
		}
		Items_.Add({Text, Seconds});
	}
	void Tick(float DeltaSeconds)
	{
		for (FItem& I : Items_)
		{
			I.Remaining -= DeltaSeconds;
		}
		Items_.RemoveAll([](const FItem& I) { return I.Remaining <= 0.f; });
	}
	int32 Num() const { return Items_.Num(); }
	const TArray<FItem>& Items() const { return Items_; }

private:
	static constexpr int32 MaxItems = 3;
	TArray<FItem> Items_;
};
