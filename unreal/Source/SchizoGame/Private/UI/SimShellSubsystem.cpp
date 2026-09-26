// SimShellSubsystem.cpp — A8: see the header.
#include "UI/SimShellSubsystem.h"

#include "SchizoGame.h"
#include "SimGameUserSettings.h"
#include "SimGameInstanceSubsystem.h"
#include "SimPlayerController.h"
#include "SimWorldSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "UI/SimScreens.h"
#include "UI/SimUiStyle.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

float SimUiStyle::TextScale()
{
	const USimGameUserSettings* Settings = USimGameUserSettings::Get();
	return Settings ? Settings->SubtitleScale : 1.f;
}

/** Holds one screen: takes focus, closes the top screen on Esc / gamepad B. */
class SSimScreenFrame : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSimScreenFrame) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, USimShellSubsystem* InShell)
	{
		Shell = InShell;
		ChildSlot
		[
			SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(SimUiStyle::PanelColor())
				.Padding(SimUiStyle::Pad())
				[
					InArgs._Content.Widget
				]
			]
		];
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Key) override
	{
		if ((Key.GetKey() == EKeys::Escape || Key.GetKey() == EKeys::Gamepad_FaceButton_Right) && Shell.IsValid())
		{
			Shell->CloseTop();
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

private:
	TWeakObjectPtr<USimShellSubsystem> Shell;
};

/** Toasts in the top middle; never takes focus or input. */
class SSimToastOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSimToastOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, USimShellSubsystem* InShell)
	{
		Shell = InShell;
		SetVisibility(EVisibility::HitTestInvisible);
		ChildSlot.HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 90.f)
		[
			SAssignNew(Lines, STextBlock)
			.Font(SimUiStyle::Heading())
			.ColorAndOpacity(SimUiStyle::Accent())
			.ShadowOffset(FVector2D(1.f, 1.f))
			.Justification(ETextJustify::Center)
		];
	}

	virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override
	{
		SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
		if (!Shell.IsValid())
		{
			return;
		}
		FSimToastQueue& Queue = Shell->Toasts();
		Queue.Tick(DeltaTime);
		TArray<FText> Texts;
		for (const FSimToastQueue::FItem& Item : Queue.Items())
		{
			Texts.Add(Item.Text);
		}
		Lines->SetText(FText::Join(FText::FromString(TEXT("\n")), Texts));
	}

private:
	TWeakObjectPtr<USimShellSubsystem> Shell;
	TSharedPtr<STextBlock> Lines;
};

const TCHAR* USimShellSubsystem::ScreenName(ESimScreen Screen)
{
	switch (Screen)
	{
	case ESimScreen::MainMenu: return TEXT("MainMenu");
	case ESimScreen::Pause: return TEXT("Pause");
	case ESimScreen::SaveLoad: return TEXT("SaveLoad");
	case ESimScreen::Settings: return TEXT("Settings");
	case ESimScreen::Journal: return TEXT("Journal");
	case ESimScreen::Tablet: return TEXT("Tablet");
	case ESimScreen::Loading: return TEXT("Loading");
	}
	return TEXT("?");
}

USimShellSubsystem* USimShellSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	return LP ? LP->GetSubsystem<USimShellSubsystem>() : nullptr;
}

