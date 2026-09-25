# Stage A, part 3 — The UI Shell — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The game becomes a game you can start, pause, save, load, configure and quit.
- A main menu over the living city: Continue, New Game, Load, Settings, Quit.
- A pause menu.
- Save and load slots, plus quicksave and quickload, and an autosave when you sleep.
- Settings: gameplay, graphics, audio, controls with full remapping, and accessibility. Each setting takes effect in the system it names.
- A journal panel on the part-2 bridge.
- Toasts and a tablet reader.
- A first-launch notice for the long shader compile.
- A polished third-person camera.

**Architecture:**
- **Screens are Slate widgets written in C++** (`SCompoundWidget`). There are no Blueprint or UMG assets, so everything is built and tested headless.
- **`USimShellSubsystem` (a `ULocalPlayerSubsystem`)** owns a pure-C++ screen stack (`FSimScreenStack`, unit-tested). It puts the top screen on the viewport, switches input mode and cursor, and pauses the game while a menu is open.
- **Saving** is split in two:
  - SimRuntime gains world-lifecycle calls: `NewWorld`, `SaveToString`, `LoadFromString`. A failed load keeps the old world.
  - SchizoGame adds `USimSaveGame` (a `USaveGame`) and `USimSaves`. A save holds the kernel snapshot, the sub-day clock, the player's transform, and slot metadata for the list.
- **Settings** live in `USimGameUserSettings` (a `UGameUserSettings`, config `GameUserSettings`). `ApplySimSettings()` pushes each value to its system. Values whose system doesn't exist yet are stored now; the table in Task 4 names the stage that will read each one.
- **Input** stays on the project's legacy action/axis mappings. Remapping goes through `UInputSettings` (`RemoveActionMapping`/`AddActionMapping`/`SaveKeyMappings`), which persists per user.

**Tech Stack:** UE 5.8.3 (Slate/SlateCore, the `Engine` save-game system, `UGameUserSettings`, the Automation framework), the C++20 kernel through the C API, the existing tools (`ue_test.sh`, `ue_smoke.sh`).

**Spec:** [docs/completion-plan.md](../../completion-plan.md) steps A8–A12, A15, and DECISIONS D-024 (third person; adjustable day length; voice hooks).

## Global Constraints

- Every player-facing string is `NSLOCTEXT("SimUi", "<Key>", "…")` or `FText::Format` of such. The one exception is data the kernel returns (names, quest titles), shown via `FText::FromString`.
- Screens support mouse, keyboard and gamepad navigation. Every button is focusable, the first one gets focus when a screen opens, and Esc / gamepad B closes the top screen, except the main menu.
- The style is D-023: flat panels, warm mudbrick and lapis colours, no textures. The colours are defined once in `SimUiStyle.h`.
- The save size is logged, and a save over 50 MB logs an error (architecture §5 budget).
- Legacy input only: no Enhanced Input assets.
- Gate: kernel ctest, canon lint/coverage/stage, UE build, `tools/ue_test.sh`, `tools/ue_smoke.sh` (extended here), and `tools/capi_reach.py --check`.
- Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. After the plan: fresh review → fix → HANDOFF → push → CI.

## Review Focus

- **Loading a corrupt or foreign save file.** The game must keep running on the current world and say so; the handle must never become null or dangling. Pinned in Task 1, `Sim.Save.BadDataKeepsWorld`.
- **Saving mid-day, then loading after the day has turned.** The hour, the day and the player's position must all come back exactly. Pinned in Task 2, the `sim.SaveSlot`/`sim.LoadSlot` smoke lines.
- **Rebinding a key that another action already uses.** The other action loses that key (no silent double binding), and the change survives a restart. Pinned in Task 6, `Sim.Input.RebindMovesKey`.
- **Opening and closing menus fast** (Esc spam, a toast arriving under a menu). The stack stays consistent, the game unpauses only when no menu is left, and gameplay input comes back. Pinned in Task 3, `Sim.Ui.ScreenStack`.
- **A settings value out of range in a hand-edited ini** (day length 0, severity 900). It is clamped on load. Pinned in Task 4, `Sim.Settings.ClampAndApply`.

