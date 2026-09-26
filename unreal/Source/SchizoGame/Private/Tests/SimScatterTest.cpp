// SimScatterTest.cpp — V-B2 T5: the city wears the scatter (scatter.csv, one HISM per mesh and season
// set): every row whose mesh is imported stands, no instance at a door, and the kernel's drought and
// season reach it (MPC_World.Wither, the spring flowers hidden out of season).
#include "Misc/AutomationTest.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "SimCityData.h"
#include "SimScatter.h"
#include "SimStreetBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** scatter.csv rows whose mesh asset exists (the count BuildScatter must stand). */
	int32 ExpectedInstances()
	{
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *ASimScatter::ScatterCsvPath());
		int32 N = 0;
		for (int32 i = 1; i < Lines.Num(); ++i)
		{
			const TArray<FString> F = SimCityData::SplitCsv(Lines[i]);
			if (F.Num() >= 7 && ASimScatter::MeshExists(F[0]))
			{
				++N;
			}
		}
		return N;
	}

	/** The poppies' HISM (flower_red_a, rains;sowing) — null when the mesh is not imported. */
	UHierarchicalInstancedStaticMeshComponent* Poppies(ASimScatter* S)
	{
		TArray<UHierarchicalInstancedStaticMeshComponent*> Comps;
		S->GetComponents(Comps);
		for (UHierarchicalInstancedStaticMeshComponent* C : Comps)
		{
			if (C->GetName().StartsWith(TEXT("Scatter_flower_red_a")))
			{
				return C;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimScatter, "Sim.Scatter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSimScatter::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SimScatterTest"));
	FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
	Ctx.SetCurrentWorld(World);
	ASimStreetBuilder::BuildQuarter(World);
	const FSimScatterResult R = ASimScatter::BuildScatter(World);

	TestEqual(TEXT("every row whose mesh is imported stands"), R.NumInstances, ExpectedInstances());
	TestTrue(TEXT("the scatter stands (run tools/art/ue_import_scatter.py if 0)"), R.NumInstances > 0);

	TArray<FSimDoorSlotInfo> Slots;
	ASimStreetBuilder::GetAllDoorSlots(Slots);
	ASimScatter* Scatter = nullptr;
	for (TActorIterator<ASimScatter> It(World); It; ++It)
	{
		Scatter = *It;
	}
	if (TestNotNull(TEXT("the scatter actor"), Scatter))
	{
		TArray<UHierarchicalInstancedStaticMeshComponent*> Comps;
		Scatter->GetComponents(Comps);
		int32 AtDoor = 0;
		for (const UHierarchicalInstancedStaticMeshComponent* C : Comps)
		{
			for (int32 i = 0; i < C->GetInstanceCount(); ++i)
			{
				FTransform T;
				C->GetInstanceTransform(i, T, true);
				for (const FSimDoorSlotInfo& S : Slots)
				{
					AtDoor += FVector2D::Distance(FVector2D(T.GetLocation()), FVector2D(S.Location)) < 250.f;
				}
			}
		}
		TestEqual(TEXT("no instance within 2.5 m of a door slot"), AtDoor, 0);

		Scatter->ApplyWorldState(0, TEXT("rains"));
		TestEqual(TEXT("drought 0: no withering"), Scatter->GetWither(), 0.f);
		UHierarchicalInstancedStaticMeshComponent* P = Poppies(Scatter);
		if (TestNotNull(TEXT("the poppies"), P))
		{
			TestTrue(TEXT("poppies bloom in the rains"), P->IsVisible());
		}
		Scatter->ApplyWorldState(4, TEXT("harvest"));
		TestEqual(TEXT("drought 4: fully withered"), Scatter->GetWither(), 1.f);
		if (P != nullptr)
		{
			TestFalse(TEXT("no poppies at harvest"), P->IsVisible());
		}
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	// A fresh clone before the import: nothing stands, nothing crashes (Review Focus 2).
	UWorld* Bare = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SimScatterTestBare"));
	FWorldContext& BareCtx = GEngine->CreateNewWorldContext(EWorldType::Game);
	BareCtx.SetCurrentWorld(Bare);
	ASimScatter::bUseScatterMeshes = false;
	const FSimScatterResult None = ASimScatter::BuildScatter(Bare);
	ASimScatter::bUseScatterMeshes = true;
	TestEqual(TEXT("no meshes: no instances"), None.NumInstances, 0);
	GEngine->DestroyWorldContext(Bare);
	Bare->DestroyWorld(false);
	return true;
}

#endif
