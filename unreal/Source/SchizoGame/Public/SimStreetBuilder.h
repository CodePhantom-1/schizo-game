// SimStreetBuilder.h — the Moon Gate Quarter, built from the mudbrick kit
// pieces (tools/art/house_kit.py, imported via tools/art/ue_import_kit.py)
// and db/canon/places.csv (staged to Content/Sim/canon by
// tools/stage_canon_for_ue.py). Replaces the SimGameMode grey-box street.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimStreetBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

/** One row of db/canon/places.csv, as read at runtime. */
USTRUCT()
struct FSimPlaceRow
{
	GENERATED_BODY()

	FName Id;
	FString Kind;
	FString BuildingId;
	FString Name;
};

UCLASS()
class SCHIZOGAME_API ASimStreetBuilder : public AActor
{
	GENERATED_BODY()

public:
	ASimStreetBuilder();

	/**
	 * Reads Content/Sim/canon/places.csv and lays out the quarter: reads the
	 * street order straight from the CSV row order (the gate first, then the
	 * watch post, market, stalls, bakery, brewery, temple front, houses, well,
	 * shrine, granary — already an authored street walk). Returns the gate's
	 * world location (for the player start) or FVector::ZeroVector if the
	 * gate place is missing.
	 */
	FVector Build();

	/** Place id -> world location (the door-facing point outside the
	 * building, or the footprint centre for buildings with no door, e.g.
	 * the well/market square). Logged once per place during Build(). */
	static FVector GetPlaceLocation(FName PlaceId);

	/** How many buildings/door slots the last Build() produced (smoke evidence). */
	int32 NumBuildingsBuilt = 0;
	int32 NumDoorSlots = 0;

private:
	// --- kit meshes (instanced; "instanced static meshes are fine" per brief) ---
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_WallPlain;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_WallDoor;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_WallWindow;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_Corner;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_RoofSlab;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_RoofAccess;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_Pilaster;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_Stair;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_Lintel;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_CourtyardTile;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ISM_Awning;

	bool bKitLoaded = false;
	void LoadKitMeshes();

	static TArray<FSimPlaceRow> LoadPlaces();

	/** Builds a walled room (perimeter of wall/door/window pieces + a flat
	 * roof) footprint W x D grid cells, local min corner at Origin, door in
	 * the middle of DoorSide's run ("south"/"north"/"east"/"west" — "south"
	 * faces -Y, matching the street-side convention used below), one window
	 * opposite the door if bWindow. Returns the door's world location
	 * (outside the wall, facing away from the room) or Origin + footprint
	 * centre if there is no door.
	 * ponytail: roof is tiled uniformly with the parapet piece (no interior
	 * flat-only tile stitched in) — a visible lip runs across every roof
	 * tile, not just the outer edge (assemble_house.py's build_flat_roof
	 * does this properly, offline, for the review houses). Upgrade if the
	 * screenshot shows it reads badly at ground level.
	 */
	FVector BuildRoom(const FVector& Origin, int32 W, int32 D, const FString& DoorSide, bool bWindow,
		bool bRoofAccess);

	/** An open paved area (courtyard tiles only, no walls) — the market
	 * square, the well, the shrine niche. Returns the footprint centre. */
	FVector BuildPavedArea(const FVector& Origin, int32 W, int32 D);

	/** A single market stall: one courtyard tile + a reed awning over it,
	 * open on all sides (no door slot — a stall has no door). */
	FVector BuildStall(const FVector& Origin);

	/** Spawns an actor tagged SimDoorSlot at Location, facing OutwardYawDeg. */
	void SpawnDoorSlot(const FVector& Location, float OutwardYawDeg, FName PlaceId);

	static TMap<FName, FVector> PlaceLocations;
};
