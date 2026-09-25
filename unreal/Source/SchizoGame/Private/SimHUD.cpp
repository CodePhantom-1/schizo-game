// SimHUD.cpp — words, not bars (game-design's diegetic spirit): need effects
// print as "hungry"/"parched"/"exhausted", not meters.
#include "SimHUD.h"

#include "SchizoGame.h"
#include "SimPlayerController.h"
#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

#include "Engine/Canvas.h"

namespace
{
	// Same fixed shortlist as the Eat/Quaff keys — the C API has no
	// enumerate-inventory call (see SimPlayerController.cpp's note).
	const TCHAR* kShownItems[] = { TEXT("grain"), TEXT("bread"), TEXT("beer") };
}

void ASimHUD::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogSchizoGame, Log, TEXT("SimHUD active."));
}

void ASimHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr)
	{
		return;
	}

	const ASimPlayerController* SimPC = Cast<ASimPlayerController>(PlayerOwner);
	float Y = 40.f;
	const float X = 40.f;

	// The crosshair verb prompt.
	if (SimPC != nullptr)
	{
		const FString Verb = SimPC->GetLookVerbLabel();
		if (!Verb.IsEmpty())
		{
			Canvas->DrawText(GEngine->GetSmallFont(), FString::Printf(TEXT("[E] %s"), *Verb), X, Y, 1.5f, 1.5f);
			Y += 28.f;
		}
	}

	// Need effects as words, not bars.
	if (SimWorld* Handle = USimWorldSubsystem::GetSimHandle())
	{
		char Effects[128];
		if (sim_world_need_effects(Handle, "player", Effects, sizeof(Effects)) > 0)
		{
			FString EffectsLine = FString(UTF8_TO_TCHAR(Effects)).Replace(TEXT(";"), TEXT(", "));
			if (!EffectsLine.IsEmpty())
			{
				Canvas->DrawText(GEngine->GetSmallFont(), EffectsLine, X, Y, 1.2f, 1.2f);
				Y += 24.f;
			}
		}

		// Carried items, only while Tab is held.
		if (SimPC != nullptr && SimPC->IsInventoryShown())
		{
			Y += 10.f;
			// ponytail: no purse getter in the C API yet (D-017 exposes the
			// kernel side only) — "silver" reads via item_count like any
			// other id, so it's 0 until that surface lands.
			const int Silver = sim_world_item_count(Handle, "player", "silver");
			Canvas->DrawText(GEngine->GetSmallFont(), FString::Printf(TEXT("Silver: %d"), FMath::Max(Silver, 0)), X, Y, 1.2f, 1.2f);
			Y += 22.f;
			for (const TCHAR* Item : kShownItems)
			{
				const FTCHARToUTF8 Utf8Item(Item);
				const int Count = sim_world_item_count(Handle, "player", Utf8Item.Get());
				if (Count > 0)
				{
					Canvas->DrawText(GEngine->GetSmallFont(), FString::Printf(TEXT("%s: %d"), Item, Count), X, Y, 1.2f, 1.2f);
					Y += 22.f;
				}
			}
		}
	}
}
