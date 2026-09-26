// SimShellSubsystem.h — A8: the screens. Owns the screen stack, shows the top
// screen on the viewport, takes the input while a screen is open, pauses the
// world while a menu is open, and shows toasts. Screens are Slate widgets
// built in C++ (UI/SimScreens.h) — no Blueprint or UMG assets.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UI/SimScreenStack.h"
#include "SimShellSubsystem.generated.h"

class SWidget;
class SSimToastOverlay;

UCLASS()
class SCHIZOGAME_API USimShellSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Opens a screen on top (logs "Shell: open <name>"). */
	void Open(ESimScreen Screen);
	/** Esc / gamepad B: closes the top screen. The main menu never closes this way. */
	void CloseTop();
	/** Closes S if it is on top. */
	void Close(ESimScreen Screen);
	void CloseAll();
	/** Rebuilds the top screen (after its data changed: a save written, a slot deleted). */
	void Refresh() { ShowTop(); }
	bool IsOpen(ESimScreen Screen) const { return Stack.Contains(Screen); }
	bool IsAnyOpen() const { return !Stack.IsEmpty(); }
	/** The Esc key while playing: opens the pause menu, or closes the top screen. */
	void OnBack();

	/** A short notice that does not take the input. */
	void Toast(const FText& Text, float Seconds = 3.f);
	/** The tablet reader (letters, deeds, verdicts, rituals). */
	void ShowTablet(const FText& Title, const FText& Body);
	const FText& GetTabletTitle() const { return TabletTitle; }
	const FText& GetTabletBody() const { return TabletBody; }

	/** Starts a fresh game (A12): a new world, the settings applied, the player at the start. */
	void StartNewGame();

	static USimShellSubsystem* Get(const UObject* WorldContext);
	static const TCHAR* ScreenName(ESimScreen Screen);

	/** The toast queue (read by the toast overlay each frame). */
	FSimToastQueue& Toasts() { return ToastQueue; }

private:
	void ShowTop();
	void ApplyInputAndPause();
	void EnsureToastOverlay();

	FSimScreenStack Stack;
	FSimToastQueue ToastQueue;
	TSharedPtr<SWidget> Shown;
	TSharedPtr<SSimToastOverlay> ToastOverlay;
	FText TabletTitle;
	FText TabletBody;
};