---

### Task 1: World lifecycle in SimRuntime — new world, save to and load from a string

**Files:**
- Modify: `unreal/Plugins/SimRuntime/Source/SimRuntime/Public/SimGameInstanceSubsystem.h`, `…/Private/SimGameInstanceSubsystem.cpp`
- Test: `…/Private/Tests/SimSaveTest.cpp`

**Interfaces:**
- Produces:
  ```cpp
  // USimGameInstanceSubsystem (public):
  bool NewWorld(uint64 Seed);                       // replaces the world with a fresh one; false keeps the old
  bool SaveToString(FString& Out) const;            // kernel snapshot (sim_world_save_to_buffer)
  bool LoadFromString(const FString& Data);         // replaces the world; false (bad data) keeps the old
  void SetSecondsSinceLastDay(double Seconds);      // the sub-day clock, restored by a load
  static USimGameInstanceSubsystem* Get(const UObject* WorldContext);
  ```

- [ ] **Step 1: Failing test** `SimSaveTest.cpp`. The subsystem needs a game instance, so the test creates one: `NewObject<UGameInstance>(GetTransientPackage())`, then `GI->Init()`, then `GI->GetSubsystem<USimGameInstanceSubsystem>()`. If `Init` needs a world context in automation, create the subsystem with `NewObject<USimGameInstanceSubsystem>()` instead and call its methods directly (they don't touch the world).

```cpp
// SimSaveTest.cpp — part 3 Task 1: the kernel world's lifecycle.
#include "Misc/AutomationTest.h"
#include "SimGameInstanceSubsystem.h"
#include "sim/CApi.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimSaveRoundTrip, "Sim.Save.StringRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FSimSaveRoundTrip::RunTest(const FString&)
{
	USimGameInstanceSubsystem* S = NewObject<USimGameInstanceSubsystem>();
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
	USimGameInstanceSubsystem* S = NewObject<USimGameInstanceSubsystem>();
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
```

Build and watch it fail (`NewWorld` not a member).

- [ ] **Step 2: Implement.** In the header add the declarations above. Make `CreateWorld` call `NewWorld(1)`, keeping its canon checks: move the sentinel check into a private `bool CanonReady()`. In the .cpp:

```cpp
USimGameInstanceSubsystem* USimGameInstanceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<USimGameInstanceSubsystem>() : nullptr;
}

bool USimGameInstanceSubsystem::NewWorld(uint64 Seed)
{
	if (!CanonReady()) return false;
	SimWorld* Fresh = sim_world_create(TCHAR_TO_UTF8(*CanonDir()), Seed);
	if (Fresh == nullptr)
	{
		UE_LOG(LogSimRuntime, Error, TEXT("sim_world_create failed on the canon at %s."), *CanonDir());
		return false;
	}
	if (SimHandle != nullptr) sim_world_destroy(SimHandle);
	SimHandle = Fresh;
	SecondsSinceLastDay = 0.0;
	return true;
}

bool USimGameInstanceSubsystem::SaveToString(FString& Out) const
{
	if (SimHandle == nullptr) return false;
	const int Len = sim_world_save_to_buffer(SimHandle, nullptr, 0);
	if (Len < 0) return false;
	TArray<ANSICHAR> Buf;
	Buf.SetNumZeroed(Len + 1);
	if (sim_world_save_to_buffer(SimHandle, Buf.GetData(), Len + 1) != Len) return false;
	Out = UTF8_TO_TCHAR(Buf.GetData());
	return true;
}

bool USimGameInstanceSubsystem::LoadFromString(const FString& Data)
{
	if (!CanonReady()) return false;
	SimWorld* Loaded = sim_world_load_from_buffer(TCHAR_TO_UTF8(*CanonDir()), TCHAR_TO_UTF8(*Data));
	if (Loaded == nullptr)
	{
		UE_LOG(LogSimRuntime, Error, TEXT("Load refused: not a SchizoGame save (or made from a different canon). The current world continues."));
		return false;
	}
	if (SimHandle != nullptr) sim_world_destroy(SimHandle);
	SimHandle = Loaded;
	return true;
}

void USimGameInstanceSubsystem::SetSecondsSinceLastDay(double Seconds) { SecondsSinceLastDay = FMath::Max(0.0, Seconds); }
```

Check first that `sim_world_save_to_buffer(w, nullptr, 0)` returns the length. The part-1 convention says -1 on a null `out`, so read `CApi.cpp`. If it returns -1, add a length query to the kernel: accept `out == nullptr && cap == 0` and return the length, pinned by a kernel test in `test_capi.cpp`.

- [ ] **Step 3:** Rebuild the kernel for UE, build the editor, run `tools/ue_test.sh`: every test passes. Commit: `A10: the kernel world's lifecycle — new world, save to and load from a string (a bad save keeps the world)`.

---

### Task 2: Save slots — `USimSaveGame`, quicksave/quickload, autosave on sleep

**Files:**
- Create: `unreal/Source/SchizoGame/Public/SimSaves.h`, `…/Private/SimSaves.cpp`
- Modify: `…/Private/SimBed.cpp` (autosave after sleeping), `…/Private/SimPlayerController.cpp` (F5/F9), `unreal/Config/DefaultInput.ini` (`QuickSave`=F5, `QuickLoad`=F9)
- Modify: `…/Private/SimStreetConsole.cpp` (`sim.SaveSlot <name>`, `sim.LoadSlot <name>`, `sim.Slots`)
- Test: `unreal/Source/SchizoGame/Private/Tests/SimSavesTest.cpp`; smoke lines in `tools/ue_smoke.sh`

**Interfaces:**
- Consumes: Task 1.
- Produces:
  ```cpp
  UCLASS() class USimSaveGame : public USaveGame {
    UPROPERTY() int32 Version = 1;
    UPROPERTY() FString Kernel;              // sim_world_save_to_buffer text
    UPROPERTY() double SecondsSinceLastDay = 0;
    UPROPERTY() FTransform PlayerTransform;
    UPROPERTY() FRotator ControlRotation;
    UPROPERTY() int64 Day = 1; UPROPERTY() FString MonthName; UPROPERTY() int32 DayOfMonth = 1;
    UPROPERTY() float Hour = 6.f; UPROPERTY() FDateTime SavedAt; UPROPERTY() FString Label;
  };
  USTRUCT(BlueprintType) struct FSimSlotInfo { FString Slot, Label, MonthName; int64 Day; int32 DayOfMonth; float Hour; FDateTime SavedAt; };
  namespace SimSaves {
    bool Save(UObject* Ctx, const FString& Slot, const FString& Label = FString());
    bool Load(UObject* Ctx, const FString& Slot);     // restores kernel, clock, pawn, control rotation
    bool Delete(const FString& Slot);
    TArray<FSimSlotInfo> List();                      // newest first; reads the slot index
    FString MostRecent();                             // "" when none
    const TCHAR* QuickSlot = TEXT("quicksave"); const TCHAR* AutoSlot = TEXT("autosave");
  }
  ```
  The slot list is kept in a small index save (`USimSlotIndex`, slot `"index"`) with a `TArray<FString> Slots`, because the engine has no portable slot enumeration.

- [ ] **Step 1: Failing test** `SimSavesTest.cpp`: `Sim.Saves.SlotIndex` saves two dummy `USimSaveGame` objects through `SimSaves`' internal `WriteSlot(const FString&, USimSaveGame*)` (declared in the header's `SimSaves::Detail` namespace for tests). It checks that `List()` returns both newest first, that `Delete` removes one from the list and from disk (`DoesSaveGameExist` false), and that `MostRecent()` is the newer one. It uses slot names prefixed `test_` and deletes them at the end.
- [ ] **Step 2: Implement** `SimSaves.cpp`:
  - `Save` reads the kernel string (Task 1), the clock (`GetSecondsSinceLastDay`), the pawn transform and the control rotation, plus the calendar metadata from `USimWorldSubsystem`'s getters. It writes the slot, updates the index, and logs `Saved slot %s (%.1f KB)`. It logs an error if the size is over 50 MB.
  - `Load` reads the slot and checks `Version == 1` (anything else is refused with a clear log). It calls `LoadFromString`; on success it sets the clock, teleports the pawn to the saved transform with `ETeleportType::TeleportPhysics`, sets the control rotation, and toasts "Loaded" once Task 3 exists (until then a log line).
  - Autosave: in `SimBed` after `SkipSimHoursFor`, call `SimSaves::Save(this, SimSaves::AutoSlot, "Autosave")`.
  - F5 / F9 in `SimPlayerController` call Save and Load on `QuickSlot`.
  - The console commands wrap the same calls.
