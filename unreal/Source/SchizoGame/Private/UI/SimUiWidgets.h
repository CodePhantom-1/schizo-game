// SimUiWidgets.h — A8: the few building blocks every screen is made of, in the
// D-023 style (SimUiStyle.h). Buttons are focusable (keyboard and gamepad).
#pragma once

#include "CoreMinimal.h"
#include "UI/SimUiStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace SimUi
{
	inline TSharedRef<SWidget> Text(const FText& T, const FSlateFontInfo& Font, const FSlateColor& Colour, float Wrap = 0.f)
	{
		return SNew(STextBlock).Text(T).Font(Font).ColorAndOpacity(Colour).WrapTextAt(Wrap);
	}
	inline TSharedRef<SWidget> Title(const FText& T) { return Text(T, SimUiStyle::Title(), SimUiStyle::Accent()); }
	inline TSharedRef<SWidget> Heading(const FText& T) { return Text(T, SimUiStyle::Heading(), SimUiStyle::Ink()); }
	inline TSharedRef<SWidget> Body(const FText& T, float Wrap = 0.f) { return Text(T, SimUiStyle::Body(), SimUiStyle::Ink(), Wrap); }
	inline TSharedRef<SWidget> Muted(const FText& T, float Wrap = 0.f) { return Text(T, SimUiStyle::Body(), SimUiStyle::Muted(), Wrap); }

	inline TSharedRef<SWidget> MakeButton(const FText& T, TFunction<void()> OnClick, bool bEnabled, const FSlateFontInfo& Font, FMargin Pad)
	{
		return SNew(SButton)
			.IsEnabled(bEnabled)
			.ButtonColorAndOpacity(SimUiStyle::ButtonColor())
			.ContentPadding(Pad)
			.HAlign(HAlign_Center)
			.IsFocusable(true)
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(T).Font(Font).ColorAndOpacity(bEnabled ? SimUiStyle::Ink() : SimUiStyle::Muted())
			];
	}
	/** A menu button: full width, large. */
	inline TSharedRef<SWidget> Button(const FText& T, TFunction<void()> OnClick, bool bEnabled = true)
	{
		return SNew(SBox).MinDesiredWidth(320.f).Padding(0.f, 3.f)[MakeButton(T, MoveTemp(OnClick), bEnabled, SimUiStyle::Button(), FMargin(18.f, 8.f))];
	}
	/** A row button (Load, Save over, …). */
	inline TSharedRef<SWidget> SmallButton(const FText& T, TFunction<void()> OnClick, bool bEnabled = true)
	{
		return SNew(SBox).Padding(4.f, 0.f)[MakeButton(T, MoveTemp(OnClick), bEnabled, SimUiStyle::Body(), FMargin(10.f, 4.f))];
	}

	/** Two presses: the first asks, the second acts (Delete). */
	class SConfirmButton : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SConfirmButton) {}
			SLATE_ARGUMENT(FText, Label)
			SLATE_ARGUMENT(FText, Confirm)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args, TFunction<void()> InOnConfirmed)
		{
			Label = Args._Label;
			Confirm = Args._Confirm;
			OnConfirmed = MoveTemp(InOnConfirmed);
			ChildSlot.Padding(4.f, 0.f)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(SimUiStyle::ButtonColor())
				.ContentPadding(FMargin(10.f, 4.f))
				.OnClicked_Lambda([this]()
				{
					if (bArmed)
					{
						OnConfirmed();
					}
					bArmed = !bArmed;
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Font(SimUiStyle::Body()).ColorAndOpacity(SimUiStyle::Ink())
					.Text_Lambda([this]() { return bArmed ? Confirm : Label; })
				]
			];
		}

	private:
		FText Label;
		FText Confirm;
		TFunction<void()> OnConfirmed;
		bool bArmed = false;
	};
	inline TSharedRef<SWidget> ConfirmButton(const FText& T, const FText& Confirm, TFunction<void()> OnConfirmed)
	{
		return SNew(SConfirmButton, MoveTemp(OnConfirmed)).Label(T).Confirm(Confirm);
	}

	/** A labelled row: label on the left, control on the right. */
	inline TSharedRef<SWidget> Row(const FText& Label, TSharedRef<SWidget> Control)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.f, 4.f)[Body(Label)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(300.f)[Control]];
	}

	inline TSharedRef<SWidget> Check(const FText& Label, TFunction<bool()> Get, TFunction<void(bool)> Set)
	{
		return Row(Label, SNew(SCheckBox)
			.IsChecked_Lambda([Get]() { return Get() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Set](ECheckBoxState S) { Set(S == ECheckBoxState::Checked); }));
	}

	/** A slider with its value shown (Format turns the value into text). */
	inline TSharedRef<SWidget> Slider(const FText& Label, float Min, float Max, float Step, TFunction<float()> Get,
		TFunction<void(float)> Set, TFunction<FText(float)> Format)
	{
		return Row(Label, SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SSlider).MinValue(Min).MaxValue(Max).StepSize(Step).MouseUsesStep(true)
				.Value_Lambda([Get]() { return Get(); })
				.OnValueChanged_Lambda([Set](float V) { Set(V); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(SimUiStyle::Body()).ColorAndOpacity(SimUiStyle::Ink())
				.Text_Lambda([Get, Format]() { return Format(Get()); })
			]);
	}

	/** A choice cycled with ‹ › (gamepad-friendly; no dropdown). */
	inline TSharedRef<SWidget> Choice(const FText& Label, TArray<FText> Options, TFunction<int32()> Get, TFunction<void(int32)> Set)
	{
		const int32 N = FMath::Max(1, Options.Num());
		return Row(Label, SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[SmallButton(FText::FromString(TEXT("‹")), [Get, Set, N]() { Set((Get() + N - 1) % N); })]
			+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(SimUiStyle::Body()).ColorAndOpacity(SimUiStyle::Ink())
				.Text_Lambda([Options, Get]() { const int32 I = Get(); return Options.IsValidIndex(I) ? Options[I] : FText::GetEmpty(); })
			]
			+ SHorizontalBox::Slot().AutoWidth()[SmallButton(FText::FromString(TEXT("›")), [Get, Set, N]() { Set((Get() + 1) % N); })]);
	}
}
