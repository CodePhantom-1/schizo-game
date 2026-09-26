// SimAtmosphereTest.cpp — V-B5 T1: the weather's targets, the easing, the drying.
#include "Misc/AutomationTest.h"
#include "SimAtmosphere.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimAtmosphereTargets, "Sim.Atmosphere.Targets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSimAtmosphereTargets::RunTest(const FString&)
{
	const FSimAtmosphere Clear = ASimAtmosphere::Target(TEXT("clear"), 0, TEXT("rains"), 12.f, false);
	TestTrue(TEXT("clear, no drought: all zeros (the world as it was)"),
		Clear.Dust == 0.f && Clear.Rain == 0.f && Clear.Overcast == 0.f && Clear.Fog == 0.f && Clear.Wet == 0.f
		&& Clear.Wither == 0.f && Clear.Shimmer == 0.f && !Clear.bFestival);
	TestEqual(TEXT("sandstorm: dust 1"), ASimAtmosphere::Target(TEXT("sandstorm"), 0, TEXT("harvest"), 12.f, false).Dust, 1.f);
	const FSimAtmosphere Fog = ASimAtmosphere::Target(TEXT("fog"), 0, TEXT("rains"), 6.f, false);
	TestTrue(TEXT("fog: overcast >= 0.5 and the haze x4 at dawn (Fog 1)"), Fog.Overcast >= 0.5f && Fog.Fog == 1.f);
	TestEqual(TEXT("scorching: heat shimmer on"), ASimAtmosphere::Target(TEXT("scorching"), 0, TEXT("harvest"), 14.f, false).Shimmer, 1.f);
	const FSimAtmosphere Rain = ASimAtmosphere::Target(TEXT("rain"), 0, TEXT("rains"), 17.f, false);
	TestTrue(TEXT("rain: rain 1, overcast >= 0.6"), Rain.Rain == 1.f && Rain.Overcast >= 0.6f);
	const FSimAtmosphere Dry = ASimAtmosphere::Target(TEXT("clear"), 4, TEXT("harvest"), 12.f, false);
	TestTrue(TEXT("drought 4: wither 1 and dust >= 0.3 on a clear day"), Dry.Wither == 1.f && Dry.Dust >= 0.3f);
	TestTrue(TEXT("a festival passes through"), ASimAtmosphere::Target(TEXT("clear"), 0, TEXT("rains"), 20.f, true).bFestival);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimAtmosphereEasing, "Sim.Atmosphere.Easing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSimAtmosphereEasing::RunTest(const FString&)
{
	// The weather changes mid-day: after 60 real seconds the dust is 63% of the way (Review Focus 1).
	FSimAtmosphere Live = ASimAtmosphere::Target(TEXT("clear"), 0, TEXT("harvest"), 12.f, false);
	const FSimAtmosphere Storm = ASimAtmosphere::Target(TEXT("sandstorm"), 0, TEXT("harvest"), 12.f, false);
	for (int32 i = 0; i < 600; ++i)
	{
		ASimAtmosphere::Ease(Live, Storm, 0.1f, 0.f);
	}
	TestTrue(FString::Printf(TEXT("63%% +- 5%% in 60 s (%.3f)"), Live.Dust), FMath::Abs(Live.Dust - 0.632f) <= 0.05f);
	FSimAtmosphere One = ASimAtmosphere::Target(TEXT("clear"), 0, TEXT("harvest"), 12.f, false);
	ASimAtmosphere::Ease(One, Storm, 1.f, 0.f);
	TestTrue(TEXT("never a snap: one second moves it < 2%"), One.Dust < 0.02f);

	// Wet: soaks with the rain, dries over two in-game hours after it (Review Focus 3).
	FSimAtmosphere Wet = ASimAtmosphere::Target(TEXT("rain"), 0, TEXT("rains"), 17.f, false);
	const FSimAtmosphere After = ASimAtmosphere::Target(TEXT("clear"), 0, TEXT("rains"), 18.f, false);
	ASimAtmosphere::Ease(Wet, After, 1.f, 1.f);
	TestTrue(FString::Printf(TEXT("half dry after one in-game hour (%.2f)"), Wet.Wet), FMath::Abs(Wet.Wet - 0.5f) < 0.01f);
	ASimAtmosphere::Ease(Wet, After, 1.f, 1.f);
	TestEqual(TEXT("dry after two"), Wet.Wet, 0.f);
	return true;
}

#endif
