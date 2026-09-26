// SimSaves.cpp — A10: see the header.
#include "SimSaves.h"

#include "SchizoGame.h"
#include "SimGameInstanceSubsystem.h"
#include "SimPlayerController.h"
#include "SimGameUserSettings.h"
#include "SimNpcDirector.h"
#include "UI/SimShellSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "SimWorldSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* IndexSlot = TEXT("sim_slot_index");
	constexpr int64 MaxSaveBytes = 50ll * 1024 * 1024;  // architecture §5 budget

	USimSlotIndex* ReadIndex()
	{
		if (USimSlotIndex* Index = Cast<USimSlotIndex>(UGameplayStatics::LoadGameFromSlot(IndexSlot, 0)))
		{
			return Index;
		}
		return Cast<USimSlotIndex>(UGameplayStatics::CreateSaveGameObject(USimSlotIndex::StaticClass()));
	}

	void Tell(UObject* Ctx, const FText& Text)
	{
		if (USimShellSubsystem* Shell = USimShellSubsystem::Get(Ctx))
		{
			Shell->Toast(Text);
		}
	}

	APlayerController* PlayerOf(UObject* Ctx)
	{
		UWorld* World = Ctx ? Ctx->GetWorld() : nullptr;
		return World ? World->GetFirstPlayerController() : nullptr;
	}
}

namespace SimSaves
{
	bool Detail::WriteSlot(const FString& Slot, USimSaveGame* Game)
	{
		if (Game == nullptr || Slot.IsEmpty() || Slot == IndexSlot)
		{
			return false;
		}
		TArray<uint8> Bytes;
		if (!UGameplayStatics::SaveGameToMemory(Game, Bytes))
		{
			return false;
		}
		UE_LOG(LogSchizoGame, Log, TEXT("Saved slot %s (%.1f KB)"), *Slot, Bytes.Num() / 1024.0);
		if (Bytes.Num() > MaxSaveBytes)
		{
			UE_LOG(LogSchizoGame, Error, TEXT("Save %s is %.1f MB — over the 50 MB budget (architecture §5)."), *Slot, Bytes.Num() / (1024.0 * 1024.0));
		}
		if (!UGameplayStatics::SaveDataToSlot(Bytes, Slot, 0))
		{
			return false;
		}
		USimSlotIndex* Index = ReadIndex();
		Index->Slots.RemoveAll([&](const FSimSlotInfo& S) { return S.Slot == Slot; });
		FSimSlotInfo& Info = Index->Slots.AddDefaulted_GetRef();
		Info.Slot = Slot;
		Info.Label = Game->Label;
		Info.MonthName = Game->MonthName;
		Info.Day = Game->Day;
		Info.DayOfMonth = Game->DayOfMonth;
		Info.Hour = Game->Hour;
		Info.SavedAt = Game->SavedAt;
		return UGameplayStatics::SaveGameToSlot(Index, IndexSlot, 0);
	}

	bool Save(UObject* Ctx, const FString& Slot, const FString& Label)
	{
		USimGameInstanceSubsystem* Owner = USimGameInstanceSubsystem::Get(Ctx);
		USimSaveGame* Game = Cast<USimSaveGame>(UGameplayStatics::CreateSaveGameObject(USimSaveGame::StaticClass()));
		if (Owner == nullptr || Game == nullptr || !Owner->SaveToString(Game->Kernel))
		{
			UE_LOG(LogSchizoGame, Warning, TEXT("Save %s: no running world to save."), *Slot);
			Tell(Ctx, NSLOCTEXT("SimUi", "SaveFailed", "The game could not be saved."));
			return false;
		}
		Game->SecondsSinceLastDay = Owner->GetSecondsSinceLastDay();
		if (APlayerController* PC = PlayerOf(Ctx))
		{
			Game->ControlRotation = PC->GetControlRotation();
			if (const APawn* Pawn = PC->GetPawn())
			{
				Game->PlayerTransform = Pawn->GetActorTransform();
			}
		}
		int32 Year = 1, Month = 1, Dom = 1;
		USimWorldSubsystem::GetSimDateFor(Ctx, Year, Month, Dom);
		Game->Day = USimWorldSubsystem::GetSimDayFor(Ctx);
		Game->MonthName = USimWorldSubsystem::GetSimMonthNameFor(Ctx);
		Game->DayOfMonth = Dom;
		Game->Hour = USimWorldSubsystem::GetSimHourFor(Ctx);
		Game->SavedAt = FDateTime::Now();
		Game->Label = Label.IsEmpty() ? FString::Printf(TEXT("Day %lld"), Game->Day) : Label;
		const bool bSaved = Detail::WriteSlot(Slot, Game);
		Tell(Ctx, bSaved ? NSLOCTEXT("SimUi", "Saved", "Saved.") : NSLOCTEXT("SimUi", "SaveFailed", "The game could not be saved."));
		return bSaved;
	}

