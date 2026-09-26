// SimRebind.cpp — A9: the Controls tab's key list (rebinding: part 3 Task 6).
#include "UI/SimScreens.h"

#include "UI/SimUiWidgets.h"

#include "GameFramework/InputSettings.h"
#include "Widgets/SBoxPanel.h"

TSharedRef<SWidget> MakeRebindList()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	const UInputSettings* Input = GetDefault<UInputSettings>();
	for (const FInputActionKeyMapping& M : Input->GetActionMappings())
	{
		Box->AddSlot().AutoHeight()[SimUi::Row(FText::FromName(M.ActionName), SimUi::Body(M.Key.GetDisplayName()))];
	}
	for (const FInputAxisKeyMapping& M : Input->GetAxisMappings())
	{
		Box->AddSlot().AutoHeight()[SimUi::Row(FText::FromName(M.AxisName), SimUi::Body(M.Key.GetDisplayName()))];
	}
	return Box;
}
