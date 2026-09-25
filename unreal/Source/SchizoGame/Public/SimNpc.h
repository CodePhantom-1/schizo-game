// SimNpc.h — W6-C: the street lives. One kernel npc made visible: a low-poly
// rigged body from the CC0 character cast (/Game/Art/Characters, ART-2) —
// or, until that import runs, a tinted engine cylinder — walking on foot
// toward wherever ASimNpcDirector last pointed it. The actor MIRRORS the
// kernel's schedule; it never decides where to be (the kernel's rule is the
// engine's rule).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SimNpc.generated.h"

/**
 * A street resident. Lifecycle: the director spawns it at the kernel place
 * the npc currently occupies, then re-points it every sim hour with WalkTo.
 * Identity is the kernel npc id (the C API exposes no display-name read, so
 * the id is the honest label; names arrive with a kernel name getter).
 */
UCLASS()
class SCHIZOGAME_API ASimNpc : public ACharacter
{
	GENERATED_BODY()

public:
	ASimNpc();

	virtual void Tick(float DeltaSeconds) override;

	/** One-time setup at spawn: the kernel identity + a deterministic tint. */
	void InitSimIdentity(const FString& InNpcId);

	/** The director's hourly order: walk toward this point (straight line —
	    the slice's street has no nav mesh and no corners yet). */
	void WalkTo(const FVector& NewTarget);

	/** The director's hourly task update (kernel npc_task_at). Logs on change. */
	void SetCurrentTask(const FString& ScheduleId, const FString& TaskText);

	/** The kernel npc id this actor mirrors (people.csv id). */
	UFUNCTION(BlueprintPure, Category = "Sim")
	FString GetNpcId() const { return NpcId; }

	/** What this npc is doing this hour (kernel task text; HUD-facing). */
	UFUNCTION(BlueprintPure, Category = "Sim")
	FString GetCurrentTaskText() const { return CurrentTaskText; }

	/** The schedule row id in force (kernel npc_schedule_at). */
	UFUNCTION(BlueprintPure, Category = "Sim")
	FString GetCurrentScheduleId() const { return CurrentScheduleId; }

private:
	/** The grey-box body: a scaled engine cylinder stands in for the
	    character mesh until the art pass imports one (it is hidden, not
	    destroyed, the moment a rigged body loads — see
	    ApplyCharacterBody). */
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	TObjectPtr<class UStaticMeshComponent> BodyMesh;

	/** The kernel npc id (people.csv id). */
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	FString NpcId;

	/** ART-2 character pass: the imported body's looping clips
	    (/Game/Art/Characters/NpcN/Anims — Walk/Idle), kept as UPROPERTYs so
	    GC can't pull them mid-street. Null until a rigged mesh loads. */
	UPROPERTY()
	TObjectPtr<class UAnimationAsset> WalkAnim;

	UPROPERTY()
	TObjectPtr<class UAnimationAsset> IdleAnim;

	/** Which of the two clips is playing (raw alias of Walk/Idle — they are
	    referenced above, this only compares). */
	class UAnimationAsset* CurrentAnim = nullptr;

	/** True once a rigged /Game/Art/Characters body took over from the
	    cylinder (the anim switch and the tint skip key off this). */
	bool bSkeletalBody = false;

	/** Try to replace the cylinder with the imported low-poly body for
	    this npc's hash-picked variant (skeletal preferred, static mesh
	    second, cylinder stays if neither asset exists yet). */
	bool ApplyCharacterBody();

	/** Single-clip locomotion: walk while moving, idle otherwise (no anim
	    blueprint in the slice — see docs/proposals/invented-ledger-humans.md). */
	void UpdateBodyAnimation(bool bMoving);

	FVector TargetLocation = FVector::ZeroVector;
	FString CurrentScheduleId;
	FString CurrentTaskText;
};
