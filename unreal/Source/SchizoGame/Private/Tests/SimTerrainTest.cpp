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
	TestTrue(TEXT("a field is farmland"), Field == ESimGround::Irrigated || Field == ESimGround::Silt || Field == ESimGround::Cracked);
	TestTrue(TEXT("the desert south"), Kind(20000.f, 65000.f, 300.f) == ESimGround::Dune);
	TestTrue(TEXT("the ring street"), Kind(33899.f, 11899.f, 0.f) == ESimGround::Street);
	return true;
}

#endif
