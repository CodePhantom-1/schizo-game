// SimHud.cpp — words and numbers from the kernel, nothing invented: needs,
// named conditions, the purse and the carried goods all read through
// sim/CApi.h on the frame they are drawn. The street's clock stays where it
// is (SimGameMode's on-screen line).
#include "SimHud.h"

#include "SchizoGame.h"
#include "SimPlayerController.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace
{
	// Readable over bright sky and dark night alike.
	FFontRenderInfo ShadowText()
	{
		FFontRenderInfo Info;
		Info.bEnableShadow = true;
		return Info;
	}
}

void ASimHud::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogSchizoGame, Log, TEXT("SimHud active."));
}

void ASimHud::DrawNeedRow(const FString& Label, int Value, float X, float& Y)
{
	// Value is the kernel's 0..100 (higher = worse for every need but health);
	// clamp the drawing, not the data.
	const float Fill = FMath::Clamp(static_cast<float>(Value), 0.f, 100.f) / 100.f;
	const float BarWidth = 120.f;
	const float BarHeight = 10.f;
	const float LabelWidth = 90.f;

	Canvas->DrawText(GEngine->GetSmallFont(), Label, X, Y, 1.2f, 1.2f, ShadowText());
	DrawRect(FLinearColor::Black, X + LabelWidth, Y + 2.f, BarWidth, BarHeight);
	// Health reads the other way (full is good); the bar mirrors the number.
	const FLinearColor FillColor = Label == TEXT("Health") ? FLinearColor(0.2f, 0.5f, 0.2f)
	                                                       : FLinearColor(0.6f, 0.4f, 0.15f);
	DrawRect(FillColor, X + LabelWidth, Y + 2.f, BarWidth * Fill, BarHeight);
	Canvas->DrawText(GEngine->GetSmallFont(), FString::Printf(TEXT("%d"), Value),
		X + LabelWidth + BarWidth + 10.f, Y, 1.2f, 1.2f, ShadowText());
	Y += 22.f;
}

