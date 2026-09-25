// SimCharacter.h — the slice's protagonist: a body on the street. The kernel
// already models "player" (needs, purse, crimes); this pawn is that person's
// legs — walking, gravity, a shoulder camera — replacing the engine's
// flying spectator DefaultPawn. Flat-colour programmer art (D-023) until the
// clothing set arrives.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SimCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;

UCLASS()
class SCHIZOGAME_API ASimCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASimCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

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

	/** Programmer-art body: a cylinder and a sphere, flat-coloured. */
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Sim")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	float WalkSpeed = 420.f;
	float SprintSpeed = 800.f;
};
