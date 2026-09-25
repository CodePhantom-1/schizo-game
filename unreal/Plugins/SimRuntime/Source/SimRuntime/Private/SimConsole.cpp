// SimConsole.cpp — A5: developer commands for every live kernel system.
// Each later stage adds the commands for its own system here (completion-plan A5).
// Bad arguments print the usage line and change nothing.
#include "SimWorldSubsystem.h"
#include "SimRuntimeModule.h"
#include "sim/CApi.h"
#include "sim/CApiVerbs.h"

#include "HAL/IConsoleManager.h"
#include "Engine/World.h"

namespace
{
	bool NeedArgs(const TArray<FString>& Args, int32 Min, const TCHAR* Usage)
	{
		if (Args.Num() < Min)
		{
			UE_LOG(LogSimRuntime, Warning, TEXT("usage: %s"), Usage);
			return false;
		}
		return true;
	}

	SimWorld* Handle(UWorld* World)
	{
		SimWorld* H = USimWorldSubsystem::GetSimHandleFor(World);
		if (H == nullptr)
		{
			UE_LOG(LogSimRuntime, Warning, TEXT("sim: the world does not exist yet"));
		}
		return H;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdAdvanceDays(TEXT("sim.AdvanceDays"), TEXT("sim.AdvanceDays N — run the world N days"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 1, TEXT("sim.AdvanceDays N")) || Handle(World) == nullptr) return;
			USimWorldSubsystem::AdvanceSimDaysFor(World, FMath::Max(0, FCString::Atoi(*Args[0])));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAdvanceHours(TEXT("sim.AdvanceHours"), TEXT("sim.AdvanceHours N — move the clock N game hours"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 1, TEXT("sim.AdvanceHours N")) || Handle(World) == nullptr) return;
			USimWorldSubsystem::SkipSimHoursFor(World, FMath::Max(0.f, FCString::Atof(*Args[0])));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdGive(TEXT("sim.Give"), TEXT("sim.Give <item> [qty=1] [actor=player]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 1, TEXT("sim.Give <item> [qty=1] [actor=player]"))) return;
			SimWorld* H = Handle(World);
			if (H == nullptr) return;
			const int32 Qty = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1;
			const FString Actor = Args.Num() > 2 ? Args[2] : TEXT("player");
			const int Now = sim_world_give_item(H, TCHAR_TO_UTF8(*Actor), TCHAR_TO_UTF8(*Args[0]), Qty);
			UE_LOG(LogSimRuntime, Display, TEXT("sim.Give: %s now holds %d %s"), *Actor, Now, *Args[0]);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSetNeed(TEXT("sim.SetNeed"), TEXT("sim.SetNeed <hunger|thirst|fatigue> <0-100> [actor=player]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 2, TEXT("sim.SetNeed <hunger|thirst|fatigue> <0-100> [actor=player]"))) return;
			SimWorld* H = Handle(World);
			if (H == nullptr) return;
			const FString Actor = Args.Num() > 2 ? Args[2] : TEXT("player");
			if (sim_world_set_need(H, TCHAR_TO_UTF8(*Actor), TCHAR_TO_UTF8(*Args[0]), FCString::Atoi(*Args[1])) != 0)
			{
				UE_LOG(LogSimRuntime, Warning, TEXT("usage: sim.SetNeed <hunger|thirst|fatigue> <0-100> [actor=player]"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdStanding(TEXT("sim.Standing"), TEXT("sim.Standing <faction> <0-100> — set standing"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 2, TEXT("sim.Standing <faction> <0-100>"))) return;
			SimWorld* H = Handle(World);
			if (H == nullptr) return;
			const FTCHARToUTF8 F(*Args[0]);
			sim_world_add_standing(H, F.Get(), FCString::Atoi(*Args[1]) - sim_world_standing(H, F.Get()));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdFavour(TEXT("sim.Favour"), TEXT("sim.Favour <deity> <0-100> — set favour"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 2, TEXT("sim.Favour <deity> <0-100>"))) return;
			SimWorld* H = Handle(World);
			if (H == nullptr) return;
			const FTCHARToUTF8 D(*Args[0]);
			sim_world_add_favour(H, D.Get(), FCString::Atoi(*Args[1]) - sim_world_favour(H, D.Get()));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDrought(TEXT("sim.Drought"), TEXT("sim.Drought <stage>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 1, TEXT("sim.Drought <stage>")) || Handle(World) == nullptr) return;
			USimWorldSubsystem::SetSimDroughtFor(World, FCString::Atoi(*Args[0]));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdWar(TEXT("sim.War"), TEXT("sim.War <stage>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 1, TEXT("sim.War <stage>"))) return;
			if (SimWorld* H = Handle(World)) sim_world_set_war(H, FCString::Atoi(*Args[0]));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDump(TEXT("sim.Dump"), TEXT("sim.Dump — print the world's vital signs"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			SimWorld* H = Handle(World);
			if (H == nullptr) return;
			int32 Y = 0, M = 0, D = 0;
			USimWorldSubsystem::GetSimDateFor(World, Y, M, D);
			UE_LOG(LogSimRuntime, Display, TEXT("sim: day %lld (%d %s, year %d) %.2fh season=%s moon=%s(%d%%) omen=%s observance=%s"),
				USimWorldSubsystem::GetSimDayFor(World), D, *USimWorldSubsystem::GetSimMonthNameFor(World), Y,
				USimWorldSubsystem::GetSimHourFor(World), *USimWorldSubsystem::GetSimSeasonFor(World),
				*USimWorldSubsystem::GetSimMoonPhaseFor(World), USimWorldSubsystem::GetSimMoonIlluminationFor(World),
				*USimWorldSubsystem::GetSimDayOmenFor(World), *USimWorldSubsystem::GetSimDayObservanceFor(World));
			UE_LOG(LogSimRuntime, Display, TEXT("sim: player hunger=%d thirst=%d fatigue=%d drought=%d war=%d npcs=%d"),
				sim_world_hunger(H, "player"), sim_world_thirst(H, "player"), sim_world_fatigue(H, "player"),
				sim_world_drought(H), sim_world_war(H), sim_world_npc_count(H));
		}));
}
