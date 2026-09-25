// SimNpc.h — W6-C: the street lives. One kernel npc made visible: a capsule
// with a basic engine body (no skeletons yet — a later wave), walking on foot
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
	/** The grey-box body: no skeleton exists yet, so a scaled engine cylinder
	    stands in for the character mesh (ACharacter's own
	    SkeletalMeshComponent stays hidden until one does). */
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	TObjectPtr<class UStaticMeshComponent> BodyMesh;

	/** The kernel npc id (people.csv id). */
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	FString NpcId;

	FVector TargetLocation = FVector::ZeroVector;
	FString CurrentScheduleId;
	FString CurrentTaskText;
};
