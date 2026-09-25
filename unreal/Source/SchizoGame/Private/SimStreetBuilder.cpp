// SimStreetBuilder.cpp — see SimStreetBuilder.h.
#include "SimStreetBuilder.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimStreetBuilder, Log, All);

TMap<FName, FVector> ASimStreetBuilder::PlaceLocations;
TMap<FName, FSimDoorSlotInfo> ASimStreetBuilder::DoorSlots;
TArray<FName> ASimStreetBuilder::PlaceOrder;
FSimStreetBuildResult ASimStreetBuilder::LastResult;

namespace
{
	// Kit grid, cm — mirrors tools/art/kit_common.py (GRID=1.0 m, WALL_THICK
	// 0.4 m, WALL_HEIGHT 2.6 m, ROOF_THICK 0.15 m, PARAPET_H/T 0.3/0.1 m).
	constexpr float GRID = 100.f;
	constexpr float WALL_THICK = 40.f;
	constexpr float WALL_HEIGHT = 260.f;
	constexpr float ROOF_THICK = 15.f;
	constexpr float PARAPET_H = 30.f;
	constexpr float PARAPET_T = 10.f;

	// Street layout, cm: plot centres every 8 m along the walk, 5 m either
	// side of the street axis; the street turns north after 14 plots.
	constexpr float PLOT = 800.f;
	constexpr float OFFSET = 500.f;
	constexpr int32 PLOTS_BEFORE_TURN = 14;

	UInstancedStaticMeshComponent* MakeISM(AActor* Owner, USceneComponent* Root, const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* C = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetMobility(EComponentMobility::Static);
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->NumCustomDataFloats = 3;  // per-house tint (M_Flat's PerInstanceCustomData 0..2)
		return C;
	}

	UStaticMesh* LoadKitMesh(const TCHAR* Id)
	{
		const FString Path = FString::Printf(TEXT("/Game/Art/Kit/Meshes/%s.%s"), Id, Id);
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
		if (Mesh == nullptr)
		{
			UE_LOG(LogSimStreetBuilder, Warning,
				TEXT("kit mesh not imported: %s (run tools/art/ue_import_kit.py, see tools/art/README.md) — engine-cube fallback for that piece."),
				*Path);
		}
		return Mesh;
	}

	/** Mirrors assemble_house.py's place(): a kit piece's local origin is its
	 * bottom-back-left corner, so rotating it about that origin swings its
	 * footprint away from the target cell unless the translation is
	 * compensated. Sx/Sy is the piece's EFFECTIVE local (length, thickness)
	 * footprint AFTER ScaleXYZ (scaled pieces must pass their scaled extents,
	 * or rotated placements land one whole unscaled length off). */
	FTransform PieceTransform(float TargetX, float TargetY, float Z, float RotDegZ, float Sx, float Sy,
		const FVector& ScaleXYZ = FVector::OneVector)
	{
		int32 R = FMath::RoundToInt(RotDegZ) % 360;
		if (R < 0)
		{
			R += 360;
		}
		float Ox = 0.f, Oy = 0.f;
		if (R == 90) { Ox = Sy; Oy = 0.f; }
		else if (R == 180) { Ox = Sx; Oy = Sy; }
		else if (R == 270) { Ox = 0.f; Oy = Sx; }
		FTransform T;
		T.SetLocation(FVector(TargetX + Ox, TargetY + Oy, Z));
		T.SetRotation(FQuat(FRotator(0.f, RotDegZ, 0.f)));
		T.SetScale3D(ScaleXYZ);
		return T;
	}

	/** Minimal quoted-CSV line split (the canon CSVs have no escaped quotes
	 * inside quoted fields, so "" handling is not needed here). */
	TArray<FString> SplitCsvLine(const FString& Line)
	{
		TArray<FString> Out;
		FString Cur;
		bool bInQuotes = false;
		for (int32 i = 0; i < Line.Len(); ++i)
		{
			const TCHAR C = Line[i];
			if (C == '"') { bInQuotes = !bInQuotes; continue; }
			if (C == ',' && !bInQuotes) { Out.Add(Cur); Cur.Empty(); continue; }
			Cur.AppendChar(C);
		}
		Out.Add(Cur);
		return Out;
	}
}

