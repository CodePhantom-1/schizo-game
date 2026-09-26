// SimCompassTest.cpp — Stage V batch 1 Task 1 (D-026): the world's compass
// matches reality. UE is left-handed, so with +X east the south is +Y.
#include "Misc/AutomationTest.h"
#include "SimCompass.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCompassTest, "Sim.Compass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSimCompassTest::RunTest(const FString&)
{
	// Turning right (UE +yaw) from east faces south, as on a real compass.
	const FVector RightOfEast = FRotator(0.f, 90.f, 0.f).RotateVector(SimCompass::East);
	TestTrue(TEXT("right of east is south"), RightOfEast.Equals(-SimCompass::North, 1e-4f));

	const FVector Rise = SimCompass::SunDir(0.f, 62.f);
	TestTrue(TEXT("the sun rises in the east"), Rise.X > 0.9f);
	const FVector Noon = SimCompass::SunDir(HALF_PI, 62.f);
	TestTrue(TEXT("noon sun stands in the south"), FVector::DotProduct(Noon, -SimCompass::North) > 0.3f);
	TestTrue(TEXT("noon sun is up"), Noon.Z > 0.8f);
	const FVector Set = SimCompass::SunDir(PI, 62.f);
	TestTrue(TEXT("the sun sets in the west"), Set.X < -0.9f);
	return true;
}

#endif
