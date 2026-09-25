// SimUiStyle.h — A8: the look of every screen, in one place (D-023: flat,
// low-fi; warm mudbrick panels, lapis accents, clay ink). No textures.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"

namespace SimUiStyle
{
	inline FLinearColor PanelColor() { return FLinearColor(0.10f, 0.07f, 0.05f, 0.92f); }   // dark mudbrick
	inline FLinearColor ButtonColor() { return FLinearColor(0.28f, 0.18f, 0.11f, 1.f); }   // fired clay
	inline FLinearColor ButtonHover() { return FLinearColor(0.08f, 0.20f, 0.45f, 1.f); }   // lapis
	inline FSlateColor Ink() { return FSlateColor(FLinearColor(0.95f, 0.89f, 0.78f)); }    // limestone
	inline FSlateColor Muted() { return FSlateColor(FLinearColor(0.70f, 0.62f, 0.52f)); }
	inline FSlateColor Accent() { return FSlateColor(FLinearColor(0.93f, 0.72f, 0.30f)); } // gold rim
	/** Text scale from the accessibility setting (1 = designed size). */
	float TextScale();
	inline FSlateFontInfo Font(int32 Size, const FName& Style = TEXT("Regular"))
	{
		return FCoreStyle::GetDefaultFontStyle(Style, FMath::RoundToInt(Size * TextScale()));
	}
	inline FSlateFontInfo Title() { return Font(30, TEXT("Bold")); }
	inline FSlateFontInfo Heading() { return Font(18, TEXT("Bold")); }
	inline FSlateFontInfo Body() { return Font(14); }
	inline FSlateFontInfo Button() { return Font(16); }
	inline FMargin Pad() { return FMargin(24.f, 18.f); }
}
