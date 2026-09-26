// SimStreetConsole.cpp — A5: console commands that need the built street.
#include "SimStreetBuilder.h"
#include "SimSaves.h"
#include "UI/SimShellSubsystem.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimStreetConsole, Log, All);

namespace
{
	FAutoConsoleCommandWithWorldAndArgs CmdPlaces(TEXT("sim.Places"), TEXT("sim.Places — list every place id the street registered"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld*)
		{
			TArray<FName> Ids;
			ASimStreetBuilder::GetAllPlaceIds(Ids);
			for (const FName& Id : Ids)
			{
				UE_LOG(LogSimStreetConsole, Display, TEXT("%s"), *Id.ToString());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdTeleport(TEXT("sim.Teleport"), TEXT("sim.Teleport <place_id> — move the player to a registered place"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FVector Where;
			if (Args.Num() < 1 || !ASimStreetBuilder::GetPlaceLocation(FName(*Args[0]), Where))
			{
				UE_LOG(LogSimStreetConsole, Warning, TEXT("usage: sim.Teleport <place_id> (see sim.Places)"));
				return;
			}
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (Pawn != nullptr)
			{
				Pawn->TeleportTo(Where + FVector(0.f, 0.f, 120.f), Pawn->GetActorRotation());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSaveSlot(TEXT("sim.SaveSlot"), TEXT("sim.SaveSlot <slot> [label…] — save the game"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogSimStreetConsole, Warning, TEXT("usage: sim.SaveSlot <slot> [label…]"));
				return;
			}
			TArray<FString> Rest = Args;
			Rest.RemoveAt(0);
			SimSaves::Save(World, Args[0], FString::Join(Rest, TEXT(" ")));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdLoadSlot(TEXT("sim.LoadSlot"), TEXT("sim.LoadSlot <slot> — load a saved game"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogSimStreetConsole, Warning, TEXT("usage: sim.LoadSlot <slot> (see sim.Slots)"));
				return;
			}
			SimSaves::Load(World, Args[0]);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSlots(TEXT("sim.Slots"), TEXT("sim.Slots — list the saved games, newest first"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld*)
		{
			for (const FSimSlotInfo& S : SimSaves::List())
			{
				UE_LOG(LogSimStreetConsole, Display, TEXT("%s — %s — %d %s (day %lld), %.2fh — %s"), *S.Slot, *S.Label,
					S.DayOfMonth, *S.MonthName, S.Day, S.Hour, *S.SavedAt.ToString());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdNewGame(TEXT("sim.NewGame"), TEXT("sim.NewGame — start a fresh game (as the main menu does)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (USimShellSubsystem* Shell = USimShellSubsystem::Get(World))
			{
				Shell->StartNewGame();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDeleteSlot(TEXT("sim.DeleteSlot"), TEXT("sim.DeleteSlot <slot> — delete a saved game"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			if (Args.Num() < 1 || !SimSaves::Delete(Args[0]))
			{
				UE_LOG(LogSimStreetConsole, Warning, TEXT("usage: sim.DeleteSlot <slot> (see sim.Slots)"));
			}
		}));
}
