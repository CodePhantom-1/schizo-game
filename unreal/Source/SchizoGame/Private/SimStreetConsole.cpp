// SimStreetConsole.cpp — A5: console commands that need the built street.
#include "SimStreetBuilder.h"

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
}
