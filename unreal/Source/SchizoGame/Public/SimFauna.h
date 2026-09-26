// SimFauna.h — Stage V batch 4: the animals of the city and its land (Content/Sim/canon/fauna.csv).
// Herds follow the kernel's head count (one visible animal per ten head), work beasts and city animals
// stand at their places' typologies and along the streets, the night's jackals come out after dark near
// the tombs and the camps. Each animal: a skeletal mesh on a tiny state machine (idle / graze / walk /
// flee / sleep) round its home, on the terrain height, no navmesh; it flees the player and sleeps
// outside its hours. Models: /Game/Art/Fauna (tools/art/ue_import_fauna.py).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimFauna.generated.h"

class UAnimationAsset;
class UInstancedStaticMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;

enum class ESimAnimalState : uint8 { Idle, Graze, Walk, Flee, Sleep };

/** A species row of fauna.csv as the game reads it. */
struct FSimFaunaSpecies
{
	FName Id;
	FString Group, Model;
	TArray<FString> Habitats;
	int32 HourStart = 0, HourEnd = 24, GroupMin = 1, GroupMax = 1, PerPlace = 0;
	float HerdShare = 0.f;
	bool IsActive(float Hour) const;
	bool IsNocturnal() const { return HourStart > HourEnd; }
};

/** One animal (the component lives in ASimFauna::Meshes at the same index). */
struct FSimAnimal
{
	int32 Species = INDEX_NONE;
	FVector2D Home = FVector2D::ZeroVector;
	float Radius = 300.f;
	FVector2D Pos = FVector2D::ZeroVector, Target = FVector2D::ZeroVector;
	float Heading = 0.f;
	ESimAnimalState State = ESimAnimalState::Idle;
	float StateTime = 0.f;
	bool bVisible = true;
};

/** A flock of birds (or a school of carp): classic boids round an anchor, one instanced mesh (Task 4). */
struct FSimFlock
{
	int32 Species = INDEX_NONE;
	FVector Anchor = FVector::ZeroVector;  // z: the ground or water the flock stands on
	float Radius = 1000.f;
	float MinZ = 0.f, MaxZ = 0.f;          // the flying band above Anchor.Z (0, 0: it stands)
	float FlapHz = 3.f;
	bool bGrounded = false;                // stands/floats until scattered (waders, fowl, crows, sparrows)
	bool bSoar = false;                    // wide circles (vultures)
	float LiftTimer = 0.f;                 // > 0: scattered, flying; lands when it runs out
	TArray<FVector> Pos, Vel;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FSimAnimalCue, FName /*Species*/, FName /*Cue: flee, bark*/);

UCLASS()
class SCHIZOGAME_API ASimFauna : public AActor
{
	GENERATED_BODY()

public:
	ASimFauna();

	/** Spawns the fauna (after the street: animals stand at its door slots) and populates today. */
	static ASimFauna* BuildFauna(UWorld* World);

	/** (Re)spawns every animal for a sim day and a kernel herd head count. Deterministic in both. */
	void Populate(int32 SimDay, int32 HerdHead);

	/** Advances every animal and flock by Dt seconds (every frame; tests call it directly). */
	void Step(float Dt);

	/** Tests: fix the hour and the player's position (negative hour / unset: read the world). */
	float HourOverride = -1.f;
	TOptional<FVector2D> PlayerOverride;

	/** Test switch (like ASimScatter::bUseScatterMeshes): false spawns nothing, as on a fresh clone. */
	static bool bUseFaunaMeshes;
	/** Tests: model ids to treat as not imported (the species is skipped, the rest spawn). */
	static TSet<FString> ForceMissingModels;

	const TArray<FSimAnimal>& GetAnimals() const { return Animals; }
	const TArray<FSimFlock>& GetFlocks() const { return Flocks; }
	const TArray<FSimFaunaSpecies>& GetSpecies() const { return Species; }
	int32 CountOf(FName SpeciesId, bool bVisibleOnly = false) const;

	/** Batch 5's sounds: a dog barks, a flock lifts, an animal flees. */
	FSimAnimalCue OnAnimalCue;

	virtual void Tick(float DeltaSeconds) override;

private:
	void LoadSpecies();
	TArray<TPair<FVector2D, float>> HomesFor(const FSimFaunaSpecies& S, int32 SpeciesIndex, int32 SimDay) const;
	void Spawn(int32 SpeciesIndex, FVector2D Home, float Radius, uint32 Seed);
	void Play(int32 Index, ESimAnimalState State);
	void AddFlocks(int32 SpeciesIndex, int32 SimDay);
	void StepFlocks(float Dt, float Hour, FVector2D Player);
	float CurrentHour() const;
	FVector2D PlayerPos() const;
	bool Blocked(FVector2D P) const;

	TArray<FSimFaunaSpecies> Species;
	TArray<FSimAnimal> Animals;
	UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> Meshes;
	/** Per species: its mesh and its role clips (Idle, Walk, Run, Eat, Sleep); null mesh = not imported. */
	UPROPERTY() TArray<TObjectPtr<USkeletalMesh>> SpeciesMesh;
	UPROPERTY() TArray<TObjectPtr<UAnimationAsset>> SpeciesClips;  // 5 per species
	TArray<int32> CurrentClip;
	TArray<FSimFlock> Flocks;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> FlockMeshes;
	int64 LastDay = -1;
	int32 LastHerd = -1;
	uint32 StepCount = 0;
	float PollTimer = 0.f;
};