ASimStreetBuilder::ASimStreetBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	// Static under static: the street never moves, and a movable root would
	// reject the static-mobility ISM children at registration.
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;

	ISM_WallPlain = MakeISM(this, Root, TEXT("ISM_WallPlain"));
	ISM_WallDoor = MakeISM(this, Root, TEXT("ISM_WallDoor"));
	ISM_WallWindow = MakeISM(this, Root, TEXT("ISM_WallWindow"));
	ISM_RoofSlabFlat = MakeISM(this, Root, TEXT("ISM_RoofSlabFlat"));
	ISM_RoofAccess = MakeISM(this, Root, TEXT("ISM_RoofAccess"));
	ISM_ParapetRun = MakeISM(this, Root, TEXT("ISM_ParapetRun"));
	ISM_Pilaster = MakeISM(this, Root, TEXT("ISM_Pilaster"));
	ISM_Stair = MakeISM(this, Root, TEXT("ISM_Stair"));
	ISM_Lintel = MakeISM(this, Root, TEXT("ISM_Lintel"));
	ISM_CourtyardTile = MakeISM(this, Root, TEXT("ISM_CourtyardTile"));
	ISM_Awning = MakeISM(this, Root, TEXT("ISM_Awning"));
}

void ASimStreetBuilder::LoadKitMeshes()
{
	// D-023: every kit slot is a flat colour on the style master M_Flat
	// (tools/art/ue_make_style_materials.py) — the imported photo-texture
	// materials (the archived A-1 path) read as yellow planks at this scale.
	// Each house tints its pieces through per-instance custom data
	// (CurrentTint, see AddPiece), so the street is not one uniform beige.
	const TMap<FName, FColor> SlotColors = {
		{TEXT("M_MudPlaster"), FColor(216, 200, 170)},
		{TEXT("M_Mudbrick"),   FColor(158, 128, 100)},
		{TEXT("M_Timber"),     FColor( 88,  58,  36)},
		{TEXT("M_Reed"),       FColor(192, 162,  92)},
	};
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Style/M_Flat.M_Flat"));
	if (Base == nullptr)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	}
	auto LoadInto = [this, &SlotColors, Base](UInstancedStaticMeshComponent* ISM, const TCHAR* Id)
	{
		UStaticMesh* Mesh = LoadKitMesh(Id);
		if (Mesh == nullptr)
		{
			bKitMeshesFound = false;
			// The grey-box stand-in: the street still stands (and collides)
			// before the one-line kit import has been run.
			Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		}
		if (ISM != nullptr && Mesh != nullptr)
		{
			ISM->SetStaticMesh(Mesh);
			const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
			for (int32 Slot = 0; Slot < Slots.Num() && Base != nullptr; ++Slot)
			{
				const FColor* Color = SlotColors.Find(Slots[Slot].MaterialSlotName);
				if (Color == nullptr)
				{
					Color = SlotColors.Find(TEXT("M_MudPlaster"));  // engine-cube fallback: plaster
				}
				if (UMaterialInstanceDynamic* Mic = ISM->CreateDynamicMaterialInstance(Slot, Base))
				{
					Mic->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(*Color));
				}
			}
		}
	};
	LoadInto(ISM_WallPlain, TEXT("SM_WallPlain"));
	LoadInto(ISM_WallDoor, TEXT("SM_WallDoor"));
	LoadInto(ISM_WallWindow, TEXT("SM_WallWindow"));
	LoadInto(ISM_RoofSlabFlat, TEXT("SM_RoofSlabFlat"));
	LoadInto(ISM_RoofAccess, TEXT("SM_RoofAccess"));
	LoadInto(ISM_ParapetRun, TEXT("SM_ParapetRun"));
	LoadInto(ISM_Pilaster, TEXT("SM_Pilaster"));
	LoadInto(ISM_Stair, TEXT("SM_Stair"));
	LoadInto(ISM_Lintel, TEXT("SM_Lintel"));
	LoadInto(ISM_CourtyardTile, TEXT("SM_CourtyardTile"));
	LoadInto(ISM_Awning, TEXT("SM_Awning"));
}

