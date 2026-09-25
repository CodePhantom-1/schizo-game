// SimPropsBuilder.cpp — see SimPropsBuilder.h. All invented specifics
// (prop set, placement rules, sky/post values) are logged in
// docs/proposals/invented-ledger-dressing.md.
#include "SimPropsBuilder.h"

#include "SchizoGame.h"
#include "SimStreetBuilder.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Scene.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimPropsBuilder, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarPropsDressing(
		TEXT("sim.PropsDressing"),
		1,
		TEXT("Dress the street: props, the sky dome and the post polish (dressing wave)."));

	// The dome's radius, cm — matches SM_SkyDome's modelled radius
	// (tools/art/props_gen.py build_sky_dome) and M_SkyDome's DomeRadius
	// default (tools/art/ue_import_props.py).
	constexpr float kDomeRadiusCm = 12000.f;

	// The street's courtyard tile tops out at TILE_THICK (kit_common) —
	// stall goods stand on it, not in it.
	constexpr float kStallTopZ = 8.f;

	// D-023 flat colours per FBX material slot (the props kit's palette,
	// tools/art/props_gen.py PROP_MAT_DEFS + the kit's own four) — same
	// pattern as SimStreetBuilder::LoadKitMeshes: the engine's parameter
	// material carries the slot colour until a content pass authors real
	// materials. sRGB values as FColor.
	const TMap<FName, FColor>& SlotColors()
	{
		static const TMap<FName, FColor> Colors = {
			{TEXT("M_MudPlaster"),  FColor(194, 158, 107)},
			{TEXT("M_Mudbrick"),    FColor(140, 102,  69)},
			{TEXT("M_Timber"),      FColor( 77,  48,  26)},
			{TEXT("M_Reed"),        FColor(173, 148,  77)},
			{TEXT("M_Clay"),        FColor(161,  92,  56)},
			{TEXT("M_Dates"),       FColor( 82,  41,  13)},
			{TEXT("M_Bread"),       FColor(184, 128,  72)},
			{TEXT("M_Hide"),        FColor(140, 107,  77)},
			{TEXT("M_Grain"),       FColor(199, 166,  90)},
			{TEXT("M_SkyDomeSlot"), FColor(154, 178, 204)},
		};
		return Colors;
	}

	/** Deterministic ±~26 degree yaw jitter per place (quantised to 7.5). */
	float YawJig(const FName& PlaceId)
	{
		return static_cast<float>(FCrc::StrCrc32(*PlaceId.ToString()) % 8) * 7.5f - 26.25f;
	}

	/** Spawn one prop mesh by kit id. Null-checked load: a missing mesh is
	 * recorded in Missing and the prop is skipped SILENTLY (the props import
	 * is an optional step — the street must never depend on it). Returns the
	 * spawned actor or null. */
	AStaticMeshActor* SpawnProp(UWorld* World, const TCHAR* MeshId, const FVector& Location, float YawDeg,
		bool bCollide, TSet<FName>& Missing)
	{
		const FString Path = FString::Printf(TEXT("/Game/Art/Props/Meshes/%s.%s"), MeshId, MeshId);
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
		if (Mesh == nullptr)
		{
			Missing.Add(FName(MeshId));
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Prop = World->SpawnActor<AStaticMeshActor>(Location, FRotator(0.f, YawDeg, 0.f), Params);
		if (Prop == nullptr)
		{
			return nullptr;
		}
#if WITH_EDITOR
		Prop->SetActorLabel(FString::Printf(TEXT("Dressing_%s"), MeshId));
#endif
		// Runtime-spawned statics must be movable to register after begin play
		// (same as the street builder's ground cube).
		Prop->SetMobility(EComponentMobility::Movable);
		if (UStaticMeshComponent* Comp = Prop->GetStaticMeshComponent())
		{
			Comp->SetMobility(EComponentMobility::Movable);
			Comp->SetStaticMesh(Mesh);
			Comp->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			if (UMaterial* Base = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial")))
			{
				const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
				for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
				{
					if (const FColor* Color = SlotColors().Find(Slots[Slot].MaterialSlotName))
					{
						if (UMaterialInstanceDynamic* Mic = Comp->CreateDynamicMaterialInstance(Slot, Base))
						{
							Mic->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(*Color));
						}
					}
				}
			}
		}
		return Prop;
	}
}  // namespace

// ---------------------------------------------------------------------------
// ASimSkyDome

ASimSkyDome::ASimSkyDome()
{
	PrimaryActorTick.bCanEverTick = false;
	DomeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DomeMesh"));
	RootComponent = DomeMesh;
	// Spawned after begin play: movable registers; static would not draw.
	DomeMesh->SetMobility(EComponentMobility::Movable);
	DomeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DomeMesh->SetCastShadow(false);
}

