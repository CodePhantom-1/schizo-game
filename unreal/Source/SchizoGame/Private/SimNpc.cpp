// SimNpc.cpp — see SimNpc.h. A grey-box body that walks straight at the
// director's target (no nav mesh yet — the street is one line, so
// straight-line steering is honest for the slice; upgrade when it grows
// corners). Movement itself is the CharacterMovementComponent's smooth
// walk — no physics trickery, no collision beyond the capsule's block.
#include "SimNpc.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Crc.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimNpc, Log, All);

namespace
{
	// Stop walking this close to the spot: arriving residents don't jitter on
	// top of the point, and several residents at one place keep a small ring.
	constexpr float kArriveRadiusCm = 50.f;
}

ASimNpc::ASimNpc()
{
	PrimaryActorTick.bCanEverTick = true;

	// Human-scale capsule (ACharacter's default radius 34 / half-height 88 —
	// already about right; kept explicit so the body lines up under it).
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);

	// No skeleton exists yet: hide the inherited mesh and stand a scaled
	// cylinder in its place. No collision on the body — the capsule blocks.
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		Skel->SetVisibility(false);
		Skel->SetHiddenInGame(true);
	}
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderAsset.Object);
	}
	// Engine cylinder is 100x100x100: scale to a human silhouette (~60 cm
	// across, ~180 cm tall) with its base on the capsule floor.
	BodyMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.8f));
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -88.f + 90.f));

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = 140.f;  // a human walking pace, cm/s
		Move->bOrientRotationToMovement = true;
	}
}

void ASimNpc::InitSimIdentity(const FString& InNpcId)
{
	NpcId = InNpcId;
#if WITH_EDITOR
	// Actor labels are editor-only: the Game target has no SetActorLabel.
	SetActorLabel(FString::Printf(TEXT("Npc_%s"), *NpcId));
#endif

	// Deterministic tint: hash of the npc id -> hue, on the engine's basic
	// shape material (the one basic-shapes material that carries a "Color"
	// parameter). Best effort — if the load fails the body stays grey.
	if (BodyMesh == nullptr)
	{
		return;
	}
	if (UMaterial* Base = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial")))
	{
		BodyMesh->SetMaterial(0, Base);
		if (UMaterialInstanceDynamic* Dyn = BodyMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			const float Hue = static_cast<float>(FCrc::StrCrc32(*NpcId.ToLower()) % 360u);
			// HSV by hand (the engine's helpers are uint8-based or blueprint-side).
			const float C = 0.85f * 0.45f;
			const float Hp = Hue / 60.f;
			const float X = C * (1.f - FMath::Abs(FMath::Fmod(Hp, 2.f) - 1.f));
			FLinearColor Rgb = Hp < 1.f ? FLinearColor(C, X, 0.f, 1.f)
				: Hp < 2.f ? FLinearColor(X, C, 0.f, 1.f)
				: Hp < 3.f ? FLinearColor(0.f, C, X, 1.f)
				: Hp < 4.f ? FLinearColor(0.f, X, C, 1.f)
				: Hp < 5.f ? FLinearColor(X, 0.f, C, 1.f)
				: FLinearColor(C, 0.f, X, 1.f);
			Rgb += FLinearColor(0.85f - C, 0.85f - C, 0.85f - C, 0.f);
			Dyn->SetVectorParameterValue(TEXT("Color"), Rgb);
		}
	}
}

void ASimNpc::WalkTo(const FVector& NewTarget)
{
	TargetLocation = NewTarget;
}

void ASimNpc::SetCurrentTask(const FString& ScheduleId, const FString& TaskText)
{
	if (ScheduleId == CurrentScheduleId)
	{
		return;  // same row in force — nothing new to log
	}
	CurrentScheduleId = ScheduleId;
	CurrentTaskText = TaskText;
	UE_LOG(LogSimNpc, Log, TEXT("%s: %s [%s]"), *NpcId, *TaskText, *ScheduleId);
}

void ASimNpc::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Straight-line steering toward the director's target. The movement
	// component does the walking (and the smooth interpolation) from the
	// input fed here; Z is ignored so residents stay on the street floor.
	FVector ToTarget = TargetLocation - GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.SizeSquared() > FMath::Square(kArriveRadiusCm))
	{
		ToTarget.Normalize();
		AddMovementInput(ToTarget, 1.f);
	}
}