- [ ] **Step 3: Smoke lines.** In `tools/ue_smoke.sh`, extend the `-ExecCmds` with `sim.SaveSlot smoke_a, sim.AdvanceDays 3, sim.LoadSlot smoke_a, sim.Dump` after the existing dump. Add `need 'Saved slot smoke_a' …` and a check that the dump after loading shows the same day and hour as the one before saving. Capture the pre-save day and hour with `grep -oE` on the first dump line, then compare against the post-load dump line.
- [ ] **Step 4:** Build, `ue_test.sh`, `ue_smoke.sh`: all pass. Commit: `A10: save slots — quicksave (F5), quickload (F9), autosave on sleep, a slot index`.

---

### Task 3: The screen stack and the shell subsystem

**Files:**
- Create: `unreal/Source/SchizoGame/Public/UI/SimScreenStack.h` (pure C++), `…/Public/UI/SimShellSubsystem.h`, `…/Private/UI/SimShellSubsystem.cpp`, `…/Public/UI/SimUiStyle.h`, `…/Private/UI/SimToast.cpp/.h`
- Modify: `unreal/Source/SchizoGame/SchizoGame.Build.cs` (add `"Slate", "SlateCore", "InputCore"` to the private dependencies)
- Test: `…/Private/Tests/SimUiTest.cpp`

