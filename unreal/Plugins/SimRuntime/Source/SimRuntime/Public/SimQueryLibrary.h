// SimQueryLibrary.h — A7: the UI's read and verb surface over the kernel's
// quests, journal, dialogue, people and events (sim/CApiQuests.h,
// sim/CApiPeople.h). Blueprint-callable for the UI shell; each UFUNCTION
// forwards to a SimQuery:: function that takes the SimWorld* directly, so
// tests and C++ callers need no play world.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SimQueryLibrary.generated.h"

struct SimWorld;

UENUM(BlueprintType)
enum class ESimQuestList : uint8
{
	Available,
	Active,
	Completed,
	Failed
};

USTRUCT(BlueprintType)
struct FSimQuestInfo
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Giver;
	/** Active quests only. */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Stage;
	/** Active quests only: deadline day, 0 = none. */
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int64 Deadline = 0;
};

USTRUCT(BlueprintType)
struct FSimJournalEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int64 Day = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Stage;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Text;
};

USTRUCT(BlueprintType)
struct FSimDialogueLine
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Text;
};

USTRUCT(BlueprintType)
struct FSimMemory
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int64 Day = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Subject;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Fact;
};

USTRUCT(BlueprintType)
struct FSimEventInfo
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Sim") int64 Day = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Rule;
	UPROPERTY(BlueprintReadOnly, Category = "Sim") FString Summary;
};

/** Handle-level functions (null world: empty results, -1 codes). */
namespace SimQuery
{
	SIMRUNTIME_API TArray<FSimQuestInfo> GetQuests(SimWorld* W, ESimQuestList List);
	SIMRUNTIME_API TArray<FSimJournalEntry> GetJournal(SimWorld* W, const FString& QuestId);
	SIMRUNTIME_API int32 AcceptQuest(SimWorld* W, const FString& QuestId);
	SIMRUNTIME_API int32 AdvanceQuest(SimWorld* W, const FString& QuestId, const FString& Stage, const FString& Text);
	SIMRUNTIME_API int32 CompleteQuest(SimWorld* W, const FString& QuestId);
	SIMRUNTIME_API int32 AbandonQuest(SimWorld* W, const FString& QuestId);
	SIMRUNTIME_API TArray<FSimDialogueLine> GetDialogue(SimWorld* W, const FString& Speaker);
	SIMRUNTIME_API FString GetNpcName(SimWorld* W, const FString& NpcId);
	SIMRUNTIME_API TArray<FSimMemory> GetNpcMemories(SimWorld* W, const FString& NpcId);
	SIMRUNTIME_API TArray<FSimEventInfo> GetRecentEvents(SimWorld* W, int32 MaxCount);
}

UCLASS()
class SIMRUNTIME_API USimQueryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Sim|Quests", meta = (WorldContext = "Ctx"))
	static TArray<FSimQuestInfo> GetQuests(const UObject* Ctx, ESimQuestList List);

	UFUNCTION(BlueprintPure, Category = "Sim|Quests", meta = (WorldContext = "Ctx"))
	static TArray<FSimJournalEntry> GetJournal(const UObject* Ctx, const FString& QuestId);

	/** 0 accepted; -2 unknown; -3 already active, completed or failed. */
	UFUNCTION(BlueprintCallable, Category = "Sim|Quests", meta = (WorldContext = "Ctx"))
	static int32 AcceptQuest(const UObject* Ctx, const FString& QuestId);

	/** 1 stage changed, 0 same stage, -3 not active. */
	UFUNCTION(BlueprintCallable, Category = "Sim|Quests", meta = (WorldContext = "Ctx"))
	static int32 AdvanceQuest(const UObject* Ctx, const FString& QuestId, const FString& Stage, const FString& Text);

	/** 0 done (rewards paid), -3 not active. */
	UFUNCTION(BlueprintCallable, Category = "Sim|Quests", meta = (WorldContext = "Ctx"))
	static int32 CompleteQuest(const UObject* Ctx, const FString& QuestId);

	/** 0 given up (journaled "abandoned"), -3 not active. */
	UFUNCTION(BlueprintCallable, Category = "Sim|Quests", meta = (WorldContext = "Ctx"))
	static int32 AbandonQuest(const UObject* Ctx, const FString& QuestId);

	UFUNCTION(BlueprintPure, Category = "Sim|Dialogue", meta = (WorldContext = "Ctx"))
	static TArray<FSimDialogueLine> GetDialogue(const UObject* Ctx, const FString& Speaker);

	/** The display name; the id itself when the npc is unknown. */
	UFUNCTION(BlueprintPure, Category = "Sim|People", meta = (WorldContext = "Ctx"))
	static FString GetNpcName(const UObject* Ctx, const FString& NpcId);

	UFUNCTION(BlueprintPure, Category = "Sim|People", meta = (WorldContext = "Ctx"))
	static TArray<FSimMemory> GetNpcMemories(const UObject* Ctx, const FString& NpcId);

	/** Newest first, at most MaxCount. */
	UFUNCTION(BlueprintPure, Category = "Sim|Events", meta = (WorldContext = "Ctx"))
	static TArray<FSimEventInfo> GetRecentEvents(const UObject* Ctx, int32 MaxCount);
};
