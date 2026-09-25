// SimNpc.cpp — see SimNpc.h. A low-poly rigged body (or the cylinder it
// grew out of) that walks straight at the director's target (no nav mesh
// yet — the street is one line, so straight-line steering is honest for
// the slice; upgrade when it grows corners). Movement itself is the
// CharacterMovementComponent's smooth walk — no physics trickery, no
// collision beyond the capsule's block.
#include "SimNpc.h"

#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
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

	// ART-2 character pass: rigged low-poly bodies live under
	// /Game/Art/Characters (Quaternius CC0 cast, imported by
	// tools/art/ue_import_characters.py). Npc0..Npc8 are the variants; the
	// npc-id hash picks one, the SAME hash that drove the old cylinder tint,
	// so a resident keeps their look across sessions. Missing assets are
	// NOT an error — before the import runs (or if it never does) the
	// street keeps its tinted cylinders.
	constexpr int32 kNumBodyVariants = 9;

	// Every imported body is bounds-normalized to this height: the sources
	// carry a ~4x export scale and whether the importer applied the metre
	// -> cm conversion is its call, so the capsule (2 x 88 half-height) is
	// the one honest ruler in the room.
	constexpr float kBodyHeightCm = 176.f;
}

ASimNpc::ASimNpc()
{
	PrimaryActorTick.bCanEverTick = true;

	// Human-scale capsule (ACharacter's default radius 34 / half-height 88 —
	// already about right; kept explicit so the body lines up under it).
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);

	// The rigged body loads in InitSimIdentity (LoadObject at construction
	// time would run against the CDO); until then hide the inherited mesh
	// and stand a scaled cylinder in its place. No collision on the body —
	// the capsule blocks.
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

bool ASimNpc::ApplyCharacterBody()
{
	// Same hash the tint used: id -> stable variant, rain or re-import.
	const int32 Variant = static_cast<int32>(FCrc::StrCrc32(*NpcId.ToLower())) % kNumBodyVariants;
	const FString MeshPath = FString::Printf(
		TEXT("/Game/Art/Characters/Npc%d.Npc%d"), Variant, Variant);

	// Preferred: the rigged mesh on ACharacter's own skeletal component
	// (hidden in the constructor until exactly this moment).
	if (USkeletalMeshComponent* SkelComp = GetMesh())
	{
		if (USkeletalMesh* SkelMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath))
		{
			SkelComp->SetSkeletalMesh(SkelMesh);
			// glTF characters import facing UE +X (Interchange bakes the
			// glTF +Z-forward conversion); bOrientRotationToMovement turns
			// the actor — and the mesh with it — down the walking line.
			SkelComp->SetRelativeRotation(FRotator::ZeroRotator);
			// glTF origins sit at the feet: park them on the capsule floor.
			SkelComp->SetRelativeLocation(FVector(
				0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
			const float MeshHeightCm = SkelMesh->GetBounds().BoxExtent.Z * 2.f;
			if (MeshHeightCm > 1.f)
			{
				SkelComp->SetRelativeScale3D(FVector(kBodyHeightCm / MeshHeightCm));
			}
			SkelComp->SetVisibility(true);
			SkelComp->SetHiddenInGame(false);
			// The cylinder is hidden, not destroyed — a failed LoadObject
			// on the NEXT spawn still has a body to fall back to.
			if (BodyMesh)
			{
				BodyMesh->SetVisibility(false);
				BodyMesh->SetHiddenInGame(true);
			}
			WalkAnim = LoadObject<UAnimationAsset>(nullptr, *FString::Printf(
				TEXT("/Game/Art/Characters/Npc%d/Anims/Walk.Walk"), Variant));
			IdleAnim = LoadObject<UAnimationAsset>(nullptr, *FString::Printf(
				TEXT("/Game/Art/Characters/Npc%d/Anims/Idle.Idle"), Variant));
			bSkeletalBody = true;
			return true;
		}
	}

	// Second chance: a static-mesh export of the character (if the import
	// ever lands meshes-only) on the existing body component — a posed
	// body that slides still beats a cylinder.
	if (BodyMesh)
	{
		if (UStaticMesh* StaticBody = LoadObject<UStaticMesh>(nullptr, *MeshPath))
		{
			BodyMesh->SetStaticMesh(StaticBody);
			const FBox SourceBounds = StaticBody->GetBoundingBox();
			const float MeshHeightCm = SourceBounds.GetSize().Z;
			if (MeshHeightCm > 1.f)
			{
				const float Scale = kBodyHeightCm / MeshHeightCm;
				BodyMesh->SetRelativeScale3D(FVector(Scale));
				// Put the mesh's own box floor on the capsule floor,
				// wherever the artist put the pivot.
				BodyMesh->SetRelativeLocation(FVector(0.f, 0.f,
					-GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - Scale * SourceBounds.Min.Z));
			}
			return true;
		}
	}

	return false;  // nothing imported under that path — cylinder it is
}

void ASimNpc::InitSimIdentity(const FString& InNpcId)
{
	NpcId = InNpcId;
#if WITH_EDITOR
	// Actor labels are editor-only: the Game target has no SetActorLabel.
	SetActorLabel(FString::Printf(TEXT("Npc_%s"), *NpcId));
#endif

	if (BodyMesh == nullptr)
	{
		return;
	}

	if (ApplyCharacterBody())
	{
		// The imported bodies wear their own flat-colour materials
		// (D-023); the per-npc hash picks the VARIANT instead of a tint.
		return;
	}

	// Deterministic tint, cylinder fallback only: hash of the npc id ->
	// hue, on the engine's basic shape material (the one basic-shapes
	// material that carries a "Color" parameter). Best effort — if the
	// load fails the body stays grey.
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

void ASimNpc::UpdateBodyAnimation(const bool bMoving)
{
	if (!bSkeletalBody)
	{
		return;  // cylinder has no clips to switch
	}
	// Walk when walking, idle when not — falling back to whichever clip
	// exists if the import dropped one.
	UAnimationAsset* Wanted = bMoving
		? (WalkAnim != nullptr ? WalkAnim.Get() : IdleAnim.Get())
		: (IdleAnim != nullptr ? IdleAnim.Get() : WalkAnim.Get());
	if (Wanted == nullptr || Wanted == CurrentAnim)
	{
		return;
	}
	if (USkeletalMeshComponent* SkelComp = GetMesh())
	{
		SkelComp->PlayAnimation(Wanted, true);
		CurrentAnim = Wanted;
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
	const bool bWalking = ToTarget.SizeSquared() > FMath::Square(kArriveRadiusCm);
	if (bWalking)
	{
		ToTarget.Normalize();
		AddMovementInput(ToTarget, 1.f);
	}
	UpdateBodyAnimation(bWalking);
}