TArray<FSimPlaceRow> ASimStreetBuilder::LoadPlaces()
{
	TArray<FSimPlaceRow> Rows;
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Sim/canon/places.csv"));
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
	{
		UE_LOG(LogSimStreetBuilder, Error, TEXT("places.csv missing at %s — run tools/stage_canon_for_ue.py."), *Path);
		return Rows;
	}
	// header: id,city,district,kind,building_id,owner_person_id,name,...
	// Only this wave's slice district is laid out; the other districts
	// (Lighthouse Wharf, Beyond the Moon Gate) belong to later scenes.
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		if (Lines[i].IsEmpty())
		{
			continue;
		}
		TArray<FString> F = SplitCsvLine(Lines[i]);
		if (F.Num() < 7)
		{
			continue;
		}
		FSimPlaceRow Row;
		Row.Id = FName(*F[0]);
		Row.City = F[1];
		Row.District = F[2];
		Row.Kind = F[3];
		Row.BuildingId = F[4];
		Row.OwnerPersonId = F[5];
		Row.Name = F[6];
		if (Row.City == TEXT("city_of_the_moon") && Row.District == TEXT("Moon Gate Quarter"))
		{
			Rows.Add(Row);
		}
	}
	return Rows;
}

FVector ASimStreetBuilder::BuildRoom(const FVector& Origin, int32 W, int32 D, const FString& DoorSide,
	bool bWindow, bool bRoofAccess, bool bRoofed)
{
	const float X0 = Origin.X, Y0 = Origin.Y, Z0 = Origin.Z;
	const int32 MidX = W / 2, MidY = D / 2;
	FString WindowSide;
	if (bWindow)
	{
		if (DoorSide == TEXT("south")) { WindowSide = TEXT("north"); }
		else if (DoorSide == TEXT("north")) { WindowSide = TEXT("south"); }
		else if (DoorSide == TEXT("east")) { WindowSide = TEXT("west"); }
		else if (DoorSide == TEXT("west")) { WindowSide = TEXT("east"); }
	}

	FVector DoorPoint = Origin + FVector(W * GRID * 0.5f, D * GRID * 0.5f, 0.f); // fallback: footprint centre

	auto PlaceWall = [&](const FString& Side, int32 Idx, float TargetX, float TargetY, float RotDeg)
	{
		const bool bRunsAlongX = (Side == TEXT("south") || Side == TEXT("north"));
		const bool bIsDoor = (Side == DoorSide) && (Idx == (bRunsAlongX ? MidX : MidY));
		const bool bIsWindow = !bIsDoor && (Side == WindowSide) && (Idx == (bRunsAlongX ? MidX : MidY));
		UInstancedStaticMeshComponent* ISM = bIsDoor ? ISM_WallDoor : (bIsWindow ? ISM_WallWindow : ISM_WallPlain);
		AddPiece(ISM, PieceTransform(TargetX, TargetY, Z0, RotDeg, GRID, WALL_THICK));
		if (bIsDoor)
		{
			// The door piece's cut is centred on its own cell; the anchor
			// point stands 1 m outside the wall, on the ground.
			const float Cx = bRunsAlongX ? TargetX + GRID * 0.5f : TargetX;
			const float Cy = (Side == TEXT("east") || Side == TEXT("west")) ? TargetY + GRID * 0.5f : TargetY;
			if (Side == TEXT("south")) { DoorPoint = FVector(Cx, Y0 - GRID, Z0); }
			else if (Side == TEXT("north")) { DoorPoint = FVector(Cx, Y0 + D * GRID + GRID, Z0); }
			else if (Side == TEXT("west")) { DoorPoint = FVector(X0 - GRID, Cy, Z0); }
			else if (Side == TEXT("east")) { DoorPoint = FVector(X0 + W * GRID + GRID, Cy, Z0); }
		}
	};

	for (int32 i = 0; i < W; ++i)
	{
		PlaceWall(TEXT("south"), i, X0 + i * GRID, Y0, 0.f);
		PlaceWall(TEXT("north"), i, X0 + i * GRID, Y0 + D * GRID - WALL_THICK, 180.f);
	}
	for (int32 j = 0; j < D; ++j)
	{
		PlaceWall(TEXT("west"), j, X0, Y0 + j * GRID, 270.f);
		PlaceWall(TEXT("east"), j, X0 + W * GRID - WALL_THICK, Y0 + j * GRID, 90.f);
	}

	if (bRoofed)
	{
		// Flat tiles everywhere (assemble_house.py's build_flat_roof), with
		// the ladder hole at the centre tile when the house sleeps on the roof.
		const int32 AccessI = W / 2, AccessJ = D / 2;
		for (int32 i = 0; i < W; ++i)
		{
			for (int32 j = 0; j < D; ++j)
			{
				UInstancedStaticMeshComponent* RoofISM =
					(bRoofAccess && i == AccessI && j == AccessJ) ? ISM_RoofAccess : ISM_RoofSlabFlat;
				AddPiece(RoofISM, PieceTransform(X0 + i * GRID, Y0 + j * GRID, Z0 + WALL_HEIGHT, 0.f, GRID, GRID));
			}
		}
		// One continuous parapet ring around the outer edge (the runtime
		// counterpart of build_parapet_ring): south/north runs own the
		// corners, west/east run between them, each run one X-scaled metre
		// piece — the stretch is invisible with flat-colour materials.
		const float ZP = Z0 + WALL_HEIGHT + ROOF_THICK;
		const float L = D * GRID - 2.f * PARAPET_T; // west/east run length
		AddPiece(ISM_ParapetRun, PieceTransform(X0, Y0, ZP, 0.f, W * GRID, PARAPET_T, FVector(W, 1.f, 1.f)));
		AddPiece(ISM_ParapetRun, PieceTransform(X0, Y0 + D * GRID - PARAPET_T, ZP, 0.f, W * GRID, PARAPET_T, FVector(W, 1.f, 1.f)));
		AddPiece(ISM_ParapetRun, PieceTransform(X0, Y0 + PARAPET_T, ZP, 90.f, L, PARAPET_T, FVector(L / GRID, 1.f, 1.f)));
		AddPiece(ISM_ParapetRun, PieceTransform(X0 + W * GRID - PARAPET_T, Y0 + PARAPET_T, ZP, 270.f, L, PARAPET_T, FVector(L / GRID, 1.f, 1.f)));
	}

	if (bRoofAccess)
	{
		// The external stair to the roof, against the south wall's west end.
		AddPiece(ISM_Stair, PieceTransform(X0, Y0 - 2.f * GRID, Z0, 0.f, GRID, 2.f * GRID));
	}

	return DoorPoint;
}

