// SimQueryLibrary.cpp — A7: see the header.
#include "SimQueryLibrary.h"

#include "SimWorldSubsystem.h"
#include "sim/CApi.h"

namespace
{
	const char* ListName(ESimQuestList List)
	{
		switch (List)
		{
		case ESimQuestList::Active: return "active";
		case ESimQuestList::Completed: return "completed";
		case ESimQuestList::Failed: return "failed";
		default: return "available";
		}
	}

	FString Str(const char* S) { return FString(UTF8_TO_TCHAR(S)); }

	SimWorld* H(const UObject* Ctx) { return USimWorldSubsystem::GetSimHandleFor(Ctx); }
}

namespace SimQuery
{
	TArray<FSimQuestInfo> GetQuests(SimWorld* W, ESimQuestList List)
	{
		TArray<FSimQuestInfo> Out;
		const char* L = ListName(List);
		const int N = sim_world_quest_count(W, L);
		char Id[256], Buf[2048];
		for (int i = 0; i < N; ++i)
		{
			if (sim_world_quest_at(W, L, i, Id, sizeof(Id)) < 0) continue;
			FSimQuestInfo& Q = Out.AddDefaulted_GetRef();
			Q.Id = Str(Id);
			if (sim_world_quest_title(W, Id, Buf, sizeof(Buf)) >= 0) Q.Title = Str(Buf);
			if (sim_world_quest_giver(W, Id, Buf, sizeof(Buf)) >= 0) Q.Giver = Str(Buf);
			if (sim_world_quest_kind(W, Id, Buf, sizeof(Buf)) >= 0) Q.Kind = Str(Buf);
			if (sim_world_quest_act(W, Id, Buf, sizeof(Buf)) >= 0) Q.Act = Str(Buf);
			if (List == ESimQuestList::Active)
			{
				if (sim_world_quest_stage(W, Id, Buf, sizeof(Buf)) >= 0) Q.Stage = Str(Buf);
				Q.Deadline = FMath::Max<int64>(0, sim_world_quest_deadline(W, Id));
			}
		}
		return Out;
	}

	TArray<FSimJournalEntry> GetJournal(SimWorld* W, const FString& QuestId)
	{
		TArray<FSimJournalEntry> Out;
		const FTCHARToUTF8 Q(*QuestId);
		const int N = sim_world_journal_count(W, Q.Get());
		char Stage[256], Text[2048];
		for (int i = 0; i < N; ++i)
		{
			int64_t Day = 0;
			if (sim_world_journal_at(W, Q.Get(), i, &Day, Stage, sizeof(Stage), Text, sizeof(Text)) < 0) continue;
			Out.Add({Day, Str(Stage), Str(Text)});
		}
		return Out;
	}

	int32 AcceptQuest(SimWorld* W, const FString& QuestId) { return sim_world_quest_accept(W, TCHAR_TO_UTF8(*QuestId)); }

	int32 AdvanceQuest(SimWorld* W, const FString& QuestId, const FString& Stage, const FString& Text)
	{
		const FTCHARToUTF8 Q(*QuestId), S(*Stage), T(*Text);
		return sim_world_quest_advance(W, Q.Get(), S.Get(), T.Get());
	}

	int32 CompleteQuest(SimWorld* W, const FString& QuestId) { return sim_world_quest_complete(W, TCHAR_TO_UTF8(*QuestId)); }

	int32 AbandonQuest(SimWorld* W, const FString& QuestId) { return sim_world_quest_abandon(W, TCHAR_TO_UTF8(*QuestId)); }

	TArray<FSimDialogueLine> GetDialogue(SimWorld* W, const FString& Speaker)
	{
		TArray<FSimDialogueLine> Out;
		const FTCHARToUTF8 S(*Speaker);
		const int N = sim_world_dialogue_count(W, S.Get());
		char Id[256], Text[2048];
		for (int i = 0; i < N; ++i)
		{
			if (sim_world_dialogue_at(W, S.Get(), i, Id, sizeof(Id), Text, sizeof(Text)) < 0) continue;
			Out.Add({Str(Id), Str(Text)});
		}
		return Out;
	}

	FString GetNpcName(SimWorld* W, const FString& NpcId)
	{
		char Buf[256];
		return sim_world_npc_name(W, TCHAR_TO_UTF8(*NpcId), Buf, sizeof(Buf)) > 0 ? Str(Buf) : NpcId;
	}

	TArray<FSimMemory> GetNpcMemories(SimWorld* W, const FString& NpcId)
	{
		TArray<FSimMemory> Out;
		const FTCHARToUTF8 N(*NpcId);
		const int Count = sim_world_npc_memory_count(W, N.Get());
		char Subject[256], Fact[2048];
		for (int i = 0; i < Count; ++i)
		{
			int64_t Day = 0;
			if (sim_world_npc_memory_at(W, N.Get(), i, &Day, Subject, sizeof(Subject), Fact, sizeof(Fact)) < 0) continue;
			Out.Add({Day, Str(Subject), Str(Fact)});
		}
		return Out;
	}

	TArray<FSimEventInfo> GetRecentEvents(SimWorld* W, int32 MaxCount)
	{
		TArray<FSimEventInfo> Out;
		char Rule[256], Summary[2048];
		for (int i = sim_world_event_count(W) - 1; i >= 0 && Out.Num() < MaxCount; --i)
		{
			int64_t Day = 0;
			if (sim_world_event_at(W, i, &Day, Rule, sizeof(Rule), Summary, sizeof(Summary)) < 0) continue;
			Out.Add({Day, Str(Rule), Str(Summary)});
		}
		return Out;
	}
}

TArray<FSimQuestInfo> USimQueryLibrary::GetQuests(const UObject* Ctx, ESimQuestList List) { return SimQuery::GetQuests(H(Ctx), List); }
TArray<FSimJournalEntry> USimQueryLibrary::GetJournal(const UObject* Ctx, const FString& QuestId) { return SimQuery::GetJournal(H(Ctx), QuestId); }
int32 USimQueryLibrary::AcceptQuest(const UObject* Ctx, const FString& QuestId) { return SimQuery::AcceptQuest(H(Ctx), QuestId); }
int32 USimQueryLibrary::AdvanceQuest(const UObject* Ctx, const FString& QuestId, const FString& Stage, const FString& Text) { return SimQuery::AdvanceQuest(H(Ctx), QuestId, Stage, Text); }
int32 USimQueryLibrary::CompleteQuest(const UObject* Ctx, const FString& QuestId) { return SimQuery::CompleteQuest(H(Ctx), QuestId); }
int32 USimQueryLibrary::AbandonQuest(const UObject* Ctx, const FString& QuestId) { return SimQuery::AbandonQuest(H(Ctx), QuestId); }
TArray<FSimDialogueLine> USimQueryLibrary::GetDialogue(const UObject* Ctx, const FString& Speaker) { return SimQuery::GetDialogue(H(Ctx), Speaker); }
FString USimQueryLibrary::GetNpcName(const UObject* Ctx, const FString& NpcId) { return SimQuery::GetNpcName(H(Ctx), NpcId); }
TArray<FSimMemory> USimQueryLibrary::GetNpcMemories(const UObject* Ctx, const FString& NpcId) { return SimQuery::GetNpcMemories(H(Ctx), NpcId); }
TArray<FSimEventInfo> USimQueryLibrary::GetRecentEvents(const UObject* Ctx, int32 MaxCount) { return SimQuery::GetRecentEvents(H(Ctx), MaxCount); }
