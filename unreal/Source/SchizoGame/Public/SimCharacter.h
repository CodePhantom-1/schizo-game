// SimCharacter.h — the slice's protagonist: a body on the street. The kernel
// already models "player" (needs, purse, crimes); this pawn is that person's
// legs — walking, gravity, a shoulder camera — replacing the engine's
// flying spectator DefaultPawn. ART-2: a low-poly rigged traveller from the
// CC0 character cast (/Game/Art/Characters/Player) with a single-clip walk/
// idle switch; until that import runs, the flat-colour cylinder + sphere
// programmer art (D-023) carries on.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SimCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UAnimationAsset;

/** Where the third-person camera wants to be (A15): arm length and shoulder offset. */
struct FSimCameraRig
{
	float ArmLength = 340.f;
	FVector SocketOffset = FVector(0.f, 45.f, 80.f);
};

UCLASS()
class SCHIZOGAME_API ASimCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASimCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void Tick(float DeltaSeconds) override;

	/** The camera rig for a situation: outdoors 340 cm, under a roof 180, aiming 140 (aiming wins);
	 *  shoulder offset ±45 (±60 aiming), head height 80 (60 indoors). */
	static FSimCameraRig ComputeCameraRig(bool bAiming, bool bIndoors, bool bRightShoulder);

	void OnAimStart() { bAiming = true; }
	void OnAimEnd() { bAiming = false; }
	void OnShoulderSwap() { bRightShoulder = !bRightShoulder; }

protected:
	virtual void BeginPlay() override;

	/** W/S relative to the camera's facing — the street's long axis. */
	void OnMoveForward(float AxisValue);
	/** Look input scaled by the player's sensitivity and Y-invert settings (A11). */
	void OnTurn(float Value);
	void OnLookUp(float Value);
	/** A/D strafing. */
	void OnMoveRight(float AxisValue);
	/** LeftShift held: the walk becomes a run (an INVENTED convenience; the
	    kernel's fatigue is unaffected — fatigue ticks by hour, not strides). */
	void OnSprintStart();
	void OnSprintEnd();

private:
	/** Shoulder camera (Valheim-like third person; there are no arms yet). */
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	TObjectPtr<USpringArmComponent> Boom;

	UPROPERTY(VisibleAnywhere, Category = "Sim")
	TObjectPtr<UCameraComponent> Camera;

	/** Programmer-art body: a cylinder and a sphere, flat-coloured. Hidden
	    (not destroyed) the moment the imported traveller loads. */
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Sim")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	/** ART-2 character pass: the imported traveller's looping clips
	    (/Game/Art/Characters/Player/Anims — Walk/Idle). Null until the
	    rigged mesh loads (UPROPERTY keeps GC from pulling them). */
	UPROPERTY()
	TObjectPtr<UAnimationAsset> WalkAnim;

	UPROPERTY()
	TObjectPtr<UAnimationAsset> IdleAnim;

	/** Which of the two clips is playing (raw alias of Walk/Idle). */
	UAnimationAsset* CurrentAnim = nullptr;

	/** True once the rigged traveller took over from the programmer art. */
	bool bSkeletalBody = false;

	/** Camera state (A15). */
	bool bAiming = false;
	bool bRightShoulder = true;
	/** Eases the boom toward the rig each tick; a roof overhead shortens it. */
	void UpdateCamera(float DeltaSeconds);

	/** Try to replace the cylinder + sphere with the imported traveller
	    (skeletal preferred, static mesh second, programmer art stays if
	    neither asset exists yet). */
	bool ApplyTravellerBody();

	/** Single-clip locomotion: walk while moving, idle otherwise (no anim
	    blueprint in the slice — see docs/proposals/invented-ledger-humans.md). */
	void UpdateBodyAnimation(bool bMoving);

	float WalkSpeed = 420.f;
	float SprintSpeed = 800.f;
};
