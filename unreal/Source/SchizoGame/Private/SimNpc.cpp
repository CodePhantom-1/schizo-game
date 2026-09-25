// SimNpc.cpp — see SimNpc.h. Grey-box body; walks straight at the target
// (no nav mesh yet — the street is one line, so straight-line steering is
// honest for the slice; upgrade to nav when the street grows corners).
#include "SimNpc.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimNpc, Log, All);

ASimNpc::ASimNpc()
{
	PrimaryActorTick.bCanEverTick = true;

	// Human-scale capsule (ACharacter's default: radius 34, half-height 88 —
	// already about right; kept explicit so the grey-box body lines up).
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);

	// No skeleton exists yet: hide the inherited SkeletalMeshComponent and
	// stand a scaled cylinder in its place (per-role tint below).
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		Skel->SetVisibility(false);
		Skel->SetHiddenInGame(true);
	}

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // the capsule already collides
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderAsset.Object);
	}
	// Engine cylinder is ~100x100x100; scale to a human silhouette (~60cm
	// across, ~180cm tall) and drop it so its base sits on the capsule floor.
	BodyMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.8f));
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -88.f + 90.f));

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = 140.f;  // roughly human walking pace, cm/s
		Move->bOrientRotationToMovement = true;
	}
}

void ASimNpc::BeginPlay()
{
	Super::BeginPlay();
	TargetLocation = GetActorLocation();
}

void ASimNpc::InitResident(const FString& InNpcId, const FString& InResidentName, const FString& InRole)
{
	NpcId = InNpcId;
	ResidentName = InResidentName;
	Role = InRole;
	SetActorLabel(*FString::Printf(TEXT("Npc_%s"), *InNpcId));

	// Per-role tint: deterministic hash of the role string -> hue. Best
	// effort — a material without a "Color" param just stays the engine
	// default cylinder grey (no crash either way).
	if (BodyMesh && BodyMesh->GetMaterial(0))
	{
		UMaterialInstanceDynamic* Dyn = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (Dyn != nullptr)
		{
			const uint32 Hash = GetTypeHash(Role);
			const float Hue = static_cast<float>(Hash % 360u);
			const FLinearColor Tint = FLinearColor::MakeFromHSV8(
				static_cast<uint8>(Hue * 255.f / 360.f), 200, 200);
			Dyn->SetVectorParameterValue(TEXT("Color"), Tint);
			Dyn->SetVectorParameterValue(TEXT("BaseColor"), Tint);
		}
	}
}

void ASimNpc::SetTargetLocation(const FVector& NewTarget)
{
	TargetLocation = NewTarget;
}

void ASimNpc::SetCurrentTask(const FString& ScheduleId, const FString& TaskText)
{
	if (ScheduleId == CurrentScheduleId) return;  // no change — nothing to log
	CurrentScheduleId = ScheduleId;
	CurrentTaskText = TaskText;
	UE_LOG(LogSimNpc, Log, TEXT("%s (%s, %s): %s [%s]"), *ResidentName, *NpcId, *Role, *TaskText, *ScheduleId);
}

void ASimNpc::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FVector ToTarget = TargetLocation - GetActorLocation();
	ToTarget.Z = 0.f;
	const float DistSq = ToTarget.SizeSquared();
	if (DistSq > FMath::Square(50.f))  // close enough — stop walking on top of the spot
	{
		ToTarget.Normalize();
		AddMovementInput(ToTarget, 1.0f);
	}
}