**Interfaces:**
- Produces:
  ```cpp
  enum class ESimScreen : uint8 { MainMenu, Pause, SaveLoad, Settings, Journal, Tablet, Loading };
  class FSimScreenStack {           // pure logic, no Slate
  public:
    void Push(ESimScreen S);        // re-pushing the top is a no-op
    bool Pop();                     // false when empty
    bool PopIf(ESimScreen S);       // pops only if S is on top
    TOptional<ESimScreen> Top() const;
    bool IsEmpty() const;
    bool WantsPause() const;        // true if any screen in the stack pauses (all but Loading and Tablet)
    bool BlocksGameInput() const;   // true unless empty
    int32 Num() const;
  };
  UCLASS() class USimShellSubsystem : public ULocalPlayerSubsystem {
    void Open(ESimScreen S);        // builds the widget, shows it, focuses its first control
    void CloseTop();                // Esc/B; never closes MainMenu
    void CloseAll();
    void Toast(const FText& Text, float Seconds = 3.f);   // non-blocking, stacks up to 3
    void ShowTablet(const FText& Title, const FText& Body);
    bool IsOpen(ESimScreen S) const;
    static USimShellSubsystem* Get(const UObject* Ctx);
  };
  namespace SimUiStyle { FSlateColor Panel(), Ink(), Accent(), Muted(); FSlateFontInfo Title(), Body(), Button(); FMargin Pad(); }
  ```

- [ ] **Step 1: Failing test** `Sim.Ui.ScreenStack`:

```cpp
FSimScreenStack S;
TestTrue(TEXT("starts empty"), S.IsEmpty());
TestFalse(TEXT("empty: no pause"), S.WantsPause());
S.Push(ESimScreen::Pause); S.Push(ESimScreen::Settings); S.Push(ESimScreen::Settings);
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
```

plus `Sim.Ui.Toasts`: a `FSimToastQueue` (pure) holding at most 3 entries with expiry. `Add(text, 3s)` four times leaves 3, dropping the oldest, and `Tick(3.1f)` empties it.