void ASimSkyDome::SetupDome(float RadiusCm)
{
	// Prefer the kit's dome (modelled at exactly kDomeRadiusCm, normals
	// already inward); the engine sphere is the pre-import fallback, scaled
	// to the same radius so one DomeRadius value drives the material for both.
	UStaticMesh* Dome = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/Props/Meshes/SM_SkyDome.SM_SkyDome"));
	float Scale = 1.f;
	if (Dome == nullptr)
	{
		Dome = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		Scale = RadiusCm / 50.f;  // the engine sphere is 50 cm radius
	}
	if (Dome == nullptr)
	{
		UE_LOG(LogSimPropsBuilder, Warning, TEXT("Sky dome: no dome mesh (kit import AND engine sphere failed)."));
		return;
	}
	DomeMesh->SetStaticMesh(Dome);
	if (!FMath::IsNearlyEqual(Scale, 1.f))
	{
		DomeMesh->SetWorldScale3D(FVector(Scale));
	}
	if (UMaterial* SkyMat = LoadObject<UMaterial>(nullptr, TEXT("/Game/Art/Sky/M_SkyDome.M_SkyDome")))
	{
		if (UMaterialInstanceDynamic* Mic = DomeMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mic->SetScalarParameterValue(TEXT("DomeRadius"), RadiusCm);
		}
	}
	else
	{
		// Headless material authoring is a separate optional step: a flat
		// pale sky still beats the black void.
		UE_LOG(LogSimPropsBuilder, Warning,
			TEXT("Sky dome: M_SkyDome not imported (run tools/art/ue_import_props.py) — flat fallback colour."));
		if (UMaterial* Base = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial")))
		{
			DomeMesh->SetMaterial(0, Base);
			if (UMaterialInstanceDynamic* Mic = DomeMesh->CreateAndSetMaterialInstanceDynamic(0))
			{
				Mic->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.65f, 0.78f));
			}
		}
	}
}

// ---------------------------------------------------------------------------
// USimPropsBuilder

void USimPropsBuilder::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bDressed || CVarPropsDressing.GetValueOnGameThread() == 0)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (World == nullptr || !World->HasBegunPlay())
	{
		return;
	}

	// The registries fill during the game mode's StartPlay (before the first
	// world tick); the brief wait only guards unusual init orders.
	TArray<FName> PlaceIds;
	ASimStreetBuilder::GetAllPlaceIds(PlaceIds);
	if (PlaceIds.Num() == 0)
	{
		WaitedForStreetSeconds += DeltaTime;
		if (WaitedForStreetSeconds < 5.f)
		{
			return;
		}
	}

	bDressed = true;
	// The painted dome (a 120 m shell — it walled off everything past the
	// street) and this pass's post volume are superseded by SimDayNight's
	// physical sky and grade; two unbound post volumes would also fight.
	// -SimLegacySky brings them back.
	if (FParse::Param(FCommandLine::Get(), TEXT("SimLegacySky")))
	{
		SpawnSkyDome();
		SpawnPostPolish();
	}
	const int32 NumProps = PlaceIds.Num() > 0 ? PlaceStreetProps() : 0;
	UE_LOG(LogSimPropsBuilder, Log,
		TEXT("Dressing pass: sky dome + post polish up%s, %d prop(s) placed."),
		PlaceIds.Num() > 0 ? TEXT("") : TEXT(" (no street — props skipped)"), NumProps);
}

void USimPropsBuilder::SpawnSkyDome()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	// The dome centres on the street (the average place), so the horizon
	// stays put around the whole walk; no street -> the world origin.
	FVector Centre = FVector::ZeroVector;
	int32 Num = 0;
	TArray<FName> PlaceIds;
	ASimStreetBuilder::GetAllPlaceIds(PlaceIds);
	for (const FName& Id : PlaceIds)
	{
		FVector Loc;
		if (ASimStreetBuilder::GetPlaceLocation(Id, Loc))
		{
			Centre += Loc;
			++Num;
		}
	}
	if (Num > 0)
	{
		Centre /= static_cast<float>(Num);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASimSkyDome* Dome = World->SpawnActor<ASimSkyDome>(Centre, FRotator::ZeroRotator, Params);
	if (Dome == nullptr)
	{
		UE_LOG(LogSimPropsBuilder, Warning, TEXT("Sky dome spawn failed — the sky stays a void."));
		return;
	}
#if WITH_EDITOR
	Dome->SetActorLabel(TEXT("SimSkyDome"));
#endif
	Dome->SetupDome(kDomeRadiusCm);
}

