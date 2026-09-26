// SimScreens.cpp — A8/A10/A12: every screen but Settings (SimScreenSettings.cpp).
// Built in C++ Slate; each sits in SSimScreenFrame (Esc / gamepad B closes).
#include "UI/SimScreens.h"

#include "SimQueryLibrary.h"
#include "SimSaves.h"
#include "SimWorldSubsystem.h"
#include "UI/SimShellSubsystem.h"
#include "UI/SimUiStyle.h"
#include "UI/SimUiWidgets.h"

#include "Engine/LocalPlayer.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SimUi"

namespace
{
	UObject* Ctx(USimShellSubsystem& Shell) { return Shell.GetLocalPlayer(); }

	TSharedRef<SWidget> MainMenu(USimShellSubsystem& Shell)
	{
		TWeakObjectPtr<USimShellSubsystem> W(&Shell);
		const FString Continue = SimSaves::MostRecent();
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[SimUi::Title(LOCTEXT("GameTitle", "The City of the Moon"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)[SimUi::Muted(LOCTEXT("GameSubtitle", "A prisoner in a strange land"))]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Button(LOCTEXT("Continue", "Continue"), [W, Continue]
				{
					if (W.IsValid() && SimSaves::Load(Ctx(*W), Continue))
					{
						W->CloseAll();
						W->Toast(LOCTEXT("Loaded", "Loaded."));
					}
				}, !Continue.IsEmpty())]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Button(LOCTEXT("NewGame", "New Game"), [W] { if (W.IsValid()) W->StartNewGame(); })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Button(LOCTEXT("Load", "Load"), [W] { if (W.IsValid()) W->Open(ESimScreen::SaveLoad); })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Button(LOCTEXT("Settings", "Settings"), [W] { if (W.IsValid()) W->Open(ESimScreen::Settings); })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Button(LOCTEXT("Quit", "Quit"), [] { FGenericPlatformMisc::RequestExit(false); })];
	}