- [ ] **Step 2: Implement** the stack (a `TArray<ESimScreen>`) and the toast queue. Then the subsystem:
  - `Open` pushes, builds the widget through a switch into Task 5's screen factories (`MakeMainMenu(*this)` and so on; until Task 5, `SNullWidget`), and adds it with `GEngine->GameViewport->AddViewportWidgetContent(Widget, 10)`. It removes the previous top's widget from the viewport (only the top is visible) and calls `ApplyInputAndPause()`.
  - `ApplyInputAndPause` runs after every change:
    - Pause follows `UGameplayStatics::SetGamePaused(World, Stack.WantsPause())`.
    - If the stack blocks game input: `PC->SetInputMode(FInputModeUIOnly().SetWidgetToFocus(Top))` and show the cursor.
    - Otherwise: `FInputModeGameOnly`, and hide the cursor.
  - Esc: bind `Pause` (Escape, Gamepad_Special_Right) in `DefaultInput.ini` and in `SimPlayerController`:
    - with nothing open, `Open(Pause)`;
    - with a screen open, the screen's `OnKeyDown` handles Escape/Gamepad_FaceButton_Right by calling `CloseTop`.
  - Toasts are a small overlay widget, always present at a high Z-order, fed by the queue.
- [ ] **Step 3:** Build and test: both pass. Commit: `A8: the screen stack and the shell subsystem (input mode, pause, toasts)`.

---

### Task 4: Settings — `USimGameUserSettings`, clamped, applied to their systems

**Files:**
- Create: `unreal/Source/SchizoGame/Public/SimGameUserSettings.h`, `…/Private/SimGameUserSettings.cpp`
- Modify: `unreal/Config/DefaultEngine.ini` (`[/Script/Engine.Engine] GameUserSettingsClassName=/Script/SchizoGame.SimGameUserSettings`)
- Modify (kernel): `kernel/include/sim/CApiVerbs.h`, `kernel/src/CApiVerbs.cpp`, `kernel/include/sim/World.hpp`, plus the callers of `advance_needs` (a needs-severity percentage held on `WorldState`, **not** saved; the settings re-apply it on every new world or load)
- Test: kernel `test_capi_verbs.cpp` (`test_needs_severity`); UE `…/Private/Tests/SimSettingsTest.cpp`

**Interfaces:**
- Produces:
  ```c
  // CApiVerbs.h
  void sim_world_set_needs_severity(SimWorld* world, int percent);  // clamped 25..300; 100 = designed rate
  int sim_world_needs_severity(const SimWorld* world);               // -1 null
  ```
  ```cpp
  UCLASS(config=GameUserSettings) class USimGameUserSettings : public UGameUserSettings {
    UPROPERTY(config) float DayLengthMinutes = 48.f;     // 5..240   -> sim.DaysPerRealMinute (A4)
    UPROPERTY(config) int32 NeedsSeverityPercent = 100;  // 25..300  -> kernel (now)
    UPROPERTY(config) bool bChronicleMode = false;       // -> J7
    UPROPERTY(config) bool bGuidedMode = false;          // -> P8
    UPROPERTY(config) bool bFastTravel = false;          // -> Q6
    UPROPERTY(config) bool bDisease = true;              // -> B3
    UPROPERTY(config) bool bHistoricalPermadeath = false;// -> B10/P12
    UPROPERTY(config) bool bPlayerVoice = true;          // -> R6
    UPROPERTY(config) bool bSubtitles = true;            // -> R5/P4
    UPROPERTY(config) float SubtitleScale = 1.f;         // 0.75..2 -> HUD + UI text (now)
    UPROPERTY(config) uint8 ColourVision = 0;            // 0 off, 1 protan, 2 deutan, 3 tritan -> Slate renderer (now)
    UPROPERTY(config) float MasterVolume = 1.f, MusicVolume = 0.8f, EffectsVolume = 1.f, VoiceVolume = 1.f; // 0..1 -> R1
    UPROPERTY(config) float MouseSensitivity = 1.f;      // 0.2..3 -> controller (now)
    UPROPERTY(config) bool bInvertY = false;             // -> controller (now)
    void Clamp();                                        // every field into range
    void ApplySimSettings(UWorld* World);                // pushes what exists today
    static USimGameUserSettings* Get();
  };
  ```
  The "-> stage" comments are the contract: each later stage reads its field. Record the list in `docs/completion-plan.md` under A11.

