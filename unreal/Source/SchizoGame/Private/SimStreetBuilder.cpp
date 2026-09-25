// SimStreetBuilder.cpp — see SimStreetBuilder.h.
#include "SimStreetBuilder.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimStreetBuilder, Log, All);

TMap<FName, FVector> ASimStreetBuilder::PlaceLocations;

namespace
{
	// Kit grid, cm (100 u = 1 m — matches tools/art/kit_common.py's GRID=1.0 m,
	// WALL_THICK=0.4 m, WALL_HEIGHT=2.6 m, ROOF_THICK=0.15 m + the parapet lip).
	constexpr float GRID = 100.f;
	constexpr float WALL_THICK = 40.f;
	constexpr float WALL_HEIGHT = 260.f;
	constexpr float ROOF_Z_THICK = 45.f;

	UInstancedStaticMeshComponent* MakeISM(AActor* Owner, USceneComponent* Root, const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* C = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetMobility(EComponentMobility::Static);
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		return C;
	}

	UStaticMesh* LoadKitMesh(const TCHAR* Id)
	{
		const FString Path = FString::Printf(TEXT("/Game/Art/Kit/Meshes/%s.%s"), Id, Id);
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
		if (Mesh == nullptr)
		{
			UE_LOG(LogSimStreetBuilder, Warning, TEXT("kit mesh not imported: %s (run tools/art/ue_import_kit.py) — that building piece will be invisible."), *Path);
		}
		return Mesh;
	}

	/** Mirrors tools/art/assemble_house.py's place(): a piece's local origin
	 * is its bottom-back-left corner, so rotating it about that origin swings
	 * its footprint away from the target cell unless the translation is
	 * compensated. sx/sy is the piece's own local (width, thickness)
	 * footprint before rotation. */
	FTransform PieceTransform(float TargetX, float TargetY, float Z, float RotDegZ, float Sx, float Sy, FVector ScaleXYZ = FVector::OneVector)
	{
		int32 R = FMath::RoundToInt(RotDegZ) % 360;
		if (R < 0) R += 360;
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

	/** Minimal quoted-CSV line split (db/canon/places.csv has no escaped
	 * quotes inside quoted fields, so "" escaping is not needed here). */
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
	RootComponent = Root;

	ISM_WallPlain = MakeISM(this, Root, TEXT("ISM_WallPlain"));
	ISM_WallDoor = MakeISM(this, Root, TEXT("ISM_WallDoor"));
	ISM_WallWindow = MakeISM(this, Root, TEXT("ISM_WallWindow"));
	ISM_Corner = MakeISM(this, Root, TEXT("ISM_Corner"));
	ISM_RoofSlab = MakeISM(this, Root, TEXT("ISM_RoofSlab"));
	ISM_RoofAccess = MakeISM(this, Root, TEXT("ISM_RoofAccess"));
	ISM_Pilaster = MakeISM(this, Root, TEXT("ISM_Pilaster"));
	ISM_Stair = MakeISM(this, Root, TEXT("ISM_Stair"));
	ISM_Lintel = MakeISM(this, Root, TEXT("ISM_Lintel"));
	ISM_CourtyardTile = MakeISM(this, Root, TEXT("ISM_CourtyardTile"));
	ISM_Awning = MakeISM(this, Root, TEXT("ISM_Awning"));
}

void ASimStreetBuilder::LoadKitMeshes()
{
	if (bKitLoaded)
	{
		return;
	}
	ISM_WallPlain->SetStaticMesh(LoadKitMesh(TEXT("SM_WallPlain")));
	ISM_WallDoor->SetStaticMesh(LoadKitMesh(TEXT("SM_WallDoor")));
	ISM_WallWindow->SetStaticMesh(LoadKitMesh(TEXT("SM_WallWindow")));
	ISM_Corner->SetStaticMesh(LoadKitMesh(TEXT("SM_Corner")));
	ISM_RoofSlab->SetStaticMesh(LoadKitMesh(TEXT("SM_RoofSlab")));
	ISM_RoofAccess->SetStaticMesh(LoadKitMesh(TEXT("SM_RoofAccess")));
	ISM_Pilaster->SetStaticMesh(LoadKitMesh(TEXT("SM_Pilaster")));
	ISM_Stair->SetStaticMesh(LoadKitMesh(TEXT("SM_Stair")));
	ISM_Lintel->SetStaticMesh(LoadKitMesh(TEXT("SM_Lintel")));
	ISM_CourtyardTile->SetStaticMesh(LoadKitMesh(TEXT("SM_CourtyardTile")));
	ISM_Awning->SetStaticMesh(LoadKitMesh(TEXT("SM_Awning")));
	bKitLoaded = true;
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
	// header: id,city,district,kind,building_id,owner_person_id,name,description,tag,source_ref
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
		Row.Kind = F[3];
		Row.BuildingId = F[4];
		Row.Name = F[6];
		Rows.Add(Row);
	}
	return Rows;
}

