// SimEnvironment.h — the City of the Moon around the Moon Gate Quarter, built
// by code in the D-023 style (low-poly, flat-shaded, vertex-coloured; the
// mood carried by light and fog — see SimDayNight).
//
// What stands here (world-bible §5 "City of the Moon": the richest city, on
// the sea trade routes, home of the temple of sun and moon and the Great
// Lighthouse; §2: the drought that is breaking the world):
//   - terrain: the walled city on flat packed earth; west of the Moon Gate the
//     irrigated barley fields and canals, withering in the drought; the river
//     to the south with its reed banks; desert and dunes beyond; the sea east.
//   - the city wall with towers, the monumental Moon Gate (the street's own
//     gate place stands in its passage), a sea gate and the harbour quay.
//   - the ziggurat of the temple of sun and moon in its walled precinct.
//   - the Great Lighthouse on its mole, fire and turning beam at night.
//   - the rest of the city as a dense field of flat-roofed mudbrick houses.
//   - date palms, reeds, barley, rocks, boats, far mountains to the north.
//   - fires, door lamps and lit windows that burn from dusk to dawn.
//
// Layout units are cm, +X east, +Y north; the Moon Gate is at the origin and
// the quarter's street runs east from it (SimStreetBuilder). Everything is
// deterministic: the same build every run.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimEnvironment.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
class USpotLightComponent;
class UMaterialInterface;
struct FSimMeshKit;

/** What the ground is, per terrain triangle (V-B3): picks M_Terrain's detail cell (T_Ground, same order)
 *  and rides in the terrain's vertex alpha as Kind / 15. Tommy's colours do not depend on it. */
enum class ESimGround : uint8
{
	Silt, Irrigated, Cracked, Salt, Sand, Gravel, ReedMud, Beach, Road, Street, Bed, Rock, Hills, Dune, Tell, Spare
};

/** A tell (V-B3): an old settlement mound on the landward horizon, flat-topped. cm. */
struct FSimTell
{
	FVector2D Center;
	float Radius;
	float Height;  // above Level
	float Level;   // the mean ground round its foot, its flat base
};

UCLASS()
class SCHIZOGAME_API ASimEnvironment : public AActor
{
	GENERATED_BODY()

public:
	ASimEnvironment();

	/** Spawns the environment and builds everything that does not depend on
	 *  the street. Call BEFORE the street builds (the terrain is its ground). */
	static ASimEnvironment* BuildWorld(UWorld* World);

	/** Door lamps along the street — call AFTER the street registered its door slots. */
	void BuildStreetDressing();

	/** Ground height of the terrain at (X, Y), cm. */
	static float TerrainHeight(float X, float Y);

	/** The lagoon and sea surface height, cm (the scatter floats its lilies on it). */
	static float WaterHeight();

	/** V-B5 T4: the irrigation canals' water, 30 cm lower each drought stage (4: 120 cm); the lagoon, the
	 *  river and the sea stay at WaterHeight(). Pure. */
	static float CanalWaterZ(int32 Drought);
	/** Is (X, Y) water of the irrigation canals (the field grid west of the gate), not the river or lagoon? */
	static bool IsCanalWater(float X, float Y);
	/** Lowers the canals' water to CanalWaterZ(Drought) (Tick, on a drought change; tests call it). */
	void SetDrought(int32 Drought);
	UProceduralMeshComponent* GetCanalWater() const { return CanalWater; }

	/** Tommy's terrain colour of a triangle centred at (X, Y, H) with normal Z Nz (tests pin it). */
	static FLinearColor SampleTerrainColor(float X, float Y, float H, float Nz, uint32 Seed);

	/** The ground kind of a terrain triangle centred at (X, Y, H), normal Z Nz: the same regions as
	 *  TerrainColor, finer where the colour has one tone for two grounds. */
	static ESimGround GroundKind(float X, float Y, float H, float Nz, uint32 Seed);

	/** The five tells (seeded; clear of the road, the river, the fields and the sea). */
	static TArray<FSimTell> GetTells();

	/** Where BuildCountryside planted its palms (empty until the environment is built). */
	static const TArray<FVector2D>& GetCountryPalms();

	/** 1 on the crest of a levee beside the river or a canal, 0 away from it. */
	static float LeveeAt(float X, float Y);

	/** The river's centreline Y at X, cm (it runs east to the sea, north of the walls). */
	static float RiverCentreY(float X);

	/** Reads the crescent's frame and monuments from the canon (Build does it; tests call it first). */
	static void LoadFrame();

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Terrain;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Water;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> CanalWater;
	int32 ShownDrought = 0;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Solid;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Foliage;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Far;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Fire;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> NightGlow;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Blocks;

	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> NightLights;
	TArray<float> NightLightBase;
	UPROPERTY() TObjectPtr<USpotLightComponent> Beacon;

	UPROPERTY() TObjectPtr<UMaterialInterface> FlatMat;
	/** V-B3: Tommy's look x the ground kind's detail (null on a fresh clone: the terrain keeps FlatMat). */
	UPROPERTY() TObjectPtr<UMaterialInterface> TerrainMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> WindMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> WaterMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> GlowMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> WindowGlowMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> MoonMat;

	bool bNightShown = true;
	float Clock = 0.f;

	void LoadMaterials();
	void Build();
	void BuildTerrain();
	void BuildWater();
	void BuildWalls(FSimMeshKit& K, FSimMeshKit& Glow);
	void BuildZiggurat(FSimMeshKit& K, FSimMeshKit& Glow);
	void BuildLighthouse(FSimMeshKit& K, FSimMeshKit& Glow);
	void BuildCity(FSimMeshKit& Plants, FSimMeshKit& Windows);
	void BuildCountryside(FSimMeshKit& Solid, FSimMeshKit& Plants);
	void BuildFar(FSimMeshKit& K);
	void BuildBoats(FSimMeshKit& K, FSimMeshKit& Sails);

	/** A brazier / torch: flame into Glow, and a night light (Radius cm, cd). */
	void AddFire(FSimMeshKit& Glow, const FVector& At, float Size, float Candela, float Radius, bool bShadows);
	UPointLightComponent* AddNightLight(const FVector& At, float Candela, float Radius, const FLinearColor& Color, bool bShadows);
	/** One engine-cube block instance: bottom centre, full size, yaw, sRGB colour. */
	void AddBlock(const FVector& BottomCenter, const FVector& Size, float YawDeg, const FLinearColor& Color);
};