- [ ] **Step 1: Kernel failing test** `test_needs_severity`: a world with severity 200 climbs hunger twice as fast over 10 hours as one at 100 (use `sim_world_advance_needs` and compare `sim_world_hunger`). 900 clamps to 300, 0 clamps to 25, and a null world returns -1. Implement: `WorldState::needs_severity_pct = 100` (not in the snapshot); `advance_needs` callers pass `w.needs_severity_pct / 100.0`. Grep every caller of `advance_needs` and `rest` in `kernel/src` and change all of them.
- [ ] **Step 2: UE failing test** `Sim.Settings.ClampAndApply`: `NewObject<USimGameUserSettings>()`; set `DayLengthMinutes = 0`, `NeedsSeverityPercent = 900`, `SubtitleScale = 9`; `Clamp()`; expect 5, 300 and 2. Then set `DayLengthMinutes = 30` and call `ApplySimSettings(nullptr)`; expect the cvar `sim.DaysPerRealMinute` to read 30.
- [ ] **Step 3: Implement.** `ApplySimSettings`:
  - sets the cvar `sim.DaysPerRealMinute`;
  - calls `sim_world_set_needs_severity` on the current handle (if any);
  - sets the colour-vision mode with `FSlateApplication::Get().GetRenderer()->SetColorVisionDeficiencyType(...)` (guarded by `FSlateApplication::IsInitialized()`);
  - stores the mouse sensitivity and Y-invert for `ASimCharacter`'s turn and look input to read (multiply in `AddControllerYawInput` and `AddControllerPitchInput` wrappers).
  
  Call `ApplySimSettings` on game start, after `NewWorld` or `Load`, and whenever the Settings screen applies.
- [ ] **Step 4:** Kernel suite, UE build and `ue_test.sh`: all pass. Update the A11 row. Commit: `A11: settings — clamped, persisted, applied (day length, needs severity, colour vision, mouse); the rest wired to their stages`.

---

### Task 5: The screens — main menu, pause, save/load, settings, journal, tablet, loading notice

**Files:**
- Create: `unreal/Source/SchizoGame/Private/UI/SimScreens.h`, `SimScreenMainMenu.cpp`, `SimScreenPause.cpp`, `SimScreenSaveLoad.cpp`, `SimScreenSettings.cpp`, `SimScreenJournal.cpp`, `SimScreenTablet.cpp`, `SimScreenLoading.cpp`, and `SimUiWidgets.cpp/.h` (shared: `MakeButton(Text, OnClicked)`, `MakePanel(Content)`, `MakeTitle(Text)`, `MakeSlider(Label, Min, Max, Getter, Setter)`, `MakeCheck(Label, Getter, Setter)`, `MakeChoice(Label, Options, Getter, Setter)`)
- Test: `…/Private/Tests/SimScreensTest.cpp`

**Interfaces:**
- Consumes: Tasks 1–4; `SimQuery::GetQuests/GetJournal` (part 2).
- Produces: `TSharedRef<SWidget> MakeScreen(ESimScreen, USimShellSubsystem&)`, dispatching to one factory per screen. What each screen holds:

| Screen | Contents (every label NSLOCTEXT) | Actions |
|---|---|---|
| Main menu | Title "The City of the Moon" (working title, game-design §11.1); Continue (disabled when there are no slots); New Game; Load; Settings; Quit | Continue → `SimSaves::Load(MostRecent())` + CloseAll · New Game → Task 7's `StartNewGame` · Load → Open(SaveLoad) · Settings → Open(Settings) · Quit → `FGenericPlatformMisc::RequestExit(false)` |
| Pause | "Paused"; Resume; Save; Load; Settings; Journal; Main Menu; Quit | Resume → CloseTop · Save/Load → Open(SaveLoad) · Main Menu → CloseAll + Open(MainMenu) |
| Save/Load | The slot list from `SimSaves::List()`, each row "Label — 12 Rains-Coming, year 1, 14:30 — saved <local time>"; New Save (a text box for the label, default "Day N"); Save over, Load, Delete (with an inline confirm) per row | calls `SimSaves`, then toasts the result |
| Settings | Tabs: Gameplay (day length slider, needs severity, Mythic/Chronicle, guided mode, fast travel, disease, Historical permadeath) · Graphics (`UGameUserSettings` resolution, window mode, overall scalability 0–3, frame limit, V-Sync) · Audio (4 volume sliders) · Controls (mouse sensitivity, invert Y, and the rebind list from Task 6) · Accessibility (subtitles, subtitle size, colour vision, player voice) | Apply → `Clamp` + `ApplySettings(false)` + `ApplySimSettings` + `SaveSettings` · Back → CloseTop (discard unapplied changes by reloading the config) |
| Journal | Three sections: Active (title, giver, stage, "due day N"), Completed, Failed; clicking a quest shows its journal entries (day, stage, text) | read-only; J toggles it |
| Tablet | Title and body text on a clay-coloured panel, scrollable | Close |
| Loading | "Preparing the city…" plus a line explaining the first-launch shader compile ("the first start after an update can take 10–15 minutes; this is not a hang") | closes itself (Task 7) |

