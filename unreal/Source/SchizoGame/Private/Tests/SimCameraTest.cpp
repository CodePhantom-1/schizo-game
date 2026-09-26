// SimCameraTest.cpp — part 3 Task 8: the third-person camera's rig (A15, DQ1).
#include "Misc/AutomationTest.h"
#include "SimCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCameraRigTest, "Sim.Camera.Rig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSimCameraRigTest::RunTest(const FString&)
{
	const FSimCameraRig Out = ASimCharacter::ComputeCameraRig(false, false, true);
	TestEqual(TEXT("outdoors: long arm"), Out.ArmLength, 340.f);
	TestEqual(TEXT("right shoulder"), float(Out.SocketOffset.Y), 45.f);
	TestEqual(TEXT("head height"), float(Out.SocketOffset.Z), 80.f);

	const FSimCameraRig In = ASimCharacter::ComputeCameraRig(false, true, true);
	TestEqual(TEXT("indoors: short arm"), In.ArmLength, 180.f);
	TestEqual(TEXT("indoors: lower"), float(In.SocketOffset.Z), 60.f);

	const FSimCameraRig Aim = ASimCharacter::ComputeCameraRig(true, true, true);
	TestEqual(TEXT("aiming beats indoors"), Aim.ArmLength, 140.f);
	TestEqual(TEXT("aiming: wider shoulder"), float(Aim.SocketOffset.Y), 60.f);

	const FSimCameraRig Left = ASimCharacter::ComputeCameraRig(false, false, false);
	TestEqual(TEXT("left shoulder mirrors"), float(Left.SocketOffset.Y), -45.f);
	return true;
}

#endif