FVector ASimStreetBuilder::BuildRoom(const FVector& Origin, int32 W, int32 D, const FString& DoorSide, bool bWindow,
	bool bRoofAccess)
{
	const float X0 = Origin.X, Y0 = Origin.Y, Z0 = Origin.Z;
	const int32 MidX = W / 2, MidY = D / 2;
	FString WindowSide;
	if (bWindow)
	{
		if (DoorSide == TEXT("south")) WindowSide = TEXT("north");
		else if (DoorSide == TEXT("north")) WindowSide = TEXT("south");
		else if (DoorSide == TEXT("east")) WindowSide = TEXT("west");
		else if (DoorSide == TEXT("west")) WindowSide = TEXT("east");
	}

	FVector DoorPoint = Origin + FVector(W * GRID * 0.5f, D * GRID * 0.5f, 0.f); // fallback: footprint centre

	auto PlaceWall = [&](const FString& Side, int32 Idx, float TargetX, float TargetY, float RotDeg)
	{
		UInstancedStaticMeshComponent* ISM = ISM_WallPlain;
		const bool bIsDoor = (Side == DoorSide) && ((Side == TEXT("south") || Side == TEXT("north")) ? Idx == MidX : Idx == MidY);
		const bool bIsWindow = !bIsDoor && (Side == WindowSide) && ((Side == TEXT("south") || Side == TEXT("north")) ? Idx == MidX : Idx == MidY);
		if (bIsDoor) ISM = ISM_WallDoor;
		else if (bIsWindow) ISM = ISM_WallWindow;
		ISM->AddInstance(PieceTransform(TargetX, TargetY, Z0, RotDeg, GRID, WALL_THICK));
		if (bIsDoor)
		{
			const float Cx = (Side == TEXT("south") || Side == TEXT("north")) ? TargetX + GRID * 0.5f : TargetX;
			const float Cy = (Side == TEXT("east") || Side == TEXT("west")) ? TargetY + GRID * 0.5f : TargetY;
			if (Side == TEXT("south")) DoorPoint = FVector(Cx, Y0 - GRID, Z0);
			else if (Side == TEXT("north")) DoorPoint = FVector(Cx, Y0 + D * GRID + GRID, Z0);
			else if (Side == TEXT("west")) DoorPoint = FVector(X0 - GRID, Cy, Z0);
			else if (Side == TEXT("east")) DoorPoint = FVector(X0 + W * GRID + GRID, Cy, Z0);
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

	// Roof: uniform tiling with the parapet piece everywhere (see the
	// ponytail note on BuildRoom in the header).
	const int32 AccessI = W / 2, AccessJ = D / 2;
	for (int32 i = 0; i < W; ++i)
	{
		for (int32 j = 0; j < D; ++j)
		{
			UInstancedStaticMeshComponent* RoofISM = (bRoofAccess && i == AccessI && j == AccessJ) ? ISM_RoofAccess : ISM_RoofSlab;
			RoofISM->AddInstance(PieceTransform(X0 + i * GRID, Y0 + j * GRID, Z0 + WALL_HEIGHT, 0.f, GRID, GRID));
		}
	}

	if (bRoofAccess)
	{
		// The stair to the roof, against the outer south wall.
		ISM_Stair->AddInstance(PieceTransform(X0, Y0 - GRID * 2.f, Z0, 0.f, GRID, GRID * 2.f));
	}

	return DoorPoint;
}

FVector ASimStreetBuilder::BuildPavedArea(const FVector& Origin, int32 W, int32 D)
{
	for (int32 i = 0; i < W; ++i)
	{
		for (int32 j = 0; j < D; ++j)
		{
			ISM_CourtyardTile->AddInstance(PieceTransform(Origin.X + i * GRID, Origin.Y + j * GRID, Origin.Z, 0.f, GRID, GRID));
		}
	}
	return Origin + FVector(W * GRID * 0.5f, D * GRID * 0.5f, 0.f);
}

FVector ASimStreetBuilder::BuildStall(const FVector& Origin)
{
	ISM_CourtyardTile->AddInstance(PieceTransform(Origin.X, Origin.Y, Origin.Z, 0.f, GRID, GRID));
	ISM_Awning->AddInstance(PieceTransform(Origin.X, Origin.Y, Origin.Z, 0.f, GRID, GRID));
	return Origin + FVector(GRID * 0.5f, GRID * 0.5f, 0.f);
}

void ASimStreetBuilder::SpawnDoorSlot(const FVector& Location, float OutwardYawDeg, FName PlaceId)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Slot = World->SpawnActor<AActor>(AActor::StaticClass(),
		FTransform(FRotator(0.f, OutwardYawDeg, 0.f), Location), Params);
	if (Slot == nullptr)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Slot);
	Slot->SetRootComponent(Root);
	Root->RegisterComponent();
	Slot->Tags.Add(FName(TEXT("SimDoorSlot")));
	Slot->SetActorLabel(FString::Printf(TEXT("DoorSlot_%s"), *PlaceId.ToString()));
	++NumDoorSlots;
}