- [ ] **Step 1: Failing test** `Sim.Ui.ScreensBuild`: for every `ESimScreen`, `MakeScreen(S, *Shell)` returns a widget whose `GetChildren()->Num() > 0` (a real widget, not `SNullWidget`). Use `NewObject<USimShellSubsystem>()`. The screens must not need a world to *build*; data calls return empty lists with no world.
- [ ] **Step 2: Implement** the shared widgets first, then each screen. Every screen is an `SCompoundWidget` with `SupportsKeyboardFocus() == true` and `OnKeyDown` handling Escape/Gamepad B → `Shell.CloseTop()`, and uses the `SimUiStyle` colours and fonts, scaled by `SubtitleScale` for body text. The Settings screen edits a working copy of the fields and writes them on Apply.
- [ ] **Step 3: Wire the existing tablet** (`SimTablet.cpp`) to `ShowTablet` instead of its current on-screen text, and the J key (`Journal` action, J) to toggle the Journal.
- [ ] **Step 4:** Build, `ue_test.sh`: pass. Windowed iGPU launch: take screenshots of each screen. Add `-SimShotScreens` support to the SimShots mode: open each screen in turn and capture. Read them to confirm the layout at 1280×720. Commit: `A8/A10/A11: the screens — main menu, pause, save/load, settings, journal, tablet, loading notice`.

---

### Task 6: Controls — rebinding that moves keys, and persists

**Files:**
- Create: `unreal/Source/SchizoGame/Private/UI/SimRebind.h/.cpp` (pure logic over `UInputSettings`)
- Modify: `SimScreenSettings.cpp` (the Controls tab lists every action and axis key with a "press a key" capture)
- Test: `…/Private/Tests/SimRebindTest.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace SimRebind {
    struct FBinding { FName Action; FKey Key; bool bAxis; float Scale; };
    TArray<FBinding> List();                                   // every action + axis mapping, sorted by name
    bool Rebind(FName Action, float AxisScale, FKey OldKey, FKey NewKey); // removes NewKey from any other mapping first
    void ResetToDefaults();                                    // reloads DefaultInput.ini mappings
  }
  ```

- [ ] **Step 1: Failing test** `Sim.Input.RebindMovesKey`. Snapshot `UInputSettings`' mappings. Rebind `Use` from E to F, where F is currently `Eat`. Expect `Use`=F, and `Eat` to have no F (it keeps no key, and the UI shows it as unbound). Then `ResetToDefaults()` and expect `Use`=E and `Eat`=F again. Restore the snapshot at the end so the test leaves no trace.
- [ ] **Step 2: Implement** with `RemoveActionMapping`/`AddActionMapping` (the axis equivalents for axes), then `SaveKeyMappings()` and `ForceRebuildKeymaps()`. `ResetToDefaults` re-reads the project defaults: clear the user's Input.ini section and reload the config for `UInputSettings`.
- [ ] **Step 3:** Build, test, commit: `A9: full remapping — a rebound key leaves the action that had it; persisted per user; reset to defaults`.

---

### Task 7: The game's flow — boot to the menu, New Game, the loading notice