void ASimHud::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr)
	{
		return;  // headless (-nullrhi): nothing to draw on, nothing to read
	}

	const float X = 40.f;
	float Y = 40.f;
	const ASimPlayerController* SimPC = Cast<ASimPlayerController>(PlayerOwner);

	// The clock, top right: day, season and hour from the kernel.
	{
		const int64 Day = USimWorldSubsystem::GetSimDayFor(GetWorld());
		const float Hour = USimWorldSubsystem::GetSimHourFor(GetWorld());
		if (Day >= 0 && Hour >= 0.f)
		{
			const int32 H = FMath::Clamp(FMath::FloorToInt(Hour), 0, 23);
			const int32 M = FMath::Clamp(FMath::FloorToInt((Hour - H) * 60.f), 0, 59);
			int32 Year = 1, Month = 1, Dom = 1;
			USimWorldSubsystem::GetSimDateFor(GetWorld(), Year, Month, Dom);
			const FString MonthName = USimWorldSubsystem::GetSimMonthNameFor(GetWorld());
			const FText Line1 = FText::Format(NSLOCTEXT("SimHud", "ClockLine", "{0} {1}, year {2}  -  {3}  -  {4}"),
				FText::AsNumber(Dom),
				MonthName.IsEmpty() ? FText::AsNumber(Month) : FText::FromString(MonthName),
				FText::AsNumber(Year),
				FText::FromString(USimWorldSubsystem::GetSimSeasonFor(GetWorld())),
				FText::FromString(FString::Printf(TEXT("%02d:%02d"), H, M)));

			const FString Phase = USimWorldSubsystem::GetSimMoonPhaseFor(GetWorld());
			const FString Omen = USimWorldSubsystem::GetSimDayOmenFor(GetWorld());
			const FString Observance = USimWorldSubsystem::GetSimDayObservanceFor(GetWorld());
			FText MoonText = NSLOCTEXT("SimHud", "MoonWaning", "Waning moon");
			if (Phase == TEXT("new")) MoonText = NSLOCTEXT("SimHud", "MoonNew", "New crescent");
			else if (Phase == TEXT("waxing")) MoonText = NSLOCTEXT("SimHud", "MoonWaxing", "Waxing moon");
			else if (Phase == TEXT("full")) MoonText = NSLOCTEXT("SimHud", "MoonFull", "Full moon");
			FText Line2 = MoonText;
			if (Omen == TEXT("favourable")) Line2 = FText::Format(NSLOCTEXT("SimHud", "Favourable", "{0}  -  a favourable day"), Line2);
			else if (Omen == TEXT("unfavourable")) Line2 = FText::Format(NSLOCTEXT("SimHud", "Unfavourable", "{0}  -  an unfavourable day"), Line2);
			if (!Observance.IsEmpty()) Line2 = FText::Format(NSLOCTEXT("SimHud", "Observance", "{0}  -  {1}"), Line2, FText::FromString(Observance));

			float LineY = 36.f;
			for (const FText& Line : {Line1, Line2})
			{
				float W = 0.f, Hh = 0.f;
				Canvas->StrLen(GEngine->GetMediumFont(), Line.ToString(), W, Hh);
				FCanvasTextItem Item(FVector2D(Canvas->ClipX - W - 40.f, LineY), Line,
					GEngine->GetMediumFont(), FLinearColor(0.95f, 0.9f, 0.8f));
				Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f), FVector2D(1.f, 1.f));
				Canvas->DrawItem(Item);
				LineY += Hh + 4.f;
			}
		}
	}

	// The crosshair's verb prompt: what the Use key would do right now.
	if (SimPC != nullptr)
	{
		const FString Verb = SimPC->GetLookVerbLabel();
		if (!Verb.IsEmpty())
		{
			Canvas->DrawText(GEngine->GetSmallFont(), FString::Printf(TEXT("[E] %s"), *Verb),
				X, Y, 1.5f, 1.5f, ShadowText());
			Y += 30.f;
		}
	}

	// The body, from the kernel (needs and health; -1 means "no world yet").
	if (SimWorld* Handle = USimWorldSubsystem::GetSimHandle())
	{
		Y += 10.f;
		const int Hunger = sim_world_hunger(Handle, "player");
		const int Thirst = sim_world_thirst(Handle, "player");
		const int Fatigue = sim_world_fatigue(Handle, "player");
		const int Health = sim_world_health(Handle, "player");
		DrawNeedRow(TEXT("Hunger"), Hunger, X, Y);
		DrawNeedRow(TEXT("Thirst"), Thirst, X, Y);
		DrawNeedRow(TEXT("Fatigue"), Fatigue, X, Y);
		DrawNeedRow(TEXT("Health"), Health, X, Y);

		// The named conditions at the kernel's own thresholds ("hungry;parched").
		char Effects[128] = {};
		if (sim_world_need_effects(Handle, "player", Effects, sizeof(Effects)) > 0)
		{
			const FString EffectsLine = FString(UTF8_TO_TCHAR(Effects)).Replace(TEXT(";"), TEXT(", "));
			if (!EffectsLine.IsEmpty())
			{
				Canvas->DrawText(GEngine->GetSmallFont(), EffectsLine, X, Y, 1.2f, 1.2f, ShadowText());
				Y += 22.f;
			}
		}

		// The purse, weighed in silver grains (CApi's sim_world_purse).
		Canvas->DrawText(GEngine->GetSmallFont(),
			FString::Printf(TEXT("Silver %lld"), sim_world_purse(Handle, "player")),
			X, Y, 1.2f, 1.2f, ShadowText());
		Y += 26.f;

		// Carried goods while Tab is held: the kernel's own inventory list
		// ("beer:2;grain:3"), not a shortlist.
		if (SimPC != nullptr && SimPC->IsInventoryShown())
		{
			Canvas->DrawText(GEngine->GetSmallFont(), TEXT("Carried"), X, Y, 1.2f, 1.2f, ShadowText());
			Y += 22.f;
			char Inventory[512] = {};
			if (sim_world_inventory(Handle, "player", Inventory, sizeof(Inventory)) > 0)
			{
				TArray<FString> Records;
				FString(UTF8_TO_TCHAR(Inventory)).ParseIntoArray(Records, TEXT(";"), true);
				for (const FString& Record : Records)
				{
					FString Item;
					FString Count;
					if (Record.Split(TEXT(":"), &Item, &Count))
					{
						Canvas->DrawText(GEngine->GetSmallFont(),
							FString::Printf(TEXT("%s x %s"), *Item, *Count), X + 16.f, Y, 1.2f, 1.2f, ShadowText());
						Y += 20.f;
					}
				}
			}
			else
			{
				Canvas->DrawText(GEngine->GetSmallFont(), TEXT("nothing"), X + 16.f, Y, 1.2f, 1.2f, ShadowText());
				Y += 20.f;
			}
		}
	}
}