FVector ASimStreetBuilder::Build()
{
	LoadKitMeshes();
	TArray<FSimPlaceRow> Places = LoadPlaces();
	PlaceLocations.Empty();
	NumBuildingsBuilt = 0;
	NumDoorSlots = 0;

	// --- ground plane (once per world). The sun (tagged SimSun) and sky are
	// SimGameMode::StartPlay's job, alongside this actor's own Build() call. ---
	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		bool bHasGround = false;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetActorLabel() == TEXT("MoonGateQuarter_Ground")) { bHasGround = true; break; }
		}
		if (!bHasGround)
		{
			if (AStaticMeshActor* Ground = World->SpawnActor<AStaticMeshActor>(FVector(2500.f, 0.f, -1.f), FRotator::ZeroRotator))
			{
				Ground->SetMobility(EComponentMobility::Static);
				if (UStaticMeshComponent* Mesh = Ground->GetStaticMeshComponent())
				{
					if (UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
					{
						Mesh->SetStaticMesh(Plane);
					}
					Mesh->SetWorldScale3D(FVector(100.f, 100.f, 1.f)); // ~100x100 m ground
					Mesh->SetMobility(EComponentMobility::Static);
				}
				Ground->SetActorLabel(TEXT("MoonGateQuarter_Ground"));
			}
		}
	}

	// --- street layout: walk places.csv in order, one turn (D-005 slice) ---
	constexpr float PLOT = 800.f;   // 8 m between plot centres along the street
	constexpr float OFFSET = 500.f; // 5 m from the street centreline to a plot centre
	constexpr int32 TurnAt = 14;    // after the houses start (index 14 = bakers_house_place)

	bool bAlongY = false;
	float Cursor = 0.f;
	float TurnBaseX = 0.f, TurnBaseY = 0.f;
	FVector GateLoc = FVector::ZeroVector;

	for (int32 i = 0; i < Places.Num(); ++i)
	{
		const FSimPlaceRow& Row = Places[i];
		if (i == TurnAt)
		{
			bAlongY = true;
			TurnBaseX = Cursor; // the street's X position at the turn
			TurnBaseY = 0.f;
			Cursor = 0.f;
		}
		const bool bPosSide = (i % 2 == 0);
		FVector2D Center;
		FString DoorSide;
		float OutwardYaw = 0.f;
		if (!bAlongY)
		{
			Center = FVector2D(Cursor, bPosSide ? OFFSET : -OFFSET);
			DoorSide = bPosSide ? TEXT("south") : TEXT("north"); // door faces the street centreline
			OutwardYaw = bPosSide ? 270.f : 90.f;
		}
		else
		{
			Center = FVector2D(TurnBaseX + (bPosSide ? OFFSET : -OFFSET), TurnBaseY + Cursor);
			DoorSide = bPosSide ? TEXT("west") : TEXT("east");
			OutwardYaw = bPosSide ? 180.f : 0.f;
		}
		Cursor += PLOT;

		// --- footprint + kind dispatch ---
		int32 W = 2, D = 2;
		bool bWindow = true;
		bool bRoofAccess = false;
		bool bPaved = false;
		bool bStall = false;
		bool bGate = false;

		if (Row.Kind == TEXT("gate")) { bGate = true; W = 2; D = 1; }
		else if (Row.Kind == TEXT("watch post")) { W = 2; D = 2; bWindow = false; }
		else if (Row.Kind == TEXT("market")) { bPaved = true; W = 6; D = 6; }
		else if (Row.Kind == TEXT("market stall") || Row.Kind == TEXT("cook-shop")) { bStall = true; W = 1; D = 1; }
		else if (Row.Kind == TEXT("bakery")) { W = 3; D = 3; }
		else if (Row.Kind == TEXT("brewery/tavern")) { W = 4; D = 3; }
		else if (Row.Kind == TEXT("temple")) { W = 4; D = 4; }
		else if (Row.Kind == TEXT("house"))
		{
			const uint32 Variant = GetTypeHash(Row.Id) % 3;
			if (Variant == 0) { W = 3; D = 3; } // small single-room
			else if (Variant == 1) { W = 4; D = 4; } // two-room + courtyard (simplified footprint — see header ponytail note)
			else { W = 3; D = 3; bRoofAccess = true; bWindow = false; } // roof access
		}
		else if (Row.Kind == TEXT("well") || Row.Kind == TEXT("shrine")) { bPaved = true; W = 1; D = 1; }
		else if (Row.Kind == TEXT("granary")) { W = 2; D = 2; bWindow = false; }
		else { W = 2; D = 2; }

		const FVector Origin(Center.X - W * GRID * 0.5f, Center.Y - D * GRID * 0.5f, 0.f);
		FVector PlaceLoc;

		if (bGate)
		{
			// The moon gate: two wall-piece pillars flanking a 2 m opening,
			// bridged by a lintel — the street's threshold, not a room.
			ISM_WallPlain->AddInstance(PieceTransform(Origin.X, Origin.Y, 0.f, 0.f, GRID, WALL_THICK));
			ISM_WallPlain->AddInstance(PieceTransform(Origin.X + (W - 1) * GRID, Origin.Y, 0.f, 0.f, GRID, WALL_THICK));
			ISM_Lintel->AddInstance(PieceTransform(Origin.X, Origin.Y, WALL_HEIGHT, 0.f, GRID * W, WALL_THICK, FVector(static_cast<float>(W), 1.f, 1.f)));
			PlaceLoc = FVector(Center.X, Center.Y, 0.f);
			GateLoc = FVector(Center.X, Center.Y, 100.f);
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
			PlaceLoc = BuildRoom(Origin, W, D, DoorSide, bWindow, bRoofAccess);
			SpawnDoorSlot(PlaceLoc, OutwardYaw, Row.Id);
		}

		PlaceLocations.Add(Row.Id, PlaceLoc);
		++NumBuildingsBuilt;
		UE_LOG(LogSimStreetBuilder, Log, TEXT("place registry: %s (%s) -> (%.0f,%.0f,%.0f)"),
			*Row.Id.ToString(), *Row.Kind, PlaceLoc.X, PlaceLoc.Y, PlaceLoc.Z);
	}

	UE_LOG(LogSimStreetBuilder, Log, TEXT("The Moon Gate Quarter stands: %d buildings from %d places, %d door slots."),
		NumBuildingsBuilt, Places.Num(), NumDoorSlots);

	return GateLoc;
}

FVector ASimStreetBuilder::GetPlaceLocation(FName PlaceId)
{
	if (const FVector* Loc = PlaceLocations.Find(PlaceId))
	{
		return *Loc;
	}
	UE_LOG(LogSimStreetBuilder, Warning, TEXT("GetPlaceLocation: unknown place id '%s'"), *PlaceId.ToString());
	return FVector::ZeroVector;
}
