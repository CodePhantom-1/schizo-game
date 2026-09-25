// SimSaves.h — A10: save slots. A save is the kernel snapshot plus what the
// engine owns (the sub-day clock, the player's place and view) plus the
// metadata the Save/Load screen lists. The slot list lives in a small index
// save, because the engine has no portable way to enumerate slots.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SimSaves.generated.h"

/** What the Save/Load list shows for one slot (kept in the index, so listing reads no big saves). */
USTRUCT(BlueprintType)
struct FSimSlotInfo
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Slot;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Label;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString MonthName;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int64 Day = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int32 DayOfMonth = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") float Hour = 6.f;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FDateTime SavedAt;
};

UCLASS()
class SCHIZOGAME_API USimSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	/** Bumped when the layout changes; a load refuses any other version. */
	UPROPERTY() int32 Version = 1;
	/** The kernel snapshot (sim_world_save_to_buffer text). */
	UPROPERTY() FString Kernel;
	UPROPERTY() double SecondsSinceLastDay = 0.0;
	UPROPERTY() FTransform PlayerTransform;
	UPROPERTY() FRotator ControlRotation = FRotator::ZeroRotator;
	UPROPERTY() int64 Day = 1;
	UPROPERTY() FString MonthName;
	UPROPERTY() int32 DayOfMonth = 1;
	UPROPERTY() float Hour = 6.f;
	UPROPERTY() FDateTime SavedAt;
	UPROPERTY() FString Label;
};

/** The slot index (its own tiny save). */
UCLASS()
class SCHIZOGAME_API USimSlotIndex : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() TArray<FSimSlotInfo> Slots;
};

namespace SimSaves
{
	inline const TCHAR* QuickSlot = TEXT("quicksave");
	inline const TCHAR* AutoSlot = TEXT("autosave");

	/** Saves the running game into Slot (the label defaults to "Day N"). Logs the size; false on failure. */
	SCHIZOGAME_API bool Save(UObject* Ctx, const FString& Slot, const FString& Label = FString());
	/** Restores the kernel world, the clock, the player's place and view from Slot. False keeps the game as it was. */
	SCHIZOGAME_API bool Load(UObject* Ctx, const FString& Slot);
	/** Deletes the slot and its index entry. False when there was no such slot. */
	SCHIZOGAME_API bool Delete(const FString& Slot);
	/** Every slot, newest first. */
	SCHIZOGAME_API TArray<FSimSlotInfo> List();
	/** The newest slot, or "" when there is none (Continue). */
	SCHIZOGAME_API FString MostRecent();

	namespace Detail
	{
		/** Writes a prepared save and updates the index (Save's last step; tests use it directly). */
		SCHIZOGAME_API bool WriteSlot(const FString& Slot, USimSaveGame* Game);
	}
}