void USimShellSubsystem::Deinitialize()
{
	if (GEngine && GEngine->GameViewport)
	{
		if (Shown.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(Shown.ToSharedRef());
		if (ToastOverlay.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(ToastOverlay.ToSharedRef());
	}
	Shown.Reset();
	ToastOverlay.Reset();
	Super::Deinitialize();
}

void USimShellSubsystem::Open(ESimScreen Screen)
{
	UE_LOG(LogSchizoGame, Log, TEXT("Shell: open %s"), ScreenName(Screen));
	Stack.Push(Screen);
	ShowTop();
}

void USimShellSubsystem::CloseTop()
{
	const TOptional<ESimScreen> Top = Stack.Top();
	if (!Top.IsSet() || Top.GetValue() == ESimScreen::MainMenu)
	{
		return;
	}
	Stack.Pop();
	ShowTop();
}

void USimShellSubsystem::Close(ESimScreen Screen)
{
	if (Stack.PopIf(Screen))
	{
		ShowTop();
	}
}

void USimShellSubsystem::CloseAll()
{
	Stack.Clear();
	ShowTop();
}

void USimShellSubsystem::OnBack()
{
	if (Stack.IsEmpty())
	{
		Open(ESimScreen::Pause);
	}
	else
	{
		CloseTop();
	}
}

void USimShellSubsystem::Toast(const FText& Text, float Seconds)
{
	UE_LOG(LogSchizoGame, Log, TEXT("Shell: toast %s"), *Text.ToString());
	ToastQueue.Add(Text, Seconds);
	EnsureToastOverlay();
}

void USimShellSubsystem::ShowTablet(const FText& Title, const FText& Body)
{
	TabletTitle = Title;
	TabletBody = Body;
	Stack.PopIf(ESimScreen::Tablet);  // a new tablet replaces the one being read
	Open(ESimScreen::Tablet);
}

void USimShellSubsystem::ShowTop()
{
	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (Viewport != nullptr && Shown.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Shown.ToSharedRef());
	}
	Shown.Reset();
	const TOptional<ESimScreen> Top = Stack.Top();
	if (Viewport != nullptr && Top.IsSet())
	{
		Shown = SNew(SSimScreenFrame, this)[MakeScreen(Top.GetValue(), *this)];
		Viewport->AddViewportWidgetContent(Shown.ToSharedRef(), 10);
	}
	ApplyInputAndPause();
}

void USimShellSubsystem::ApplyInputAndPause()
{
	ULocalPlayer* LP = GetLocalPlayer();
	UWorld* World = LP ? LP->GetWorld() : nullptr;
	APlayerController* PC = World ? LP->GetPlayerController(World) : nullptr;
	if (World != nullptr)
	{
		UGameplayStatics::SetGamePaused(World, Stack.WantsPause());
	}
	if (PC == nullptr)
	{
		return;
	}
	if (Stack.BlocksGameInput() && Shown.IsValid())
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Shown);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetKeyboardFocus(Shown, EFocusCause::SetDirectly);
		}
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}

void USimShellSubsystem::EnsureToastOverlay()
{
	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (ToastOverlay.IsValid() || Viewport == nullptr)
	{
		return;
	}
	ToastOverlay = SNew(SSimToastOverlay, this);
	Viewport->AddViewportWidgetContent(ToastOverlay.ToSharedRef(), 20);
}

void USimShellSubsystem::StartNewGame()
{
	ULocalPlayer* LP = GetLocalPlayer();
	UWorld* World = LP ? LP->GetWorld() : nullptr;
	USimGameInstanceSubsystem* Owner = USimGameInstanceSubsystem::Get(World);
	if (Owner == nullptr || !Owner->NewWorld(static_cast<uint64>(FMath::Rand()) + 1))
	{
		Toast(NSLOCTEXT("SimUi", "NewGameFailed", "A new game could not start (see the log)."));
		return;
	}
	USimWorldSubsystem::ResetClockToStartFor(World);
	if (USimGameUserSettings* Settings = USimGameUserSettings::Get())
	{
		Settings->ApplySimSettings(World);
	}
	// The player back at the start: the gate today; the prisoner barracks with Stage AA6.
	APlayerController* PC = LP->GetPlayerController(World);
	if (PC != nullptr && World->GetAuthGameMode() != nullptr)
	{
		if (AActor* Start = World->GetAuthGameMode()->FindPlayerStart(PC))
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				Pawn->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation(), false, true);
			}
			PC->SetControlRotation(Start->GetActorRotation());
		}
		if (ASimPlayerController* SimPC = Cast<ASimPlayerController>(PC))
		{
			SimPC->ResetNeedsClock();
		}
	}
	UE_LOG(LogSchizoGame, Log, TEXT("Shell: new game"));
	CloseAll();
	Toast(NSLOCTEXT("SimUi", "NewGame", "You arrive in chains at the City of the Moon."), 5.f);
}
