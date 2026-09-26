// SimTablet.cpp — reading is a skill, not a given (rpg-systems §2.5).
#include "SimTablet.h"
#include "UI/SimShellSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimTablet, Log, All);

namespace
{
	// Debug literacy until the Scripts skill exists (rpg-systems §2.5):
	// sim.PlayerReads 1 — the engine-side skill data is a slice task.
	TAutoConsoleVariable<int32> CVarPlayerReads(
		TEXT("sim.PlayerReads"),
		0,
		TEXT("Whether the player can read cuneiform (0/1). Debug until the Scripts skill lands."));
}

ASimTablet::ASimTablet()
{
	PrimaryActorTick.bCanEverTick = false;

	UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TabletMesh"));
	RootComponent = Mesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
		Mesh->SetWorldScale3D(FVector(0.3f, 0.05f, 0.4f));
		// D-023 flat colour (tools/art/ue_make_style_materials.py), not the engine checker.
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> StyleMat(TEXT("/Game/Art/Style/MI_Tablet.MI_Tablet"));
		if (StyleMat.Succeeded())
		{
			Mesh->SetMaterial(0, StyleMat.Object);
		}
	}
}

bool ASimTablet::PlayerReads()
{
	return CVarPlayerReads.GetValueOnGameThread() != 0;
}

void ASimTablet::Interact()
{
	USimShellSubsystem* Shell = USimShellSubsystem::Get(this);
	if (PlayerReads())
	{
		// Readable: the canon line itself, in the tablet reader (A8).
		UE_LOG(LogSimTablet, Log, TEXT("Read: %s"), *CanonText);
		if (Shell != nullptr)
		{
			Shell->ShowTablet(NSLOCTEXT("SimUi", "TabletTitle", "A clay tablet"), FText::FromString(CanonText));
		}
	}
	else
	{
		// Unreadable: the real script, beautiful and alien (game-design §8).
		UE_LOG(LogSimTablet, Log, TEXT("Unreadable: %s"), *CanonText);
		if (Shell != nullptr)
		{
			Shell->ShowTablet(NSLOCTEXT("SimUi", "TabletTitle", "A clay tablet"),
				NSLOCTEXT("SimUi", "Unreadable", "Wedges in clay — a script you cannot yet read."));
		}
	}
}
