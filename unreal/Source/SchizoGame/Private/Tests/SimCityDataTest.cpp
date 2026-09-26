// SimCityDataTest.cpp — AA4/AA5: the engine reads the crescent from the canon.
#include "Misc/AutomationTest.h"
#include "SimCityData.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCityDataLoads, "Sim.City.DataLoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSimCityDataLoads::RunTest(const FString&)
{
	const FSimCityFrame& F = SimCityData::Frame();
	TestEqual(TEXT("crescent centre x (cm)"), F.Center.X, 24000.0);
	TestEqual(TEXT("lagoon edge (cm)"), F.LagoonR, 10500.f);
	TestEqual(TEXT("wall (cm)"), F.WallR, 17500.f);
	TestTrue(TEXT("a whole city of places"), SimCityData::Places().Num() > 100);
	const FSimCityPlace* Gate = SimCityData::Find(TEXT("moon_gate_place"));
	if (TestNotNull(TEXT("the Moon Gate"), Gate))
	{
		TestEqual(TEXT("gate x (cm)"), Gate->Center.X, 6570.0, 1.0);
		TestEqual(TEXT("gate yaw"), Gate->YawDeg, 95.f, 0.01f);
		TestEqual(TEXT("gate typology"), Gate->Typology, FString(TEXT("city_gate")));
		TestEqual(TEXT("gate width (cm)"), Gate->Size.X, 1600.0, 0.1);
	}
	// door_side (V-B1 T5): the lagoon-side row opens outward (+y), the wall side inward.
	const FSimCityPlace* Inner = SimCityData::Find(TEXT("caravan_yard_place"));
	const FSimCityPlace* Outer = SimCityData::Find(TEXT("donkey_stables_place"));
	TestTrue(TEXT("lagoon-side door opens on +y"), Inner != nullptr && Inner->bDoorPlusY);
	TestTrue(TEXT("wall-side door opens on -y"), Outer != nullptr && !Outer->bDoorPlusY);
	const FSimCityPlace* Name = SimCityData::Find(TEXT("temple_front_place"));
	TestTrue(TEXT("quoted names with commas parse"), Name != nullptr && !Name->Name.IsEmpty() && !Name->Quarter.IsEmpty());
	return true;
}

#endif
