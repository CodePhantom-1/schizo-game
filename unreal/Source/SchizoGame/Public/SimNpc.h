// SimNpc.h — UE-4: the street lives. A named City-of-the-Moon resident:
// capsule + a basic engine mesh (no MetaHumans yet — that's a later wave),
// walking on foot toward wherever ASimNpcDirector last pointed it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SimNpc.generated.h"

UCLASS()
class SCHIZOGAME_API ASimNpc : public ACharacter
{
	GENERATED_BODY()

public:
	ASimNpc();

	virtual void Tick(float DeltaSeconds) override;

	/** One-time setup at spawn: identity + a per-role tint. */
	void InitResident(const FString& InNpcId, const FString& InResidentName, const FString& InRole);

	/** The director calls this every sim hour with the derived destination. */
	void SetTargetLocation(const FVector& NewTarget);

	/** The director calls this every sim hour with the kernel's task for now. */
	void SetCurrentTask(const FString& ScheduleId, const FString& TaskText);

	/** What this NPC is doing right now (hearing tasks, HUD-facing). */
	UFUNCTION(BlueprintPure, Category = "Sim")
	FString GetCurrentTaskText() const { return CurrentTaskText; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sim")
	FString NpcId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sim")
	FString ResidentName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sim")
	FString Role;

protected:
	virtual void BeginPlay() override;

	/** The grey-box body: no skeleton exists yet, so a scaled cylinder stands
	    in for the character mesh (ACharacter's own SkeletalMeshComponent
	    stays unused/hidden). */
	UPROPERTY(VisibleAnywhere, Category = "Sim")
	TObjectPtr<class UStaticMeshComponent> BodyMesh;

	FVector TargetLocation = FVector::ZeroVector;
	FString CurrentScheduleId;
	FString CurrentTaskText;
};
