// SimCharacter.cpp — see SimCharacter.h.
#include "SimCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

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

void ASimCharacter::BeginPlay()
{
	Super::BeginPlay();

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
