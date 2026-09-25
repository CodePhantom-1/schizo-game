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

UCLASS()
class SCHIZOGAME_API ASimCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASimCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** W/S relative to the camera's facing — the street's long axis. */
	void OnMoveForward(float AxisValue);
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
