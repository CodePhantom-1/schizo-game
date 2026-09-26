// SimTerrainTest.cpp — V-B3: the terrain's ground kinds (for M_Terrain's detail) never change Tommy's
// colours; landforms keep the city, road and canals as they were.
#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "Misc/Crc.h"
#include "SimEnvironment.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 2,000 seeded (X, Y, H, Nz, Seed) samples over the whole terrain window, water to far hills. */
	template <typename F>
	void ForSamples(F&& Fn)
	{
		FRandomStream R(20260926);
		for (int32 i = 0; i < 2000; ++i)
		{
			const float X = R.FRandRange(-95000.f, 60000.f);
			const float Y = R.FRandRange(-60000.f, 70000.f);
			const float H = (i % 2 == 0) ? R.FRandRange(-450.f, 400.f) : R.FRandRange(-450.f, 25000.f);
			const float Nz = R.FRandRange(0.6f, 1.f);
			Fn(X, Y, H, Nz, static_cast<uint32>(i) * 7919u);
		}
	}

	uint32 TerrainColorHash()
	{
		uint32 Crc = 0;
		ForSamples([&Crc](float X, float Y, float H, float Nz, uint32 Seed)
		{
			const FLinearColor C = ASimEnvironment::SampleTerrainColor(X, Y, H, Nz, Seed);
			const int32 Q[3] = { FMath::RoundToInt(C.R * 100000.f), FMath::RoundToInt(C.G * 100000.f), FMath::RoundToInt(C.B * 100000.f) };
			Crc = FCrc::MemCrc32(Q, sizeof(Q), Crc);
		});
		return Crc;
	}

	// Computed on the unmodified TerrainColor (V-B3 T2 step 0): the refactor must reproduce it exactly.
	constexpr uint32 PreRefactorColorHash = 4154157424u;

	/** Heights of 500 seeded points on the city and its apron (within 210 m of the crescent's centre). */
	uint32 ApronHeightHash()
	{
		FRandomStream R(4000);
		uint32 Crc = 0;
		for (int32 i = 0; i < 500; ++i)
		{
			const float A = R.FRandRange(0.f, 2.f * PI), Rad = FMath::Sqrt(R.FRand()) * 21000.f;
			const int32 H = FMath::RoundToInt(ASimEnvironment::TerrainHeight(24000.f + Rad * FMath::Cos(A), 2000.f + Rad * FMath::Sin(A)) * 100.f);
			Crc = FCrc::MemCrc32(&H, sizeof(H), Crc);
		}
		return Crc;
	}

	// Computed before the landforms (V-B3 T3 step 0): the city and its apron must not move.
	constexpr uint32 PreLandformApronHash = 371132571u;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimTerrainGroundKind, "Sim.Terrain.GroundKind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSimTerrainGroundKind::RunTest(const FString&)
{
	ASimEnvironment::LoadFrame();
	const uint32 Hash = TerrainColorHash();
	AddInfo(FString::Printf(TEXT("terrain colour hash over 2000 samples: %u"), Hash));
	TestEqual(TEXT("Tommy's terrain colours are unchanged"), Hash, PreRefactorColorHash);

	// Six hand-picked points (cm): the kind is the region's.
	auto Kind = [](float X, float Y, float H) { return ASimEnvironment::GroundKind(X, Y, H, 1.f, 7u); };
	TestTrue(TEXT("the lagoon bed"), Kind(24000.f, 2000.f, -420.f) == ESimGround::Bed);
	TestTrue(TEXT("the beach by the sea"), Kind(43000.f, 30000.f, 10.f) == ESimGround::Beach);
	TestTrue(TEXT("the road west of the Moon Gate"), Kind(-3000.f, 500.f, 10.f) == ESimGround::Road);
	const ESimGround Field = Kind(-40000.f, 5000.f, 20.f);
	TestTrue(TEXT("a field is farmland"), Field == ESimGround::Irrigated || Field == ESimGround::Silt || Field == ESimGround::Cracked || Field == ESimGround::Salt);
	TestTrue(TEXT("the desert south"), Kind(20000.f, 65000.f, 300.f) == ESimGround::Dune);
	TestTrue(TEXT("the ring street"), Kind(33899.f, 11899.f, 0.f) == ESimGround::Street);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimTerrainLandforms, "Sim.Terrain.Landforms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSimTerrainLandforms::RunTest(const FString&)
{
	ASimEnvironment::LoadFrame();
	const uint32 Apron = ApronHeightHash();
	AddInfo(FString::Printf(TEXT("apron height hash over 500 samples: %u"), Apron));
	TestEqual(TEXT("the city and its apron stay flat"), Apron, PreLandformApronHash);

	// The road west from the Moon Gate (6570, 470) stays at road level; the canal crossing is its bridge.
	int32 HighRoad = 0;
	for (float X = -90000.f; X < 5500.f; X += 1000.f)
	{
		if (FMath::Abs(X + 30000.f) > 1200.f)
		{
			HighRoad += ASimEnvironment::TerrainHeight(X, 470.f) > 20.f;
		}
	}
	TestEqual(TEXT("the road west stays at or under 20 cm"), HighRoad, 0);

	// The canals' centrelines stay dug.
	int32 Shallow = 0;
	for (float Cy : { -21000.f, -9000.f, 9000.f, 21000.f })
	{
		for (float X = -65000.f; X < -5000.f; X += 5000.f)
		{
			Shallow += ASimEnvironment::TerrainHeight(X, Cy) > -250.f;
		}
	}
	TestEqual(TEXT("every canal centre stays at or under -250 cm"), Shallow, 0);

	// Five tells, each a mound of its kind, clear of the road and the river.
	const TArray<FSimTell> Tells = ASimEnvironment::GetTells();
	TestEqual(TEXT("five tells"), Tells.Num(), 5);
	for (const FSimTell& T : Tells)
	{
		const float Top = ASimEnvironment::TerrainHeight(T.Center.X, T.Center.Y);
		float Ring = 0.f;
		for (int32 k = 0; k < 16; ++k)
		{
			const FVector2D P = T.Center + FVector2D(1.3f * T.Radius, 0.f).GetRotated(22.5f * k);
			Ring += ASimEnvironment::TerrainHeight(P.X, P.Y) / 16.f;
		}
		const FString Where = FString::Printf(TEXT("tell at (%.0f, %.0f) r %.0f"), T.Center.X, T.Center.Y, T.Radius);
		TestTrue(Where + TEXT(" stands 8 m over its surroundings"), Top - Ring >= 800.f);
		TestTrue(Where + TEXT(" is a Tell"), ASimEnvironment::GroundKind(T.Center.X, T.Center.Y, Top, 1.f, 0u) == ESimGround::Tell);
		TestTrue(Where + TEXT(" is 500 m clear of the road west"), T.Center.X > 6570.f + 50000.f + T.Radius || FMath::Abs(T.Center.Y - 470.f) >= 50000.f + T.Radius);
		TestTrue(Where + TEXT(" is 500 m clear of the river"), FMath::Abs(T.Center.Y - ASimEnvironment::RiverCentreY(T.Center.X)) >= 50000.f + T.Radius);
	}
	return true;
}

#endif
