// SimStreetBuilder.h — the Moon Gate Quarter street, built at runtime from
// the mudbrick house kit (tools/art/house_kit.py, imported to /Game/Art/Kit
// by tools/art/ue_import_kit.py) and the staged canon places table
// (Content/Sim/canon/places.csv — tools/stage_canon_for_ue.py). Replaces
// the grey-box placeholder cubes SimGameMode spawns today. D-023 art
// direction: low-poly kit pieces, flat-colour materials, no PBR textures.
//
// WIRING (SimGameMode::StartPlay, BEFORE Super::StartPlay() so the pawn
// finds the PlayerStart — this header's counterpart in the spec branch
// carried these lines; the game mode is not edited from the street wave):
//
//   const FSimStreetBuildResult Street = ASimStreetBuilder::BuildQuarter(World);
//   if (Street.bOk)
//   {
//       // tablet just inside the gate, PlayerStart facing it:
//       World->SpawnActor<ASimTablet>(Street.GateLocation + FVector(300.f, 0.f, 140.f), FRotator(0, 90, 0));
//       StreetStart = World->SpawnActor<APlayerStart>(Street.GateLocation + FVector(500.f, 0.f, 50.f), FRotator(0, 180, 0));
//   }
//
// BuildQuarter owns the whole street scene: kit buildings, the ground
// plane, and the sun/sky pair (tagged SimSun/SimSky, duplicate-guarded).
// Do NOT also port the spec branch's sun/sky spawn into the game mode —
// the guards here would then light the street twice.
//
// REGISTRIES (the other agents' entry points):
//   - W6-A (doors): every doorable place is in the door-slot registry
//     (GetDoorSlot/GetAllDoorSlots) AND has a tagged anchor actor in the
//     world (actor tag "SimDoorSlot", label "DoorSlot_<place_id>", root
//     rotation = the door's outward yaw).
//   - W6-C (NPCs): GetPlaceLocation resolves any place id the kernel's
//     sim_world_npc_place_at returns (the CSV ids are the sim's ids).
//
// DETERMINISM: same places.csv -> same street. The layout walks the
// district's CSV rows in file order (an authored street walk: gate, watch
// post, market square, stalls, bakery, brewery, temple, houses, well,
// shrine, granary), alternating plots either side of the street axis and
// turning north after a fixed number of plots. House variants hash the
// place id with FCrc::StrCrc32 (NOT GetTypeHash(FName) — name-table ids
// are not stable across sessions). Place ids and their registry entries
// stay stable even when later rows shift positions.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimStreetBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

/** One row of Content/Sim/canon/places.csv, as read at build time. */
USTRUCT()
struct FSimPlaceRow
{
	GENERATED_BODY()

	FName Id;
	FString City;
	FString District;
	FString Kind;
	FString BuildingId;
	FString OwnerPersonId;
	FString Name;
};

/** A place's door anchor: where a door (W6-A) or an NPC's building entry
 * (W6-C) resolves. OutwardYawDeg faces away from the building, toward the
 * street (0=+X, 90=+Y, 180=-X, 270=-Y). */
USTRUCT()
struct FSimDoorSlotInfo
{
	GENERATED_BODY()

	FName PlaceId;
	FString Kind;          // places.csv kind (gate, house, bakery, ...)
	FString DisplayName;  // places.csv name
	FString OwnerPersonId; // places.csv owner_person_id ("" when none)
	FVector Location = FVector::ZeroVector; // on the ground, 1 m outside the wall
	float OutwardYawDeg = 0.f;
};

/** What BuildQuarter produced (smoke evidence + wiring data). */
USTRUCT()
struct FSimStreetBuildResult
{
	GENERATED_BODY()

	/** A street stands (places.csv was found and read). */
	bool bOk = false;
	/** False when kit meshes were missing and engine-cube fallbacks were
	 * used — run tools/art/ue_import_kit.py (see tools/art/README.md). */
	bool bKitMeshesFound = true;
	int32 NumPlacesBuilt = 0;
	int32 NumDoorSlots = 0;
	/** Places standing as their generated building mesh (/Game/Art/Buildings, V-B1); the rest are kit rooms. */
	int32 NumBuildingMeshes = 0;
	/** The gate's world location (the PlayerStart and the first tablet
	 * place off this), or ZeroVector when the gate row is missing. */
	FVector GateLocation = FVector::ZeroVector;
};

UCLASS()
class SCHIZOGAME_API ASimStreetBuilder : public AActor
{
	GENERATED_BODY()

public:
	ASimStreetBuilder();

	/**
	 * Builds the Moon Gate Quarter into World (city_of_the_moon, district
	 * "Moon Gate Quarter" rows of Content/Sim/canon/places.csv): spawns one
	 * builder actor holding the kit instanced meshes, the ground plane, and
	 * the sun/sky pair, and fills the place/door registries. Idempotent per
	 * world — a second call returns the first call's result. No tick.
	 */
	static FSimStreetBuildResult BuildQuarter(UWorld* World);

