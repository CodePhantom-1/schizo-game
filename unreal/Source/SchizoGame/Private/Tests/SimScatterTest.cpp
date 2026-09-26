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

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Builds the scatter in a fresh game world; returns it (the world is left for the caller to destroy). */
	ASimScatter* BuildCountryWorld(UWorld*& OutWorld, FSimScatterResult& OutResult)
	{
		OutWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SimCountrysideTest"));
		FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
		Ctx.SetCurrentWorld(OutWorld);
		OutResult = ASimScatter::BuildScatter(OutWorld);
		for (TActorIterator<ASimScatter> It(OutWorld); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	void DestroyCountryWorld(UWorld* World)
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	TArray<UHierarchicalInstancedStaticMeshComponent*> CountryComps(ASimScatter* S, const TCHAR* MeshPrefix = TEXT(""))
	{
		TArray<UHierarchicalInstancedStaticMeshComponent*> All, Out;
		S->GetComponents(All);
		const FString Prefix = FString(TEXT("Country_")) + MeshPrefix;
		for (UHierarchicalInstancedStaticMeshComponent* C : All)
		{
			if (C->GetName().StartsWith(Prefix))
			{
				Out.Add(C);
			}
		}
		Out.Sort([](const UHierarchicalInstancedStaticMeshComponent& A, const UHierarchicalInstancedStaticMeshComponent& B) { return A.GetName() < B.GetName(); });
		return Out;
	}

	bool AllVisible(const TArray<UHierarchicalInstancedStaticMeshComponent*>& Comps, bool bVisible)
	{
		for (const UHierarchicalInstancedStaticMeshComponent* C : Comps)
		{
			if (C->IsVisible() != bVisible)
			{
				return false;
			}
		}
		return Comps.Num() > 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimScatterCountryside, "Sim.Scatter.Countryside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSimScatterCountryside::RunTest(const FString&)
{
	UWorld* World = nullptr;
	FSimScatterResult R;
	ASimScatter* S = BuildCountryWorld(World, R);
	if (!TestNotNull(TEXT("the scatter actor"), S))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("countryside: %d instances"), R.NumCountryside));
	TestTrue(TEXT("the countryside keeps its budget"), R.NumCountryside > 0 && R.NumCountryside <= 60000);
	for (const TCHAR* Habitat : { TEXT("levee"), TEXT("field"), TEXT("bank"), TEXT("desert"), TEXT("tell_top") })
	{
		const int32 N = R.CountrysideByHabitat.FindRef(Habitat);
		AddInfo(FString::Printf(TEXT("  %s: %d"), Habitat, N));
		TestTrue(FString::Printf(TEXT("the %s habitat has instances"), Habitat), N > 0);
	}

	// Nothing on the road west of the Moon Gate (6570, 470): 6 m either side (Review Focus 5).
	int32 OnRoad = 0;
	for (const UHierarchicalInstancedStaticMeshComponent* C : CountryComps(S))
	{
		for (int32 i = 0; i < C->GetInstanceCount(); ++i)
		{
			FTransform T;
			C->GetInstanceTransform(i, T, true);
			OnRoad += T.GetLocation().X < 6570.f && FMath::Abs(T.GetLocation().Y - 470.f) < 600.f;
		}
	}
	TestEqual(TEXT("the road west stays clear"), OnRoad, 0);

	// The crops follow the season (Review Focus 4).
	const TArray<UHierarchicalInstancedStaticMeshComponent*> Shoots = CountryComps(S, TEXT("crops_wheat_a"));
	const TArray<UHierarchicalInstancedStaticMeshComponent*> Ripe = CountryComps(S, TEXT("crops_wheat_b"));
	const TArray<UHierarchicalInstancedStaticMeshComponent*> Bare = CountryComps(S, TEXT("crops_dirt_row"));
	S->ApplyWorldState(0, TEXT("sowing"));
	TestTrue(TEXT("sowing: green shoots"), AllVisible(Shoots, true) && AllVisible(Ripe, false) && AllVisible(Bare, false));
	S->ApplyWorldState(0, TEXT("harvest"));
	TestTrue(TEXT("harvest: ripe barley"), AllVisible(Ripe, true) && AllVisible(Shoots, false) && AllVisible(Bare, false));
	S->ApplyWorldState(0, TEXT("rains"));
	TestTrue(TEXT("rains: bare rows"), AllVisible(Bare, true) && AllVisible(Shoots, false) && AllVisible(Ripe, false));
	S->ApplyWorldState(0, TEXT("vintage"));
	TestTrue(TEXT("vintage: bare rows"), AllVisible(Bare, true) && AllVisible(Shoots, false) && AllVisible(Ripe, false));

	// Deterministic: a second build stands the same first 100 instances.
	TArray<FVector> First;
	for (const UHierarchicalInstancedStaticMeshComponent* C : CountryComps(S))
	{
		for (int32 i = 0; i < C->GetInstanceCount() && First.Num() < 100; ++i)
		{
			FTransform T;
			C->GetInstanceTransform(i, T, true);
			First.Add(T.GetLocation());
		}
	}
	DestroyCountryWorld(World);
	UWorld* Again = nullptr;
	FSimScatterResult R2;
	ASimScatter* S2 = BuildCountryWorld(Again, R2);
	TestEqual(TEXT("the same count"), R2.NumCountryside, R.NumCountryside);
	int32 k = 0, Same = 0;
	if (S2 != nullptr)
	{
		for (const UHierarchicalInstancedStaticMeshComponent* C : CountryComps(S2))
		{
			for (int32 i = 0; i < C->GetInstanceCount() && k < First.Num(); ++i, ++k)
			{
				FTransform T;
				C->GetInstanceTransform(i, T, true);
				Same += T.GetLocation().Equals(First[k], 0.01);
			}
		}
	}
	TestEqual(TEXT("the same first 100 transforms"), Same, First.Num());
	DestroyCountryWorld(Again);
	return true;
}

#endif