FVector ASimStreetBuilder::BuildPavedArea(const FVector& Origin, int32 W, int32 D)
{
	for (int32 i = 0; i < W; ++i)
	{
		for (int32 j = 0; j < D; ++j)
		{
			AddPiece(ISM_CourtyardTile, PieceTransform(Origin.X + i * GRID, Origin.Y + j * GRID, Origin.Z, 0.f, GRID, GRID));
		}
	}
	return Origin + FVector(W * GRID * 0.5f, D * GRID * 0.5f, 0.f);
}

FVector ASimStreetBuilder::BuildStall(const FVector& Origin)
{
	AddPiece(ISM_CourtyardTile, PieceTransform(Origin.X, Origin.Y, Origin.Z, 0.f, GRID, GRID));
	AddPiece(ISM_Awning, PieceTransform(Origin.X, Origin.Y, Origin.Z, 0.f, GRID, GRID));
	return Origin + FVector(GRID * 0.5f, GRID * 0.5f, 0.f);
}

void ASimStreetBuilder::SpawnDoorSlot(const FSimDoorSlotInfo& Slot)
{
	DoorSlots.Add(Slot.PlaceId, Slot);
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Anchor = World->SpawnActor<AActor>(AActor::StaticClass(),
		FTransform(FRotator(0.f, Slot.OutwardYawDeg, 0.f), Slot.Location), Params);
	if (Anchor == nullptr)
	{
		UE_LOG(LogSimStreetBuilder, Warning, TEXT("door-slot anchor spawn failed for %s."), *Slot.PlaceId.ToString());
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Anchor);
	if (Root != nullptr)
	{
		Anchor->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	Anchor->Tags.Add(FName(TEXT("SimDoorSlot")));
#if WITH_EDITOR
	// Actor labels are editor-only: the Game target has no SetActorLabel.
	Anchor->SetActorLabel(FString::Printf(TEXT("DoorSlot_%s"), *Slot.PlaceId.ToString()));
#endif
}

FVector ASimStreetBuilder::BuildGate(const FVector2D& Center)
{
	// Two chunky pillars (1x1 m, full wall height) flanking the street's
	// 2 m passage: the street runs along X, so the pillars stand NORTH and
	// SOUTH of the centreline (Y +100..+200 and -200..-100) and the player
	// walks between them along X. The timber lintel bridges the passage
	// across Y at pillar-top height (the lintel piece's own geometry sits
	// at door-head height above its origin, so z-scale 1.3 = 260/200 lifts
	// it from 2.0 m to exactly the wall top). Audit P1-1: the previous
	// orientation flanked along X and blocked the street centreline.
	// The monumental Moon Gate is SimEnvironment's (towers, lintel, lapis,
	// crescent); the kit pillars stay only when no environment stands.
	if (bLegacyGate)
	{
	AddPiece(ISM_WallPlain, PieceTransform(Center.X - GRID * 0.5f, Center.Y + GRID, 0.f, 0.f, GRID, GRID));
	AddPiece(ISM_WallPlain, PieceTransform(Center.X - GRID * 0.5f, Center.Y - 2.f * GRID, 0.f, 0.f, GRID, GRID));
	AddPiece(ISM_Lintel, PieceTransform(Center.X - WALL_THICK * 0.5f, Center.Y - 110.f, 0.f, 0.f, WALL_THICK, 220.f, FVector(1.f, 1.f, 1.3f)));
	}
	return FVector(Center.X, Center.Y, 0.f);
}

void ASimStreetBuilder::BuildGroundAndLighting(const FVector2D& BoundsMin, const FVector2D& BoundsMax)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// --- ground: one engine cube sized to the street's bounds (20 m head
	// room), top face at z=0. The cube (not the plane) so collision exists.
	auto HasTag = [](const AActor* Actor, FName Tag)
	{
		return Actor != nullptr && Actor->Tags.Contains(Tag);
	};
	bool bHasGround = false;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (HasTag(*It, FName(TEXT("SimGround")))) { bHasGround = true; break; }
	}
	if (!bHasGround)
	{
		constexpr float Margin = 2000.f;
		const FVector Center((BoundsMin.X + BoundsMax.X) * 0.5f, (BoundsMin.Y + BoundsMax.Y) * 0.5f, -10.f);
		const FVector Scale(
			(BoundsMax.X - BoundsMin.X + 2.f * Margin) / 100.f,
			(BoundsMax.Y - BoundsMin.Y + 2.f * Margin) / 100.f,
			0.2f);
		if (AStaticMeshActor* Ground = World->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator))
		{
			Ground->Tags.Add(FName(TEXT("SimGround")));
			Ground->SetMobility(EComponentMobility::Movable);
			if (UStaticMeshComponent* Mesh = Ground->GetStaticMeshComponent())
			{
				if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
				{
					Mesh->SetStaticMesh(Cube);
				}
				Mesh->SetWorldScale3D(Scale);
				Mesh->SetMobility(EComponentMobility::Movable);
				Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				// Packed earth, not the engine's checkerboard: the authored
				// M_Ground (CC0 Ground109, tiled — ue_import_kit.py) when the
				// art import ran; the flat colour MIC as fallback otherwise.
				if (UMaterial* GroundMat = LoadObject<UMaterial>(nullptr, TEXT("/Game/Art/Kit/Materials/M_Ground.M_Ground")))
				{
					Mesh->SetMaterial(0, GroundMat);
				}
				else if (UMaterial* Base = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial")))
				{
					if (UMaterialInstanceDynamic* GroundMic = Mesh->CreateDynamicMaterialInstance(0, Base))
					{
						GroundMic->SetVectorParameterValue(TEXT("Color"),
							FLinearColor::FromSRGBColor(FColor(168, 138, 96)));
					}
				}
			}
#if WITH_EDITOR
			Ground->SetActorLabel(TEXT("MoonGateQuarter_Ground"));
#endif
		}
	}

	// Light and sky: SimDayNight owns them (sun, moon, sky light, fog, grade).
}