void USimPropsBuilder::SpawnPostPolish()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APostProcessVolume* Volume = World->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Volume == nullptr)
	{
		UE_LOG(LogSimPropsBuilder, Warning, TEXT("Post process volume spawn failed — no image polish."));
		return;
	}
#if WITH_EDITOR
	Volume->SetActorLabel(TEXT("SimPostPolish"));
#endif
	Volume->bUnbound = true;
	Volume->BlendWeight = 1.f;
	// Conservative D-023 grade — the look is carried by grading, not
	// fidelity. Values logged in docs/proposals/invented-ledger-dressing.md.
	FPostProcessSettings& P = Volume->Settings;
	P.bOverride_WhiteTemp = true;         // warm-white balance (6500 neutral)
	P.WhiteTemp = 6000.f;
	P.bOverride_VignetteIntensity = true; // a slight eye-draw to the street
	P.VignetteIntensity = 0.5f;
	P.bOverride_ColorSaturation = true;   // mild filmic saturation (5.8: per-channel)
	P.ColorSaturation = FVector4(1.1f, 1.1f, 1.1f, 1.1f);
	P.bOverride_ColorContrast = true;
	P.ColorContrast = FVector4(1.05f, 1.05f, 1.05f, 1.05f);
}

int32 USimPropsBuilder::PlaceStreetProps()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return 0;
	}
	TSet<FName> Missing;
	int32 Placed = 0;
	auto Try = [World, &Missing, &Placed](const TCHAR* MeshId, const FVector& Location, float YawDeg, bool bCollide)
	{
		if (SpawnProp(World, MeshId, Location, YawDeg, bCollide, Missing) != nullptr)
		{
			++Placed;
		}
	};

	// --- doorable places: kind + the door's outward axis from the slot registry.
	TArray<FSimDoorSlotInfo> Slots;
	ASimStreetBuilder::GetAllDoorSlots(Slots);
	for (const FSimDoorSlotInfo& Slot : Slots)
	{
		const FName Id = Slot.PlaceId;
		const FVector Outward = FRotator(0.f, Slot.OutwardYawDeg, 0.f).Vector();
		const FVector Right = FRotator(0.f, Slot.OutwardYawDeg + 90.f, 0.f).Vector();
		const FVector& S = Slot.Location;  // on the ground, 1 m outside the wall
		const float Jig = YawJig(Id);
		// Props sit BESIDE the doorway (55 cm off the wall face, 85 cm to
		// one side), never in the door's path. Always the door's +Right
		// side: for south doors (outward 270) that is +X, clear of the
		// roof-access stair which hugs the wall runs' west end.
		const FVector Beside = S - Outward * 55.f + Right * 85.f;

		if (Slot.Kind == TEXT("house"))
		{
			// Every door gets a bench or a rolled mat beside it (which one
			// hashed per house), yawed with the door, roughly wall-parallel.
			const float WallYaw = Slot.OutwardYawDeg + 90.f;
			if (FCrc::StrCrc32(*Id.ToString()) % 3 == 0)
			{
				Try(TEXT("SM_PropBench"), Beside, WallYaw + Jig, true);
			}
			else
			{
				Try(TEXT("SM_PropMatRoll"), Beside, WallYaw + Jig, true);
			}
		}
		else if (Slot.Kind == TEXT("bakery"))
		{
			// The sunrise bread, out for sale either side of the door.
			Try(TEXT("SM_PropBreadTray"), S - Outward * 70.f + Right * 85.f, Slot.OutwardYawDeg + 90.f + Jig, false);
			Try(TEXT("SM_PropBreadTray"), S - Outward * 70.f - Right * 85.f, Slot.OutwardYawDeg + 90.f - Jig, false);
		}
		else if (Slot.Kind == TEXT("granary"))
		{
			// The ration measure, stacked either side of its door.
			Try(TEXT("SM_PropJarStack"), S - Outward * 70.f + Right * 80.f, Jig, true);
			Try(TEXT("SM_PropJarStack"), S - Outward * 70.f - Right * 80.f, 180.f + Jig, true);
		}
		else if (Slot.Kind == TEXT("temple"))
		{
			// An offering brazier one side, rolled worshippers' mats the other.
			Try(TEXT("SM_PropBrazier"), S - Outward * 55.f - Right * 110.f, Jig, true);
			const FVector MatSpot = S - Outward * 75.f + Right * 115.f;
			Try(TEXT("SM_PropMatRoll"), MatSpot, Slot.OutwardYawDeg + 90.f + Jig, true);
			Try(TEXT("SM_PropMatRoll"), MatSpot + FVector(0.f, 0.f, 18.f), Slot.OutwardYawDeg + 90.f - Jig, false);
		}
		else if (Slot.Kind == TEXT("smithy"))
		{
			// The yard's fire (the furnace itself is the smithy wave's).
			Try(TEXT("SM_PropBrazier"), S - Outward * 55.f - Right * 105.f, Jig, true);
		}
		else if (Slot.Kind == TEXT("brewery/tavern"))
		{
			// A drinking bench and a beer jar by the door.
			Try(TEXT("SM_PropBench"), S - Outward * 55.f + Right * 85.f, Slot.OutwardYawDeg + 90.f, true);
			Try(TEXT("SM_PropAmphora"), S - Outward * 70.f - Right * 100.f, Jig, true);
		}
		else if (Slot.Kind == TEXT("watch post"))
		{
			// The night watch's fire.
			Try(TEXT("SM_PropBrazier"), S - Outward * 55.f + Right * 90.f, Jig, true);
		}
		else if (Slot.Kind == TEXT("gate"))
		{
			// Two braziers flank the opening, just inside the city (the
			// street leaves the gate along +X; the walk's plots sit at Y 0,
			// so Y +/-160 clears both the street and the pillars).
			Try(TEXT("SM_PropBrazier"), S + FVector(60.f, 160.f, 0.f), Jig, true);
			Try(TEXT("SM_PropBrazier"), S + FVector(60.f, -160.f, 0.f), -Jig, true);
		}
	}

	// --- paved places carry no door slot: the canon's own ids name them.
	TArray<FName> PlaceIds;
	ASimStreetBuilder::GetAllPlaceIds(PlaceIds);
	for (const FName& Id : PlaceIds)
	{
		const FString IdStr = Id.ToString();
		FVector Loc;
		if (!ASimStreetBuilder::GetPlaceLocation(Id, Loc))
		{
			continue;
		}
		const float Jig = YawJig(Id);
		const uint32 Hash = FCrc::StrCrc32(*IdStr);

		const bool bStall = IdStr.StartsWith(TEXT("stall_")) || IdStr == TEXT("cookshop_place");
		if (bStall)
		{
			// Goods under the awning, on the stall tile (z = tile top).
			// One basket every stall; the counter goods vary by trader.
			Try(TEXT("SM_PropBasket"), Loc + FVector(-25.f, -10.f, kStallTopZ), Jig, false);
			switch (Hash % 4)
			{
			case 0:  // the produce stall: dates in a tray
				Try(TEXT("SM_PropDatesTray"), Loc + FVector(25.f, 8.f, kStallTopZ), Jig, false);
				break;
			case 1:  // the cook-shop: the noon bread
				Try(TEXT("SM_PropBreadLoaves"), Loc + FVector(25.f, 5.f, kStallTopZ), Jig, false);
				break;
			case 2:  // the pottery stall's big jar...
				Try(TEXT("SM_PropAmphora"), Loc + FVector(30.f, 0.f, kStallTopZ), Jig, true);
				break;
			default:  // ...and the grain/salt stalls' stores
				Try(TEXT("SM_PropJarStack"), Loc + FVector(28.f, 5.f, kStallTopZ), Jig, true);
				break;
			}
		}
		else if (IdStr == TEXT("street_well_place"))
		{
			// Jars for the water-carriers, a bench to wait on, a filled
			// waterskin on its post — all just off the well's 1 m tile.
			Try(TEXT("SM_PropBench"), Loc + FVector(0.f, -115.f, 0.f), Jig, true);
			Try(TEXT("SM_PropAmphora"), Loc + FVector(85.f, 30.f, 0.f), Jig, true);
			Try(TEXT("SM_PropAmphora"), Loc + FVector(-80.f, 55.f, 0.f), -Jig, true);
			Try(TEXT("SM_PropWaterSkin"), Loc + FVector(105.f, -75.f, 0.f), Jig, true);
		}
	}

	if (Missing.Num() > 0)
	{
		FString Joined;
		for (const FName& Id : Missing)
		{
			if (!Joined.IsEmpty())
			{
				Joined += TEXT(", ");
			}
			Joined += Id.ToString();
		}
		UE_LOG(LogSimPropsBuilder, Log,
			TEXT("Props builder: %d prop(s) placed; skipped missing meshes: %s (run tools/art/ue_import_props.py)."),
			Placed, *Joined);
	}
	else
	{
		UE_LOG(LogSimPropsBuilder, Log, TEXT("Props builder: %d prop(s) placed."), Placed);
	}
	return Placed;
}

TStatId USimPropsBuilder::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USimPropsBuilder, STATGROUP_Tickables);
}

bool USimPropsBuilder::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
