// SimScatter.h — Stage V batch 2: the city's plants, stones and clutter (tools/art/scatter.py's
// Content/Sim/scatter.csv), one HISM per (mesh, seasons) pair, standing on the terrain (lilies on the
// water). The kernel's drought browns the leaves through MPC_World.Wither (M_Scatter lerps a leaf's
// vertex colour to straw by it), and a species out of season is hidden (spring flowers, lilies).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimScatter.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialParameterCollection;

struct FSimScatterResult
{
	int32 NumInstances = 0;
	int32 NumMeshes = 0;         // meshes that stand (imported and in scatter.csv)
	int32 NumMissingMeshes = 0;  // meshes scatter.csv names that are not imported (their rows skipped)
	int32 NumCountryside = 0;    // V-B3: instances on the land round the crescent (not in NumInstances)
	TMap<FString, int32> CountrysideByHabitat;  // levee, field, bank, desert, tell_top
};

UCLASS()
class SCHIZOGAME_API ASimScatter : public AActor
{
	GENERATED_BODY()

public:
	ASimScatter();

	/** Spawns the scatter and stands every row of scatter.csv whose mesh is imported. Call after the
	 *  environment (its terrain is the ground). A second call returns the standing scatter's result. */
	static FSimScatterResult BuildScatter(UWorld* World);

	/** Wither = clamp(DroughtStage / 4); the HISMs whose seasons do not include Season are hidden. */
	void ApplyWorldState(int32 DroughtStage, const FString& Season);

	/** The Wither last applied (what MPC_World holds). */
	float GetWither() const { return Wither; }

	/** Test switch (like ASimStreetBuilder::bUseBuildingMeshes): false stands nothing, as on a fresh clone. */
	static bool bUseScatterMeshes;

	static FString ScatterCsvPath();
	static bool MeshExists(const FString& MeshId);

	virtual void Tick(float DeltaSeconds) override;

private:
	FSimScatterResult Build();

	/** V-B3: the countryside from the terrain's ground kinds (fields, levees, banks, desert, tells), seeded by
	 *  SimHash01 on a jittered 4 m grid, clear of the road west and Tommy's palms. */
	void ScatterCountryside(FSimScatterResult& R);

	/** The HISM for a mesh and season set (city or countryside), made on first use; INDEX_NONE if not imported. */
	int32 HismFor(const FString& Mesh, const FString& SeasonsCsv, bool bCountry);

	TMap<FString, int32> HismByKey;
	TMap<FString, FString> MeshGroupOf;
	TSet<FString> MissingMeshes, StandingMeshes;

	UPROPERTY() TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Hisms;
	/** Each HISM's seasons (parallel to Hisms): empty = all year. */
	TArray<TArray<FString>> HismSeasons;
	UPROPERTY() TObjectPtr<UMaterialParameterCollection> WorldParams;

	FSimScatterResult LastResult;
	float Wither = 0.f;
	int64 LastDay = -1;
	int32 LastDrought = -1;
	FString LastSeason;
};