void ASimStreetBuilder::AddPiece(UInstancedStaticMeshComponent* ISM, const FTransform& T)
{
	const int32 I = ISM->AddInstance(T);
	ISM->SetCustomDataValue(I, 0, CurrentTint.R, false);
	ISM->SetCustomDataValue(I, 1, CurrentTint.G, false);
	ISM->SetCustomDataValue(I, 2, CurrentTint.B, true);
}

FSimStreetBuildResult ASimStreetBuilder::Build()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->Tags.Contains(FName(TEXT("SimGround"))))
		{
			bLegacyGate = false;  // the environment stands: its Moon Gate is the gate
			break;
		}
	}
	LoadKitMeshes();
	TArray<FSimPlaceRow> Places = LoadPlaces();
	PlaceLocations.Empty();
	DoorSlots.Empty();
	PlaceOrder.Empty();

	FSimStreetBuildResult Result;
	Result.bKitMeshesFound = bKitMeshesFound;
	if (Places.Num() == 0)
	{
		UE_LOG(LogSimStreetBuilder, Error, TEXT("no 'Moon Gate Quarter' places in places.csv — the street was not built."));
		LastResult = Result;
		return Result;
	}

	bool bAlongY = false;
	float Cursor = 0.f;
	float TurnBaseX = 0.f;
	int32 PlotsLaid = 0;
	FVector GateLoc = FVector::ZeroVector;
	bool bHaveGate = false;
	FVector2D BoundsMin = FVector2D::ZeroVector, BoundsMax = FVector2D::ZeroVector;
	bool bHaveBounds = false;

	for (const FSimPlaceRow& Row : Places)
	{
		if (PlotsLaid == PLOTS_BEFORE_TURN)
		{
			bAlongY = true;
			TurnBaseX = Cursor; // the street's X position at the turn
			Cursor = 0.f;
		}

		// Each building's lime wash / mud tone (INVENTED dressing; content hash so the street is stable).
		{
			static const FLinearColor Tints[] = {
				FLinearColor(1.06f, 1.05f, 1.02f), FLinearColor(1.f, 0.9f, 0.76f), FLinearColor(0.93f, 0.82f, 0.7f),
				FLinearColor(1.04f, 0.88f, 0.8f), FLinearColor(0.86f, 0.76f, 0.64f), FLinearColor(1.08f, 1.0f, 0.86f) };
			const uint32 H = FCrc::StrCrc32(*Row.Id.ToString());
			CurrentTint = (Row.Kind == TEXT("temple")) ? FLinearColor(1.12f, 1.1f, 1.08f) : Tints[H % 6];
		}
		const bool bGate = (Row.Kind == TEXT("gate"));
		const bool bPosSide = (PlotsLaid % 2 == 0);
		FVector2D Center;
		FString DoorSide;
		float OutwardYaw = 0.f;
		if (bGate)
		{
			// The gate straddles the street axis (not a plot offset to one
			// side) — the street runs through its opening.
			Center = bAlongY ? FVector2D(TurnBaseX, Cursor) : FVector2D(Cursor, 0.f);
			DoorSide = bAlongY ? TEXT("west") : TEXT("south"); // only used for slot bookkeeping
			OutwardYaw = bAlongY ? 270.f : 180.f; // faces back down the street, out of the city
		}
		else if (!bAlongY)
		{
			Center = FVector2D(Cursor, bPosSide ? OFFSET : -OFFSET);
			DoorSide = bPosSide ? TEXT("south") : TEXT("north"); // door faces the street centreline
			OutwardYaw = bPosSide ? 270.f : 90.f;
		}
		else
		{
			Center = FVector2D(TurnBaseX + (bPosSide ? OFFSET : -OFFSET), Cursor);
			DoorSide = bPosSide ? TEXT("west") : TEXT("east");
			OutwardYaw = bPosSide ? 180.f : 0.f;
		}
		Cursor += PLOT;
		++PlotsLaid;

		// --- footprint + kind dispatch ---
		int32 W = 2, D = 2;
		bool bWindow = true;
		bool bRoofAccess = false;
		bool bRoofed = true;
		bool bPaved = false;
		bool bStall = false;

		if (Row.Kind == TEXT("watch post")) { W = 2; D = 2; bWindow = false; }
		else if (Row.Kind == TEXT("market")) { bPaved = true; W = 6; D = 6; }
		else if (Row.Kind == TEXT("market stall") || Row.Kind == TEXT("cook-shop")) { bStall = true; W = 1; D = 1; }
		else if (Row.Kind == TEXT("bakery")) { W = 3; D = 3; }
		else if (Row.Kind == TEXT("brewery/tavern")) { W = 4; D = 3; }
		else if (Row.Kind == TEXT("temple")) { W = 4; D = 4; }
		else if (Row.Kind == TEXT("house"))
		{
			// buildings.csv already says which house it is: the courtyard
			// house is the larger plan. The modest one-room houses vary by a
			// content hash — FCrc::StrCrc32, not GetTypeHash(FName): name-table
			// ids are not stable across sessions, and the street must be
			// (same CSV -> same houses, so place ids keep pointing at the
			// same buildings).
			if (Row.BuildingId == TEXT("mudbrick_house_courtyard")) { W = 4; D = 4; }
			else if (FCrc::StrCrc32(*Row.Id.ToString()) % 3 == 2)
			{
				W = 3; D = 3; bRoofAccess = true; bWindow = false; // roof access (roof-top living)
			}
			else { W = 3; D = 3; } // small single-room
		}
		else if (Row.Kind == TEXT("well") || Row.Kind == TEXT("shrine")) { bPaved = true; W = 1; D = 1; }
		else if (Row.Kind == TEXT("granary")) { W = 2; D = 2; bWindow = false; }
		else if (Row.Kind == TEXT("smithy")) { W = 3; D = 3; bWindow = false; bRoofed = false; } // a walled open yard with its furnace
		else { W = 2; D = 2; }

		const FVector Origin(Center.X - W * GRID * 0.5f, Center.Y - D * GRID * 0.5f, 0.f);

		if (!bHaveBounds)
		{
			BoundsMin = Center - FVector2D(W * GRID, D * GRID) * 0.5f;
			BoundsMax = Center + FVector2D(W * GRID, D * GRID) * 0.5f;
			bHaveBounds = true;
		}
		else
		{
			BoundsMin = FVector2D(FMath::Min(BoundsMin.X, Center.X - W * GRID * 0.5f), FMath::Min(BoundsMin.Y, Center.Y - D * GRID * 0.5f));
			BoundsMax = FVector2D(FMath::Max(BoundsMax.X, Center.X + W * GRID * 0.5f), FMath::Max(BoundsMax.Y, Center.Y + D * GRID * 0.5f));
		}

		FVector PlaceLoc;
		FSimDoorSlotInfo Slot;
		Slot.PlaceId = Row.Id;
		Slot.Kind = Row.Kind;
		Slot.DisplayName = Row.Name;
		Slot.OwnerPersonId = Row.OwnerPersonId;
		Slot.OutwardYawDeg = OutwardYaw;

		if (bGate)
		{
			PlaceLoc = BuildGate(Center);
			GateLoc = PlaceLoc + FVector(0.f, 0.f, 100.f);
			bHaveGate = true;
			Slot.Location = PlaceLoc;
			SpawnDoorSlot(Slot); // the gate opens at dawn and shuts at dusk — it is a door too
		}
		else if (bPaved)
		{
			PlaceLoc = BuildPavedArea(Origin, W, D);
		}
		else if (bStall)
		{
			PlaceLoc = BuildStall(Origin);
		}
		else
		{
			PlaceLoc = BuildRoom(Origin, W, D, DoorSide, bWindow, bRoofAccess, bRoofed);
			Slot.Location = PlaceLoc;
			SpawnDoorSlot(Slot);

			if (Row.Kind == TEXT("temple") && W >= 3)
			{
				// Engaged pilasters flanking the temple's door cell, on the
				// outside face of the street wall (pieces are 30x15 cm).
				const int32 M = (DoorSide == TEXT("south") || DoorSide == TEXT("north")) ? W / 2 : D / 2;
				for (int32 Side = -1; Side <= 1; Side += 2)
				{
					const float Off = (M + Side) * GRID + 35.f;
					if (DoorSide == TEXT("south"))
					{
						AddPiece(ISM_Pilaster, PieceTransform(Origin.X + Off, Origin.Y - 15.f, 0.f, 0.f, 30.f, 15.f));
					}
					else if (DoorSide == TEXT("north"))
					{
						AddPiece(ISM_Pilaster, PieceTransform(Origin.X + Off, Origin.Y + D * GRID, 0.f, 0.f, 30.f, 15.f));
					}
					else if (DoorSide == TEXT("west"))
					{
						AddPiece(ISM_Pilaster, PieceTransform(Origin.X - 15.f, Origin.Y + Off, 0.f, 90.f, 30.f, 15.f));
					}
					else if (DoorSide == TEXT("east"))
					{
						AddPiece(ISM_Pilaster, PieceTransform(Origin.X + W * GRID, Origin.Y + Off, 0.f, 270.f, 30.f, 15.f));
					}
				}
			}
		}

		PlaceLocations.Add(Row.Id, PlaceLoc);
		PlaceOrder.Add(Row.Id);
		UE_LOG(LogSimStreetBuilder, Log, TEXT("place registry: %s (%s) -> (%.0f,%.0f,%.0f)"),
			*Row.Id.ToString(), *Row.Kind, PlaceLoc.X, PlaceLoc.Y, PlaceLoc.Z);
	}

	BuildGroundAndLighting(BoundsMin, BoundsMax);

	Result.bOk = true;
	Result.NumPlacesBuilt = Places.Num();
	Result.NumDoorSlots = DoorSlots.Num();
	Result.GateLocation = bHaveGate ? GateLoc : FVector::ZeroVector;
	if (!bHaveGate)
	{
		UE_LOG(LogSimStreetBuilder, Warning, TEXT("no 'gate' place in the quarter — GateLocation is zero."));
	}
	UE_LOG(LogSimStreetBuilder, Log, TEXT("The Moon Gate Quarter stands: %d places, %d door slots%s."),
		Result.NumPlacesBuilt, Result.NumDoorSlots, Result.bKitMeshesFound ? TEXT("") : TEXT(" (kit meshes missing — engine-cube fallback)"));
	LastResult = Result;
	return Result;
}

