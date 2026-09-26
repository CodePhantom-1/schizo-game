// SimStreetBuildingsTest.cpp — V-B1 T7: the street wears the generated buildings (one mesh per place)
// and every door slot stands exactly where the kit room would have put it.
#include "Misc/AutomationTest.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "SimCityData.h"
#include "SimStreetBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Builds the street in a fresh game world; returns its door slots and the building components' names. */
	TMap<FName, FSimDoorSlotInfo> BuildOnce(bool bMeshes, TSet<FName>& OutComponents)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SimStreetBuildingsTest"));
		FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
		Ctx.SetCurrentWorld(World);
		ASimStreetBuilder::bUseBuildingMeshes = bMeshes;
		ASimStreetBuilder::BuildQuarter(World);
		ASimStreetBuilder::bUseBuildingMeshes = true;
		TArray<FSimDoorSlotInfo> Slots;
		ASimStreetBuilder::GetAllDoorSlots(Slots);
		TMap<FName, FSimDoorSlotInfo> Out;
		for (const FSimDoorSlotInfo& S : Slots)
		{
			Out.Add(S.PlaceId, S);
		}
		for (TActorIterator<ASimStreetBuilder> It(World); It; ++It)
		{
			TArray<UStaticMeshComponent*> Comps;
			It->GetComponents(Comps);
			for (const UStaticMeshComponent* C : Comps)
			{
				OutComponents.Add(C->GetFName());
			}
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimStreetBuildings, "Sim.StreetBuildings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSimStreetBuildings::RunTest(const FString&)
{
	TSet<FName> MeshComps, KitComps;
	const TMap<FName, FSimDoorSlotInfo> WithMeshes = BuildOnce(true, MeshComps);
	const TMap<FName, FSimDoorSlotInfo> KitOnly = BuildOnce(false, KitComps);

	TestEqual(TEXT("the same door slots either way"), WithMeshes.Num(), KitOnly.Num());
	int32 Meshes = 0;
	for (const TPair<FName, FSimDoorSlotInfo>& It : KitOnly)
	{
		const FSimDoorSlotInfo* M = WithMeshes.Find(It.Key);
		if (!TestNotNull(*FString::Printf(TEXT("%s keeps its door slot"), *It.Key.ToString()), M))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s door slot in the same place"), *It.Key.ToString()),
			M->Location.Equals(It.Value.Location, 1.0));
		TestEqual(*FString::Printf(TEXT("%s door faces the same way"), *It.Key.ToString()), M->OutwardYawDeg, It.Value.OutwardYawDeg, 0.01f);
		const FName Comp(*FString::Printf(TEXT("Bld_%s"), *It.Key.ToString()));
		if (ASimStreetBuilder::TryBuildingMesh(It.Key) != nullptr)
		{
			++Meshes;
			TestTrue(*FString::Printf(TEXT("%s stands as its building mesh"), *It.Key.ToString()), MeshComps.Contains(Comp));
		}
		TestFalse(TEXT("the kit-only street has no building meshes"), KitComps.Contains(Comp));
	}
	// Tommy's fresh clone before the import has none: the street still stands on the kit (Review Focus 4).
	AddInfo(FString::Printf(TEXT("%d of %d door slots stand as generated buildings"), Meshes, KitOnly.Num()));
	// The sea gate is SimEnvironment's monument: the street builds no room inside it.
	TestFalse(TEXT("no room at the sea gate"), WithMeshes.Contains(TEXT("sea_gate_place")) || KitOnly.Contains(TEXT("sea_gate_place")));
	return true;
}

#endif