	/** Place id -> world location: the door-facing point just outside the
	 * building for doorable places, else the footprint centre (market
	 * square, well, shrine niche, stalls). Returns false for unknown ids. */
	static bool GetPlaceLocation(FName PlaceId, FVector& OutLocation);

	/** The door slot for a doorable place (false for stalls/paved places
	 * and unknown ids). Same data as the world's tagged SimDoorSlot actors. */
	static bool GetDoorSlot(FName PlaceId, FSimDoorSlotInfo& OutSlot);

	/** Every door slot, in places.csv row order (the street walk). */
	static void GetAllDoorSlots(TArray<FSimDoorSlotInfo>& OutSlots);

	/** Every place id that produced geometry, in street-walk order. */
	static void GetAllPlaceIds(TArray<FName>& OutPlaceIds);

	/** False: build every room from the kit even where a generated building exists (tests compare the two). */
	static bool bUseBuildingMeshes;

	/** /Game/Art/Buildings/SM_B_<id> (tools/art/ue_import_buildings.py), or null when not imported. */
	static UStaticMesh* TryBuildingMesh(FName PlaceId);

	/** Where BuildRoom puts the door slot (local): the middle cell of the +y/-y run, 1 m outside. */
	static FVector KitDoorLocal(const FVector& Origin, int32 W, int32 D, bool bDoorPlusY);

private:
	// --- kit meshes (instanced static meshes; one component per piece) ---
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_WallPlain;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_WallDoor;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_WallWindow;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_RoofSlabFlat;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_RoofAccess;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_ParapetRun;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_Pilaster;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_Stair;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_Lintel;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_CourtyardTile;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_Awning;

	bool bKitMeshesFound = true;
	int32 NumBuildingMeshes = 0;
	void LoadKitMeshes();

	/** The current building's tint (M_Flat per-instance custom data). */
	FLinearColor CurrentTint = FLinearColor::White;
	/** Kit pillars + lintel at the gate (only without SimEnvironment's Moon Gate). */
	bool bLegacyGate = true;
	/** AddInstance + the current tint as the instance's custom data. */
	void AddPiece(UInstancedStaticMeshComponent* ISM, const FTransform& T);

	static TArray<FSimPlaceRow> LoadPlaces();

	/** Builds a walled room (perimeter of wall/door/window pieces) with
	 * footprint W x D grid cells, local min corner at Origin, door in the
	 * middle of DoorSide's run ("south" faces -Y, "north" +Y, "west" -X,
	 * "east" +X), one window opposite the door when bWindow. bRoofed tiles
	 * a flat roof (SM_RoofSlabFlat + a continuous SM_ParapetRun ring —
	 * assemble_house.py's build_flat_roof/build_parapet_ring look); a
	 * bRoofAccess roof has the ladder hole at the centre tile and an
	 * external stair against the south wall. Returns the door's world
	 * location (1 m outside the wall, on the ground) or the footprint
	 * centre when the room has no door. */
	FVector BuildRoom(const FVector& Origin, int32 W, int32 D, const FString& DoorSide,
		bool bWindow, bool bRoofAccess, bool bRoofed);

	/** An open paved area (courtyard tiles, no walls) — the market square,
	 * the well, the shrine niche. Returns the footprint centre. */
	FVector BuildPavedArea(const FVector& Origin, int32 W, int32 D);

	/** A single market stall: one courtyard tile + a reed awning over it,
	 * open on all sides (no door). Returns the tile centre. */
	FVector BuildStall(const FVector& Origin);

	/** Registers the slot and spawns its tagged anchor actor. */
	void SpawnDoorSlot(const FSimDoorSlotInfo& Slot);

	/** The moon gate: two chunky wall-piece pillars flanking a 2 m opening
	 * on the street axis, one timber lintel scaled across at pillar-top
	 * height. Returns the gate's centre (the build result's GateLocation). */
	FVector BuildGate(const FVector2D& Center);

	/** Ground plane sized to the laid-out street's bounds (engine cube —
	 * has collision), plus the duplicate-guarded sun/sky pair. */
	void BuildGroundAndLighting(const FVector2D& BoundsMin, const FVector2D& BoundsMax);

	FSimStreetBuildResult Build();

	// --- registries (static: one street per process at this slice; a
	// second BuildQuarter in a fresh world rebuilds them) ---
	/** The building being laid out: every piece and door slot goes through this frame (AA5). */
	FTransform CurrentFrame = FTransform::Identity;

	static TMap<FName, FVector> PlaceLocations;
	static TMap<FName, FSimDoorSlotInfo> DoorSlots;
	static TArray<FName> PlaceOrder;
	static FSimStreetBuildResult LastResult;
};
