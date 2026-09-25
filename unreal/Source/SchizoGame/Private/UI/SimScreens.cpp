// SimScreens.cpp — A8: the screen factory (each screen: Task 5 of part 3).
#include "UI/SimScreens.h"

#include "UI/SimShellSubsystem.h"
#include "UI/SimUiStyle.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> MakeScreen(ESimScreen Screen, USimShellSubsystem& /*Shell*/)
{
	return SNew(STextBlock).Text(FText::FromString(USimShellSubsystem::ScreenName(Screen))).Font(SimUiStyle::Title()).ColorAndOpacity(SimUiStyle::Ink());
}