	bool Load(UObject* Ctx, const FString& Slot)
	{
		USimSaveGame* Game = Cast<USimSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		if (Game == nullptr)
		{
			UE_LOG(LogSchizoGame, Warning, TEXT("Load %s: no such save."), *Slot);
			Tell(Ctx, NSLOCTEXT("SimUi", "NoSuchSave", "There is no such save."));
			return false;
		}
		if (Game->Version != 1)
		{
			UE_LOG(LogSchizoGame, Error, TEXT("Load %s: save version %d is not supported (expected 1)."), *Slot, Game->Version);
			Tell(Ctx, NSLOCTEXT("SimUi", "BadSave", "That save could not be read; your game continues."));
			return false;
		}
		USimGameInstanceSubsystem* Owner = USimGameInstanceSubsystem::Get(Ctx);
		if (Owner == nullptr || !Owner->LoadFromString(Game->Kernel))
		{
			Tell(Ctx, NSLOCTEXT("SimUi", "BadSave", "That save could not be read; your game continues."));
			return false;  // logged; the current world continues
		}
		// The hour, not the raw seconds: the day length may differ from when it was saved.
		USimWorldSubsystem::SetSimHourFor(Ctx, Game->Hour);
		if (USimGameUserSettings* Settings = USimGameUserSettings::Get())
		{
			Settings->ApplySimSettings(Ctx);  // settings are the player's, not the save's (needs severity)
		}
		if (APlayerController* PC = PlayerOf(Ctx))
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				Pawn->TeleportTo(Game->PlayerTransform.GetLocation(), Game->PlayerTransform.Rotator(), false, true);
				if (ACharacter* Character = Cast<ACharacter>(Pawn))
				{
					Character->GetCharacterMovement()->StopMovementImmediately();  // no momentum carried in
				}
			}
			PC->SetControlRotation(Game->ControlRotation);
			if (ASimPlayerController* SimPC = Cast<ASimPlayerController>(PC))
			{
				SimPC->ResetNeedsClock();  // the clock jumped: re-anchor, charge nothing
			}
		}
		if (ASimNpcDirector* Director = ASimNpcDirector::GetInstance(Ctx))
		{
			Director->RefreshAll();  // the townspeople of the loaded world, now
		}
		UE_LOG(LogSchizoGame, Log, TEXT("Loaded slot %s (day %lld)."), *Slot, Game->Day);
		Tell(Ctx, NSLOCTEXT("SimUi", "Loaded", "Loaded."));
		return true;
	}

	bool Delete(const FString& Slot)
	{
		if (!UGameplayStatics::DoesSaveGameExist(Slot, 0))
		{
			return false;
		}
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
		USimSlotIndex* Index = ReadIndex();
		Index->Slots.RemoveAll([&](const FSimSlotInfo& S) { return S.Slot == Slot; });
		UGameplayStatics::SaveGameToSlot(Index, IndexSlot, 0);
		return true;
	}

	TArray<FSimSlotInfo> List()
	{
		TArray<FSimSlotInfo> Slots = ReadIndex()->Slots;
		// Drop entries whose file is gone (deleted by hand).
		Slots.RemoveAll([](const FSimSlotInfo& S) { return !UGameplayStatics::DoesSaveGameExist(S.Slot, 0); });
		Slots.Sort([](const FSimSlotInfo& A, const FSimSlotInfo& B) { return A.SavedAt > B.SavedAt; });
		return Slots;
	}

	FString MostRecent()
	{
		const TArray<FSimSlotInfo> Slots = List();
		return Slots.Num() > 0 ? Slots[0].Slot : FString();
	}
}
