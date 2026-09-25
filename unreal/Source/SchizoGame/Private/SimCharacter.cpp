// SimCharacter.cpp — see SimCharacter.h.
#include "SimCharacter.h"

#include "Animation/AnimationAsset.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// ART-2 character pass: the imported traveller (Quaternius CC0 cast,
	// written by tools/art/ue_import_characters.py). Missing is NOT an
	// error — the programmer art below carries on until the import runs.
	const TCHAR* kTravellerMeshPath = TEXT("/Game/Art/Characters/Player.Player");
	const TCHAR* kTravellerWalkPath = TEXT("/Game/Art/Characters/Player/Anims/Walk.Walk");
	const TCHAR* kTravellerIdlePath = TEXT("/Game/Art/Characters/Player/Anims/Idle.Idle");

	// Bounds-normalize the imported body to the capsule (2 x 88 half-height)
	// whatever the importer did with the source's ~4x export scale.
	constexpr float kBodyHeightCm = 176.f;

	// Above this horizontal speed the traveller counts as walking (walk
	// speed is 420 cm/s, sprint 800 — the margin ignores float noise).
	constexpr float kMovingSpeedCm = 50.f;
}

ASimCharacter::ASimCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 176 cm, a traveller's shoulders; the camera turns the body with it.
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	bUseControllerRotationYaw = true;

	// The shoulder camera: behind and above, lagging a touch so walking
	// reads as walking.
	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(GetRootComponent());
	Boom->TargetArmLength = 340.f;
	Boom->SocketOffset = FVector(0.f, 0.f, 80.f);
	Boom->bUsePawnControlRotation = true;
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 12.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = true;

	// A modest stride; sprint doubles it (kernel fatigue is untouched).
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->JumpZVelocity = 420.f;

	// Programmer art: a cylinder body and a sphere head, both flat colours
	// in BeginPlay (dynamic materials need a world). Collides like the
	// capsule does, blocks the Use trace — the controller ignores self.
	// The rigged traveller replaces both in BeginPlay (ApplyTravellerBody).
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetRootComponent());
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(Cylinder.Object);
	}
	BodyMesh->SetWorldScale3D(FVector(0.66f, 0.66f, 1.5f));
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 0.f));

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(GetRootComponent());
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		HeadMesh->SetStaticMesh(Sphere.Object);
	}
	HeadMesh->SetWorldScale3D(FVector(0.44f, 0.44f, 0.44f));
	HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 78.f));
}

bool ASimCharacter::ApplyTravellerBody()
{
	// Preferred: the rigged mesh on ACharacter's own skeletal component
	// (idle until exactly this swap — BeginPlay, not the constructor, so
	// the CDO never loads content).
	if (USkeletalMeshComponent* SkelComp = GetMesh())
	{
		if (USkeletalMesh* SkelMesh = LoadObject<USkeletalMesh>(nullptr, kTravellerMeshPath))
		{
			SkelComp->SetSkeletalMesh(SkelMesh);
			// glTF characters import facing UE +X (Interchange bakes the
			// conversion); the controller's yaw turns the capsule — and
			// the body with it — under the shoulder camera.
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
			// The programmer art is hidden, not destroyed — a failed
			// LoadObject next session still has a body to fall back to.
			if (BodyMesh)
			{
				BodyMesh->SetVisibility(false);
				BodyMesh->SetHiddenInGame(true);
			}
			if (HeadMesh)
			{
				HeadMesh->SetVisibility(false);
				HeadMesh->SetHiddenInGame(true);
			}
			WalkAnim = LoadObject<UAnimationAsset>(nullptr, kTravellerWalkPath);
			IdleAnim = LoadObject<UAnimationAsset>(nullptr, kTravellerIdlePath);
			bSkeletalBody = true;
			return true;
		}
	}

	// Second chance: a static-mesh export of the traveller on the body
	// component (a posed body that slides still beats a cylinder).
	if (BodyMesh)
	{
		if (UStaticMesh* StaticBody = LoadObject<UStaticMesh>(nullptr, kTravellerMeshPath))
		{
			BodyMesh->SetStaticMesh(StaticBody);
			HeadMesh->SetVisibility(false);
			HeadMesh->SetHiddenInGame(true);
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

	return false;  // nothing imported under that path — programmer art it is
}

void ASimCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (ApplyTravellerBody())
	{
		// The traveller wears the model's own flat-colour garb (D-023):
		// sand-and-beige reads as the sun-bleached wool already.
		return;
	}

	// The traveller's plain clothes: sun-bleached wool over skin.
	const FColor BodyColor(146, 116, 78);
	const FColor HeadColor(150, 110, 84);
	if (UMaterial* Base = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial")))
	{
		if (UMaterialInstanceDynamic* BodyMic = BodyMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			BodyMic->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(BodyColor));
		}
		if (UMaterialInstanceDynamic* HeadMic = HeadMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			HeadMic->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(HeadColor));
		}
	}
}

void ASimCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Single-clip locomotion for the imported body (the cylinder body has
	// no clips and skips this inside UpdateBodyAnimation).
	UpdateBodyAnimation(GetVelocity().SizeSquared2D() > FMath::Square(kMovingSpeedCm));
}

void ASimCharacter::UpdateBodyAnimation(const bool bMoving)
{
	if (!bSkeletalBody)
	{
		return;
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

void ASimCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ASimCharacter::OnMoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ASimCharacter::OnMoveRight);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &ASimCharacter::OnSprintStart);
	PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &ASimCharacter::OnSprintEnd);
}

void ASimCharacter::OnMoveForward(const float AxisValue)
{
	if (Controller == nullptr || AxisValue == 0.f)
	{
		return;
	}
	// Forward by the CAMERA's yaw, not the body's, so the shoulder view and
	// the stride agree while the mouse looks around.
	const FRotator YawOnly(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(YawOnly.Vector(), AxisValue);
}

void ASimCharacter::OnMoveRight(const float AxisValue)
{
	if (Controller == nullptr || AxisValue == 0.f)
	{
		return;
	}
	const FRotator YawOnly(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawOnly).GetScaledAxis(EAxis::Y), AxisValue);
}

void ASimCharacter::OnSprintStart()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = SprintSpeed;
	}
}

void ASimCharacter::OnSprintEnd()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}
}
