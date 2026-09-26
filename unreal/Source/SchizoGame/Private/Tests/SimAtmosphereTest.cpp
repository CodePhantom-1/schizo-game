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

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimAtmosphereFx, "Sim.Atmosphere.Fx",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSimAtmosphereFx::RunTest(const FString&)
{
	// The schedule (pure).
	TestTrue(TEXT("the bakery smokes at dawn"), ASimAtmosphere::SmokeOn(TEXT("bakery"), 6.f, 0.f));
	TestFalse(TEXT("...not at noon"), ASimAtmosphere::SmokeOn(TEXT("bakery"), 12.f, 0.f));
	TestTrue(TEXT("a home's hearth at dusk"), ASimAtmosphere::SmokeOn(TEXT("home_modest"), 18.f, 0.f));
	TestFalse(TEXT("...not at dawn"), ASimAtmosphere::SmokeOn(TEXT("home_modest"), 6.f, 0.f));
	TestTrue(TEXT("the foundry all day"), ASimAtmosphere::SmokeOn(TEXT("foundry"), 3.f, 0.f));
	TestFalse(TEXT("nothing in the rain"), ASimAtmosphere::SmokeOn(TEXT("foundry"), 3.f, 1.f));

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SimAtmosphereFxTest"));
	FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
	Ctx.SetCurrentWorld(World);
	ASimAtmosphere* A = ASimAtmosphere::BuildAtmosphere(World);
	if (TestNotNull(TEXT("the atmosphere"), A) && TestNotNull(TEXT("the rain field (run ue_make_fx_materials.py if null)"), A->GetRain()))
	{
		TestEqual(TEXT("1,500 rain streaks"), A->GetRain()->GetInstanceCount(), ASimAtmosphere::RainCount);
		TestEqual(TEXT("800 dust motes"), A->GetDust()->GetInstanceCount(), ASimAtmosphere::DustCount);
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Sim/smoke.csv")));
		TestEqual(TEXT("a smoke source per smoke.csv row"), A->NumSmokeSources(), Lines.Num() - 1);
		TestEqual(TEXT("five puffs each"), A->GetSmoke()->GetInstanceCount(), A->NumSmokeSources() * ASimAtmosphere::PuffsPerSource);

		FSimAtmosphere Dry = ASimAtmosphere::Target(TEXT("clear"), 0, TEXT("harvest"), 12.f, false);
		A->ApplyFx(Dry, 12.f, FVector::ZeroVector);
		TestFalse(TEXT("no rain: the streaks are hidden"), A->GetRain()->IsVisible());
		TestFalse(TEXT("no dust: the motes are hidden"), A->GetDust()->IsVisible());
		FSimAtmosphere Wet = ASimAtmosphere::Target(TEXT("rain"), 0, TEXT("rains"), 12.f, false);
		A->ApplyFx(Wet, 12.f, FVector::ZeroVector);
		TestTrue(TEXT("rain: the streaks show"), A->GetRain()->IsVisible());
		int32 LitAtDawn = 0, LitInRain = 0;
		A->ApplyFx(Dry, 6.f, FVector::ZeroVector);
		for (int32 s = 0; s < A->NumSmokeSources(); ++s)
		{
			LitAtDawn += A->IsSmokeLit(s);
		}
		A->ApplyFx(Wet, 6.f, FVector::ZeroVector);
		for (int32 s = 0; s < A->NumSmokeSources(); ++s)
		{
			LitInRain += A->IsSmokeLit(s);
		}
		TestTrue(TEXT("dawn: the ovens and furnaces smoke"), LitAtDawn > 0);
		TestEqual(TEXT("rain: every fire out"), LitInRain, 0);
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