	TSharedRef<SWidget> PauseMenu(USimShellSubsystem& Shell)
	{
		TWeakObjectPtr<USimShellSubsystem> W(&Shell);
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)[SimUi::Title(LOCTEXT("Paused", "Paused"))]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Button(LOCTEXT("Resume", "Resume"), [W] { if (W.IsValid()) W->CloseTop(); })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Button(LOCTEXT("SaveLoad", "Save / Load"), [W] { if (W.IsValid()) W->Open(ESimScreen::SaveLoad); })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Button(LOCTEXT("Journal", "Journal"), [W] { if (W.IsValid()) W->Open(ESimScreen::Journal); })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Button(LOCTEXT("Settings", "Settings"), [W] { if (W.IsValid()) W->Open(ESimScreen::Settings); })]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Button(LOCTEXT("MainMenu", "Main Menu"), [W]
				{
					if (W.IsValid())
					{
						W->CloseAll();
						W->Open(ESimScreen::MainMenu);
					}
				})]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Button(LOCTEXT("Quit", "Quit"), [] { FGenericPlatformMisc::RequestExit(false); })];
	}

	FText SlotLine(const FSimSlotInfo& S)
	{
		const int32 H = FMath::Clamp(FMath::FloorToInt(S.Hour), 0, 23);
		const int32 M = FMath::Clamp(FMath::FloorToInt((S.Hour - H) * 60.f), 0, 59);
		return FText::Format(LOCTEXT("SlotLine", "{0}  —  {1} {2}, {3}  —  saved {4}"),
			FText::FromString(S.Label), FText::AsNumber(S.DayOfMonth), FText::FromString(S.MonthName),
			FText::FromString(FString::Printf(TEXT("%02d:%02d"), H, M)), FText::AsDateTime(S.SavedAt));
	}

	TSharedRef<SWidget> SaveLoad(USimShellSubsystem& Shell)
	{
		TWeakObjectPtr<USimShellSubsystem> W(&Shell);
		const bool bInGame = !Shell.IsOpen(ESimScreen::MainMenu);  // saving needs a game in progress
		TSharedRef<SScrollBox> List = SNew(SScrollBox);
		const TArray<FSimSlotInfo> Slots = SimSaves::List();
		if (Slots.Num() == 0)
		{
			List->AddSlot()[SimUi::Muted(LOCTEXT("NoSaves", "No saved games yet."))];
		}
		for (const FSimSlotInfo& S : Slots)
		{
			const FString Slot = S.Slot;
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SimUi::Body(SlotLine(S))]
				+ SHorizontalBox::Slot().AutoWidth()[SimUi::SmallButton(LOCTEXT("LoadSlot", "Load"), [W, Slot]
					{
						if (W.IsValid() && SimSaves::Load(Ctx(*W), Slot))
						{
							W->CloseAll();
							W->Toast(LOCTEXT("Loaded", "Loaded."));
						}
					})];
			if (bInGame)
			{
				Row->AddSlot().AutoWidth()[SimUi::SmallButton(LOCTEXT("SaveOver", "Save over"), [W, Slot, Label = S.Label]
					{
						if (W.IsValid() && SimSaves::Save(Ctx(*W), Slot, Label))
						{
							W->Toast(LOCTEXT("Saved", "Saved."));
							W->Refresh();
						}
					})];
			}
			Row->AddSlot().AutoWidth()[SimUi::ConfirmButton(LOCTEXT("Delete", "Delete"), LOCTEXT("ReallyDelete", "Really delete?"), [W, Slot]
				{
					SimSaves::Delete(Slot);
					if (W.IsValid()) W->Refresh();
				})];
			List->AddSlot().Padding(0, 3)[Row];
		}

		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[SimUi::Title(bInGame ? LOCTEXT("SaveLoadTitle", "Save / Load") : LOCTEXT("LoadTitle", "Load"))];
		if (bInGame)
		{
			TSharedPtr<SEditableTextBox> Name;
			Box->AddSlot().AutoHeight().Padding(0, 0, 0, 10)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[SAssignNew(Name, SEditableTextBox).Font(SimUiStyle::Body())
					.HintText(LOCTEXT("SaveNameHint", "Name this save (optional)"))]
				+ SHorizontalBox::Slot().AutoWidth()[SimUi::SmallButton(LOCTEXT("NewSave", "New save"), [W, Name]
					{
						if (!W.IsValid()) return;
						const FString Slot = FString::Printf(TEXT("slot_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
						if (SimSaves::Save(Ctx(*W), Slot, Name.IsValid() ? Name->GetText().ToString().TrimStartAndEnd() : FString()))
						{
							W->Toast(LOCTEXT("Saved", "Saved."));
							W->Refresh();
						}
					})]
			];
		}
		Box->AddSlot().FillHeight(1.f)[SNew(SBox).MinDesiredWidth(760.f).MaxDesiredHeight(420.f)[List]];
		Box->AddSlot().AutoHeight().Padding(0, 12, 0, 0)[SimUi::Button(LOCTEXT("Back", "Back"), [W] { if (W.IsValid()) W->CloseTop(); })];
		return Box;
	}

	TSharedRef<SWidget> Journal(USimShellSubsystem& Shell)
	{
		TWeakObjectPtr<USimShellSubsystem> W(&Shell);
		SimWorld* Handle = USimWorldSubsystem::GetSimHandleFor(Ctx(Shell));
		TSharedRef<SScrollBox> List = SNew(SScrollBox);
		const TPair<ESimQuestList, FText> Sections[] = {
			{ESimQuestList::Active, LOCTEXT("Active", "Under way")},
			{ESimQuestList::Completed, LOCTEXT("Completed", "Done")},
			{ESimQuestList::Failed, LOCTEXT("Failed", "Failed or given up")}};
		int32 Shown = 0;
		for (const TPair<ESimQuestList, FText>& Section : Sections)
		{
			const TArray<FSimQuestInfo> Quests = SimQuery::GetQuests(Handle, Section.Key);
			if (Quests.Num() == 0) continue;
			List->AddSlot().Padding(0, 10, 0, 4)[SimUi::Heading(Section.Value)];
			for (const FSimQuestInfo& Q : Quests)
			{
				++Shown;
				FText Head = Q.Giver.IsEmpty()
					? FText::FromString(Q.Title)
					: FText::Format(LOCTEXT("QuestFrom", "{0}  —  from {1}"), FText::FromString(Q.Title), FText::FromString(Q.Giver));
				if (Q.Deadline > 0)
				{
					Head = FText::Format(LOCTEXT("QuestDue", "{0}  (by day {1})"), Head, FText::AsNumber(Q.Deadline));
				}
				List->AddSlot().Padding(8, 2)[SimUi::Body(Head)];
				for (const FSimJournalEntry& E : SimQuery::GetJournal(Handle, Q.Id))
				{
					const FText Line = E.Text.IsEmpty()
						? FText::Format(LOCTEXT("EntryNoText", "Day {0}: {1}"), FText::AsNumber(E.Day), FText::FromString(E.Stage))
						: FText::Format(LOCTEXT("Entry", "Day {0}: {1}"), FText::AsNumber(E.Day), FText::FromString(E.Text));
					List->AddSlot().Padding(28, 0)[SimUi::Muted(Line)];
				}
			}
		}
		if (Shown == 0)
		{
			List->AddSlot()[SimUi::Muted(LOCTEXT("NoQuests", "Nothing written yet. People with problems will find you."))];
		}
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SimUi::Title(LOCTEXT("JournalTitle", "Journal"))]
			+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SBox).MinDesiredWidth(760.f).MaxDesiredHeight(480.f)[List]]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)[SimUi::Button(LOCTEXT("Close", "Close"), [W] { if (W.IsValid()) W->CloseTop(); })];
	}

	TSharedRef<SWidget> Tablet(USimShellSubsystem& Shell)
	{
		TWeakObjectPtr<USimShellSubsystem> W(&Shell);
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[SimUi::Heading(Shell.GetTabletTitle())]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SBox).MinDesiredWidth(560.f).MaxDesiredHeight(420.f)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[SimUi::Body(Shell.GetTabletBody(), 560.f)]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)[SimUi::Button(LOCTEXT("Close", "Close"), [W] { if (W.IsValid()) W->CloseTop(); })];
	}

	TSharedRef<SWidget> Loading(USimShellSubsystem&)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[SimUi::Title(LOCTEXT("Preparing", "Preparing the city…"))]
			+ SVerticalBox::Slot().AutoHeight()[SimUi::Muted(LOCTEXT("ShaderNote",
				"The first start after an update compiles the city's shaders. It can take 10–15 minutes and the picture may stall: this is not a hang."), 560.f)];
	}
}

TSharedRef<SWidget> MakeScreen(ESimScreen Screen, USimShellSubsystem& Shell)
{
	switch (Screen)
	{
	case ESimScreen::MainMenu: return MainMenu(Shell);
	case ESimScreen::Pause: return PauseMenu(Shell);
	case ESimScreen::SaveLoad: return SaveLoad(Shell);
	case ESimScreen::Settings: return MakeSettingsScreen(Shell);
	case ESimScreen::Journal: return Journal(Shell);
	case ESimScreen::Tablet: return Tablet(Shell);
	case ESimScreen::Loading: return Loading(Shell);
	}
	return Loading(Shell);
}

#undef LOCTEXT_NAMESPACE
