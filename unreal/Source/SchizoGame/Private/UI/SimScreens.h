// SimScreens.h — A8: builds one screen's widget. Every screen sits in
// SSimScreenFrame, which takes keyboard focus and closes the top screen on
// Esc / gamepad B.
#pragma once

#include "CoreMinimal.h"
#include "UI/SimScreenStack.h"
#include "Widgets/SWidget.h"

class USimShellSubsystem;

TSharedRef<SWidget> MakeScreen(ESimScreen Screen, USimShellSubsystem& Shell);