FSimStreetBuildResult ASimStreetBuilder::BuildQuarter(UWorld* World)
{
	FSimStreetBuildResult Fail;
	if (World == nullptr)
	{
		UE_LOG(LogSimStreetBuilder, Warning, TEXT("BuildQuarter: null world."));
		return Fail;
	}
	TActorIterator<ASimStreetBuilder> Existing(World);
	if (Existing)
	{
		UE_LOG(LogSimStreetBuilder, Log, TEXT("BuildQuarter: a street already stands in this world — returning its result."));
		return LastResult;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASimStreetBuilder* Builder = World->SpawnActor<ASimStreetBuilder>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Builder == nullptr)
	{
		UE_LOG(LogSimStreetBuilder, Warning, TEXT("ASimStreetBuilder spawn failed — no street."));
		return Fail;
	}
	return Builder->Build();
}

bool ASimStreetBuilder::GetPlaceLocation(FName PlaceId, FVector& OutLocation)
{
	if (const FVector* Loc = PlaceLocations.Find(PlaceId))
	{
		OutLocation = *Loc;
		return true;
	}
	UE_LOG(LogSimStreetBuilder, Warning, TEXT("GetPlaceLocation: unknown place id '%s'"), *PlaceId.ToString());
	return false;
}

bool ASimStreetBuilder::GetDoorSlot(FName PlaceId, FSimDoorSlotInfo& OutSlot)
{
	if (const FSimDoorSlotInfo* Slot = DoorSlots.Find(PlaceId))
	{
		OutSlot = *Slot;
		return true;
	}
	return false;
}

void ASimStreetBuilder::GetAllDoorSlots(TArray<FSimDoorSlotInfo>& OutSlots)
{
	OutSlots.Empty();
	// In street-walk (places.csv row) order, not hash order.
	for (const FName& Id : PlaceOrder)
	{
		if (const FSimDoorSlotInfo* Slot = DoorSlots.Find(Id))
		{
			OutSlots.Add(*Slot);
		}
	}
}

void ASimStreetBuilder::GetAllPlaceIds(TArray<FName>& OutPlaceIds)
{
	OutPlaceIds = PlaceOrder;
}
