// SimPropsBuilder.h — the dressing pass: after the street stands (W6-B), the
// quarter's places get their props (tools/art/props_gen.py meshes, imported
// to /Game/Art/Props by tools/art/ue_import_props.py), the sky stops being a
// black void (a 120 m unlit gradient dome — deliberately NOT a SkyLight: a
// captured-scene SkyLight's CubemapCapture wedges this GPU, see
// SimStreetBuilder/SimDayNight and DECISIONS.md), and one conservative
// unbound PostProcessVolume warms the image (D-023: the look is carried by
// light, fog, sky and grading, not asset fidelity).
//
// Placement is deterministic from the street's registries (same places.csv
// -> same props): doorable places through ASimStreetBuilder::GetAllDoorSlots
// (kind + the door's outward axis), stalls/well by place id, per-place
// variation hashed with FCrc::StrCrc32 (the street's own house-variant
// method — GetTypeHash(FName) is not stable across sessions). Meshes load
// by path and MISSING ones are skipped silently (the props import is a
// separate optional step; the street must not depend on it).
//
// A tickable world subsystem like SimVerbSpawner: self-registering, no
// wiring into SimGameMode (not this wave's file to edit). It waits for the
// street's place registry (filled during the game mode's StartPlay) and
// gives up on props after 5 s of no street — the sky and the post polish
// still dress the empty world.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimPropsBuilder.generated.h"

class UStaticMeshComponent;

/** The sky dome: a 120 m inverted sphere wearing the unlit gradient material
 * /Game/Art/Sky/M_SkyDome (authored headless by tools/art/ue_import_props).
 * No collision, casts no shadows, never captures anything. SetupDome is
 * called by USimPropsBuilder right after spawn. */
UCLASS()
class SCHIZOGAME_API ASimSkyDome : public AActor
{
	GENERATED_BODY()

public:
	ASimSkyDome();

	/** Loads the dome mesh (the kit's SM_SkyDome, or the engine sphere
	 * scaled to match) and the gradient material; RadiusCm feeds the
	 * material's DomeRadius parameter (the gradient's height factor). */
	void SetupDome(float RadiusCm);

private:
	UPROPERTY(VisibleAnywhere, Category = "Sky")
	TObjectPtr<UStaticMeshComponent> DomeMesh;
};

/** The dressing pass (see file header). Spawns, once per world after the
 * street exists: street props, the sky dome, the post-process volume. */
UCLASS()
class SCHIZOGAME_API USimPropsBuilder : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** The dome at the middle of the street's built places (falls back to
	 * the world origin when no street stood). */
	void SpawnSkyDome();

	/** One unbound weight-1 post volume: warm white balance, a slight
	 * vignette, mild filmic saturation/contrast (values in the ledger). */
	void SpawnPostPolish();

	/** Props from the place/door registries (see file header). Returns the
	 * number of props spawned. */
	int32 PlaceStreetProps();

	bool bDressed = false;
	/** How long the street's registry has been empty after begin play (it
	 * fills during the game mode's StartPlay; 5 s means no street is
	 * coming — dress the shell only). */
	float WaitedForStreetSeconds = 0.f;
};
