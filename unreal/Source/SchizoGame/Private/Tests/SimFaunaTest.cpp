// SimFaunaTest.cpp — V-B4 T3: the herds follow the kernel's head count, animals keep to their home ground
// and out of the lagoon, sleep outside their hours, jackals come out only at night, and a missing model
// skips only its species.
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "SimCityData.h"
#include "SimFauna.h"
#include "SimStreetBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FFaunaWorld
	{
		UWorld* World = nullptr;
		ASimFauna* Fauna = nullptr;

		FFaunaWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SimFaunaTest"));
			FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
			Ctx.SetCurrentWorld(World);
			ASimStreetBuilder::BuildQuarter(World);  // the door slots the animals stand at
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Fauna = World->SpawnActor<ASimFauna>(FVector::ZeroVector, FRotator::ZeroRotator, P);
			// No player override: the test world has no pawn, so nobody flees and every animal is "near".
		}

		~FFaunaWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFaunaHerds, "Sim.Fauna.Herds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSimFaunaHerds::RunTest(const FString&)
{
	FFaunaWorld W;
	W.Fauna->HourOverride = 10.f;
	W.Fauna->Populate(1, 400);
	TestEqual(TEXT("400 head: round(400 x 0.55 / 10) = 22 sheep"), W.Fauna->CountOf(TEXT("fat_tailed_sheep")), 22);
	TestEqual(TEXT("400 head: 12 goats"), W.Fauna->CountOf(TEXT("goat")), 12);
	TestEqual(TEXT("400 head: 6 cattle"), W.Fauna->CountOf(TEXT("zebu_cattle")), 6);
	W.Fauna->Populate(2, 100);  // a raid took three quarters of the herd: the next day shows it (Review Focus 1)
	TestEqual(TEXT("100 head: round(5.5) = 6 sheep"), W.Fauna->CountOf(TEXT("fat_tailed_sheep")), 6);
	TestTrue(TEXT("the city has its animals too"), W.Fauna->CountOf(TEXT("donkey")) > 0 && W.Fauna->CountOf(TEXT("cat")) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFaunaHomeGround, "Sim.Fauna.HomeGround",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSimFaunaHomeGround::RunTest(const FString&)
{
	FFaunaWorld W;
	W.Fauna->HourOverride = 10.f;
	W.Fauna->Populate(3, 300);
	// The player walks through the middle of the herd now and then: they flee, but never off their ground.
	const FSimCityFrame& Fr = SimCityData::Frame();
	int32 Strays = 0, Wet = 0;
	for (int32 t = 0; t < 600; ++t)
	{
		if (t % 100 == 50 && W.Fauna->GetAnimals().Num() > 0)
		{
			W.Fauna->PlayerOverride = W.Fauna->GetAnimals()[t % W.Fauna->GetAnimals().Num()].Pos;
		}
		else if (t % 100 == 60)
		{
			W.Fauna->PlayerOverride.Reset();
		}
		W.Fauna->Step(0.2f);
	}
	for (const FSimAnimal& A : W.Fauna->GetAnimals())
	{
		Strays += FVector2D::Distance(A.Pos, A.Home) > A.Radius + 1.f;
		Wet += FVector2D::Distance(A.Pos, Fr.Center) < Fr.LagoonR - 100.f;
	}
	AddInfo(FString::Printf(TEXT("%d animals after 600 steps"), W.Fauna->GetAnimals().Num()));
	TestTrue(TEXT("there are animals"), W.Fauna->GetAnimals().Num() > 0);
	TestEqual(TEXT("every animal stays on its home ground"), Strays, 0);
	TestEqual(TEXT("no animal in the lagoon"), Wet, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFaunaNight, "Sim.Fauna.Night",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSimFaunaNight::RunTest(const FString&)
{
	FFaunaWorld W;
	W.Fauna->Populate(4, 300);
	W.Fauna->HourOverride = 2.f;
	W.Fauna->Step(0.2f);
	int32 Awake = 0;
	for (const FSimAnimal& A : W.Fauna->GetAnimals())
	{
		const FSimFaunaSpecies& S = W.Fauna->GetSpecies()[A.Species];
		if (!S.IsActive(2.f) && !S.IsNocturnal())
		{
			Awake += A.State != ESimAnimalState::Sleep || FVector2D::Distance(A.Pos, A.Home) > A.Radius * 0.5f + 1.f;
		}
	}
	TestEqual(TEXT("02:00: every diurnal animal asleep at home (Review Focus 3)"), Awake, 0);
	TestTrue(TEXT("02:00: jackals out"), W.Fauna->CountOf(TEXT("jackal"), true) > 0);
	W.Fauna->HourOverride = 12.f;
	W.Fauna->Step(0.2f);
	TestEqual(TEXT("12:00: no jackals"), W.Fauna->CountOf(TEXT("jackal"), true), 0);
	W.Fauna->HourOverride = 21.f;
	W.Fauna->Step(0.2f);
	TestTrue(TEXT("21:00: jackals again"), W.Fauna->CountOf(TEXT("jackal"), true) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFaunaMissing, "Sim.Fauna.MissingModels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSimFaunaMissing::RunTest(const FString&)
{
	{
		FFaunaWorld W;
		W.Fauna->HourOverride = 10.f;
		ASimFauna::ForceMissingModels = { TEXT("goat") };
		W.Fauna->Populate(5, 400);
		ASimFauna::ForceMissingModels.Reset();
		TestEqual(TEXT("a missing model: its species is skipped"), W.Fauna->CountOf(TEXT("goat")), 0);
		TestEqual(TEXT("...and the others still spawn (Review Focus 5)"), W.Fauna->CountOf(TEXT("fat_tailed_sheep")), 22);
	}
	{
		FFaunaWorld W;
		ASimFauna::bUseFaunaMeshes = false;
		W.Fauna->Populate(5, 400);
		ASimFauna::bUseFaunaMeshes = true;
		TestEqual(TEXT("a fresh clone: nothing spawns (Review Focus 4)"), W.Fauna->GetAnimals().Num(), 0);
	}
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimFaunaBirds, "Sim.Fauna.Birds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSimFaunaBirds::RunTest(const FString&)
{
	FFaunaWorld W;
	W.Fauna->HourOverride = 10.f;
	W.Fauna->Populate(6, 300);
	const TArray<FSimFlock>& Flocks = W.Fauna->GetFlocks();
	int32 Birds = 0;
	for (const FSimFlock& F : Flocks)
	{
		Birds += F.Pos.Num();
	}
	AddInfo(FString::Printf(TEXT("%d flocks, %d birds and fish"), Flocks.Num(), Birds));
	TestTrue(TEXT("flocks stand and fly"), Flocks.Num() > 0);
	TestTrue(TEXT("within the 600-bird budget"), Birds > 0 && Birds <= 600);

	// 1,000 ticks: every bird keeps within 1.5 x its flock's radius of the anchor.
	for (int32 t = 0; t < 1000; ++t)
	{
		W.Fauna->Step(0.2f);
	}
	int32 Far = 0;
	for (const FSimFlock& F : Flocks)
	{
		for (const FVector& P : F.Pos)
		{
			Far += FVector2D::Distance(FVector2D(P), FVector2D(F.Anchor)) > F.Radius * 1.5f;
		}
	}
	TestEqual(TEXT("every bird keeps to its flock's ground"), Far, 0);

	// The player walks up to a standing flock: within 1 s its mean height rises >= 2 m (Review Focus 2).
	const int32 Standing = Flocks.IndexOfByPredicate([&W](const FSimFlock& F)
	{
		return F.bGrounded && W.Fauna->GetSpecies()[F.Species].Id != TEXT("carp") && F.Pos.Num() > 0;
	});
	if (TestTrue(TEXT("a standing flock"), Standing != INDEX_NONE))
	{
		auto MeanZ = [&Flocks, Standing]()
		{
			float Z = 0.f;
			for (const FVector& P : Flocks[Standing].Pos)
			{
				Z += P.Z / Flocks[Standing].Pos.Num();
			}
			return Z;
		};
		const float Before = MeanZ();
		W.Fauna->PlayerOverride = FVector2D(Flocks[Standing].Pos[0]);
		for (int32 t = 0; t < 5; ++t)
		{
			W.Fauna->Step(0.2f);
		}
		TestTrue(FString::Printf(TEXT("the flock lifts (%.0f -> %.0f cm)"), Before, MeanZ()), MeanZ() - Before >= 200.f);
		TestTrue(TEXT("and stays up while scattered"), Flocks[Standing].LiftTimer > 0.f);
		W.Fauna->PlayerOverride.Reset();
		for (int32 t = 0; t < 150; ++t)  // 30 s after the player has gone: it has settled
		{
			W.Fauna->Step(0.2f);
		}
		TestTrue(TEXT("it resettles after 20 s"), Flocks[Standing].LiftTimer <= 0.f && MeanZ() - Before < 50.f);
	}

	// Deterministic: the same day again stands the same first flock.
	TArray<FVector> First;
	W.Fauna->Populate(7, 300);
	for (int32 t = 0; t < 50; ++t)
	{
		W.Fauna->Step(0.2f);
	}
	if (Flocks.Num() > 0)
	{
		First = Flocks[0].Pos;
	}
	W.Fauna->Populate(7, 300);
	for (int32 t = 0; t < 50; ++t)
	{
		W.Fauna->Step(0.2f);
	}
	int32 Same = 0;
	for (int32 i = 0; Flocks.Num() > 0 && i < First.Num() && i < Flocks[0].Pos.Num(); ++i)
	{
		Same += Flocks[0].Pos[i].Equals(First[i], 0.5);
	}
	TestTrue(TEXT("deterministic for a fixed day"), First.Num() > 0 && Same == First.Num());
	return true;
}

#endif