**Files:**
- Modify: `unreal/Source/SchizoGame/Private/SimGameMode.cpp` (after the city and street are built: open the MainMenu unless `-SimNoMenu` or `-SimShots` is on the command line; headless smoke and shots stay unattended), `SimShellSubsystem.cpp` (`StartNewGame`)
- Test: smoke lines

**Interfaces:**
- Produces: `void USimShellSubsystem::StartNewGame()`:
  1. Calls `NewWorld(FMath::Rand())` and then `ApplySimSettings`.
  2. Resets the clock to `StartHour`.
  3. Puts the player at the start: the Moon Gate PlayerStart today, the prisoner barracks from Stage AA6.
  4. Clears the quick and auto slots' "continue" preference; leaves the saved files alone.
  5. Calls `CloseAll()`, then toasts `NSLOCTEXT("SimUi","NewGame","You arrive in chains at the City of the Moon.")`.

  The loading notice opens on boot and closes itself once the world exists **and** 30 consecutive frames have each taken less than 100 ms, so the first-launch shader compile stays covered.

- [ ] **Step 1: Smoke lines.**
  - Add `sim.NewGame` to the console.
  - In `tools/ue_smoke.sh`, pass `-SimNoMenu` for the existing run, and add a second short run *without* it, checking that the log contains `Shell: open MainMenu`. `USimShellSubsystem::Open` logs each open.
  - Also check that `sim.NewGame` resets the day: `sim.AdvanceDays 5, sim.NewGame, sim.Dump` must show day 1.
- [ ] **Step 2: Implement** and run `ue_smoke.sh`: pass. Windowed launch: the menu shows over the dusk-lit city, New Game starts at the gate, and Esc opens pause. Take screenshots. Commit: `A12: boot to the main menu over the city; New Game; the first-launch loading notice`.

---

### Task 8: The third-person camera (A15, DQ1)

**Files:**
- Modify: `unreal/Source/SchizoGame/Public/SimCharacter.h`, `…/Private/SimCharacter.cpp`, `unreal/Config/DefaultInput.ini` (`Aim`=RightMouseButton and Gamepad_LeftTrigger, `ShoulderSwap`=V)
- Test: `…/Private/Tests/SimCameraTest.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct FSimCameraRig { float ArmLength; FVector SocketOffset; };
  static FSimCameraRig ASimCharacter::ComputeCameraRig(bool bAiming, bool bIndoors, bool bRightShoulder);
  // outdoors 340 / indoors 180 / aiming 140; shoulder offset Y = ±45 (±60 when aiming); Z 80 (60 indoors)
  ```

- [ ] **Step 1: Failing test** `Sim.Camera.Rig`: the four combinations give the lengths and offsets above; aiming beats indoors; swapping the shoulder mirrors Y.
- [ ] **Step 2: Implement.**
  - Each tick: `bIndoors` = a line trace 400 cm straight up from the head that hits something (a roof).
  - Interpolate the boom toward the rig with `FMath::FInterpTo` (speed 8).
  - The boom's `bDoCollisionTest` stays true, with a probe size of 12.
  - Aim zooms while held; `ShoulderSwap` toggles the shoulder.
  - Sensitivity and invert-Y come from Task 4.
- [ ] **Step 3:** Build, test. Windowed shots outdoors, in a doorway and while aiming: the camera never clips the walls. Commit: `A15: the third-person camera — interiors shorten the arm, aim zoom, shoulder swap, no clipping`.

---

### Task 9: The batch gate

- [ ] Fresh reviewer (most capable model) with this plan's Review Focus.
- [ ] Fix the Critical and Important findings RED→GREEN; ledger the minors.
- [ ] Full gate: kernel, canon, `ue_test.sh`, `ue_smoke.sh`, reach `--check`, and a windowed screenshot tour of every screen.
- [ ] Update `HANDOFF.md` (the Stage A part 3 section: how to play now; the keys: E use, F eat, G drink, Tab carried, J journal, Esc pause, F5/F9 quick save/load, RMB aim, V shoulder) and `docs/notes-for-tommy.md` (the menus exist; how the UI is built, Slate in C++). Push, then watch CI.
