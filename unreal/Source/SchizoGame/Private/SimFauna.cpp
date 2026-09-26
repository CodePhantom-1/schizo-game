// SimFauna.cpp — Stage V batch 4 Task 3: herds, work beasts, city animals and the night's predators.
#include "SimFauna.h"

#include "SimCityData.h"
#include "SimEnvironment.h"
#include "SimMeshKit.h"
#include "SimStreetBuilder.h"
#include "SimWorldSubsystem.h"
#include "sim/CApiWild.h"

#include "Animation/AnimationAsset.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimFauna, Log, All);

namespace
{
	const TCHAR* FaunaDir = TEXT("/Game/Art/Fauna");
	const TCHAR* ClipRoles[] = { TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("Eat"), TEXT("Sleep") };
	constexpr int32 NumClips = 5;
	constexpr float FleeRadiusCm = 600.f;
	constexpr float CullCm = 12000.f;  // 120 m
	constexpr int32 LiveCap = 150;
	constexpr float WalkCmS = 120.f, RunCmS = 380.f;
	constexpr float MeshYawOffset = -90.f;  // the imported animals face their local +Y (seen in the lineup shot)
	constexpr int32 HerdScale = 10;  // one visible animal per ten head of the kernel's herd (INVENTED)

	/** Groups of a species per habitat that has no place typology (INVENTED; ledger). */
	int32 HabitatGroups(const FString& H)
	{
		if (H == TEXT("street_edge")) { return 6; }
		if (H == TEXT("yard")) { return 4; }
		if (H == TEXT("wall_foot") || H == TEXT("tombs") || H == TEXT("camp")) { return 3; }
		return 0;  // shore, water, precinct, roof, open: the birds' (Task 4)
	}

	/** Open ground an animal stands inside (a pen, a pasture, a yard); every other place: outside its door. */
	bool IsOpenGround(const FString& Typology)
	{
		return Typology == TEXT("cattle_pen") || Typology == TEXT("pasture") || Typology == TEXT("caravan_yard");
	}

	bool Skeletal(const FSimFaunaSpecies& S)
	{
		return !S.Model.IsEmpty() && (S.Group == TEXT("herd") || S.Group == TEXT("work") || S.Group == TEXT("city") || S.Group == TEXT("wild_night"));
	}

	int32 ClipFor(ESimAnimalState State)
	{
		switch (State)
		{
		case ESimAnimalState::Walk: return 1;
		case ESimAnimalState::Flee: return 2;
		case ESimAnimalState::Graze: return 3;
		case ESimAnimalState::Sleep: return 4;
		default: return 0;
		}
	}

	FVector2D Along(float YawDeg) { const float A = FMath::DegreesToRadians(YawDeg); return FVector2D(FMath::Cos(A), FMath::Sin(A)); }
}

bool ASimFauna::bUseFaunaMeshes = true;
TSet<FString> ASimFauna::ForceMissingModels;

bool FSimFaunaSpecies::IsActive(float Hour) const
{
	if (HourStart == HourEnd)
	{
		return true;
	}
	return HourStart < HourEnd ? (Hour >= HourStart && Hour < HourEnd) : (Hour >= HourStart || Hour < HourEnd);
}

ASimFauna::ASimFauna()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;  // every frame: walking and flying must be smooth
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ASimFauna* ASimFauna::BuildFauna(UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<ASimFauna> It(World); It; ++It)
	{
		return *It;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASimFauna* F = World->SpawnActor<ASimFauna>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (F != nullptr)
	{
		const int64 Day = USimWorldSubsystem::GetSimDayFor(World);
		const SimWorld* H = USimWorldSubsystem::GetSimHandleFor(World);
		F->Populate(Day < 0 ? 1 : static_cast<int32>(Day), H != nullptr ? sim_world_herd_head(H) : 0);
	}
	return F;
}

void ASimFauna::LoadSpecies()
{
	Species.Reset();
	SpeciesMesh.Reset();
	SpeciesClips.Reset();
	TArray<FString> Lines;
	FFileHelper::LoadFileToStringArray(Lines, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Sim/canon/fauna.csv")));
	if (Lines.Num() < 2)
	{
		UE_LOG(LogSimFauna, Warning, TEXT("fauna: Content/Sim/canon/fauna.csv missing — run tools/stage_canon_for_ue.py."));
		return;
	}
	const TArray<FString> Head = SimCityData::SplitCsv(Lines[0]);
	auto Col = [&Head](const TCHAR* N) { return Head.IndexOfByKey(FString(N)); };
	const int32 CId = Col(TEXT("id")), CGroup = Col(TEXT("group")), CHab = Col(TEXT("habitats")), CHours = Col(TEXT("hours")),
		CMin = Col(TEXT("group_min")), CMax = Col(TEXT("group_max")), CPer = Col(TEXT("per_place")), CShare = Col(TEXT("herd_share")),
		CModel = Col(TEXT("model"));
	int32 Skel = 0, Missing = 0;
	TArray<FString> MissingNames;
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		const TArray<FString> F = SimCityData::SplitCsv(Lines[i]);
		if (F.Num() < Head.Num())
		{
			continue;
		}
		FSimFaunaSpecies& S = Species.AddDefaulted_GetRef();
		S.Id = FName(*F[CId]);
		S.Group = F[CGroup];
		S.Model = F[CModel];
		F[CHab].ParseIntoArray(S.Habitats, TEXT(";"));
		FString A, B;
		if (F[CHours].Split(TEXT("-"), &A, &B))
		{
			S.HourStart = FCString::Atoi(*A);
			S.HourEnd = FCString::Atoi(*B);
		}
		S.GroupMin = FCString::Atoi(*F[CMin]);
		S.GroupMax = FCString::Atoi(*F[CMax]);
		S.PerPlace = FCString::Atoi(*F[CPer]);
		S.HerdShare = FCString::Atof(*F[CShare]);

		USkeletalMesh* Mesh = nullptr;
		TArray<UAnimationAsset*> Clips;
		Clips.Init(nullptr, NumClips);
		if (Skeletal(S))
		{
			++Skel;
			const FString MeshPkg = FString::Printf(TEXT("%s/SK_%s"), FaunaDir, *S.Model);
			if (bUseFaunaMeshes && !ForceMissingModels.Contains(S.Model) && FPackageName::DoesPackageExist(MeshPkg))
			{
				Mesh = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s.SK_%s"), *MeshPkg, *S.Model));
				for (int32 c = 0; c < NumClips; ++c)
				{
					const FString Pkg = FString::Printf(TEXT("%s/%s/Anims/%s"), FaunaDir, *S.Model, ClipRoles[c]);
					if (FPackageName::DoesPackageExist(Pkg))
					{
						Clips[c] = LoadObject<UAnimationAsset>(nullptr, *FString::Printf(TEXT("%s.%s"), *Pkg, ClipRoles[c]));
					}
				}
			}
			if (Mesh == nullptr)
			{
				++Missing;
				MissingNames.Add(S.Model);
			}
		}
		SpeciesMesh.Add(Mesh);
		for (UAnimationAsset* C : Clips)
		{
			SpeciesClips.Add(C);
		}
	}
	if (Missing == Skel && Skel > 0)
	{
		// A fresh clone before ue_import_fauna.py: one line, no per-animal warnings (Review Focus 4).
		UE_LOG(LogSimFauna, Log, TEXT("fauna: no animals imported — run tools/art/ue_import_fauna.py."));
	}
	else
	{
		for (const FString& M : MissingNames)  // one warning per missing model; the others still spawn (Review Focus 5)
		{
			UE_LOG(LogSimFauna, Warning, TEXT("fauna: model %s not imported — its species is skipped."), *M);
		}
	}
}

TArray<TPair<FVector2D, float>> ASimFauna::HomesFor(const FSimFaunaSpecies& S, int32 SpeciesIndex, int32 SimDay) const
{
	TArray<TPair<FVector2D, float>> Out;
	TArray<FSimDoorSlotInfo> Slots;
	ASimStreetBuilder::GetAllDoorSlots(Slots);
	Slots.Sort([](const FSimDoorSlotInfo& A, const FSimDoorSlotInfo& B) { return A.PlaceId.LexicalLess(B.PlaceId); });
	const FSimCityFrame& Fr = SimCityData::Frame();
	for (int32 h = 0; h < S.Habitats.Num(); ++h)
	{
		const FString& H = S.Habitats[h];
		if (H.StartsWith(TEXT("@")))
		{
			const FString Typ = H.Mid(1);
			for (const FSimCityPlace& P : SimCityData::Places())
			{
				if (P.Typology != Typ)
				{
					continue;
				}
				if (IsOpenGround(Typ))
				{
					Out.Add({ P.Center, FMath::Clamp(0.4f * FMath::Min(P.Size.X, P.Size.Y), 300.f, 2500.f) });
					continue;
				}
				const FSimDoorSlotInfo* Slot = Slots.FindByPredicate([&P](const FSimDoorSlotInfo& D) { return D.PlaceId == P.Id; });
				const FVector2D Door = Slot != nullptr ? FVector2D(Slot->Location) + Along(Slot->OutwardYawDeg) * 250.f : P.Center;
				for (int32 g = 0; g < FMath::Max(1, S.PerPlace); ++g)
				{
					Out.Add({ Door, 350.f });
				}
			}
			continue;
		}
		const int32 Groups = HabitatGroups(H) * FMath::Max(1, S.PerPlace);
		for (int32 g = 0; g < Groups; ++g)
		{
			const float R0 = SimHash01(SpeciesIndex * 131 + h, g, 601 + SimDay % 7);
			const float R1 = SimHash01(SpeciesIndex * 131 + h, g, 602 + SimDay % 7);
			if ((H == TEXT("street_edge") || H == TEXT("yard")) && Slots.Num() > 0)
			{
				const FSimDoorSlotInfo& D = Slots[FMath::Min(Slots.Num() - 1, FMath::FloorToInt(R0 * Slots.Num()))];
				const FSimCityPlace* P = SimCityData::Find(D.PlaceId);
				const FVector2D Out1 = Along(D.OutwardYawDeg);
				if (H == TEXT("street_edge"))
				{
					Out.Add({ FVector2D(D.Location) + Out1 * 300.f + Along(D.OutwardYawDeg + 90.f) * (R1 - 0.5f) * 600.f, 450.f });
				}
				else if (P != nullptr)
				{
					Out.Add({ P->Center - Out1 * (0.5f * P->Size.Y + 180.f), 220.f });
				}
			}
			else if (H == TEXT("wall_foot"))
			{
				const float A = FMath::Lerp(Fr.HornEastDeg, Fr.HornWestDeg, R0);
				Out.Add({ Fr.Center + Along(A) * (Fr.WallR - 350.f), 350.f });
			}
			else if (H == TEXT("tombs") || H == TEXT("camp"))
			{
				TArray<const FSimCityPlace*> Near;
				for (const FSimCityPlace& P : SimCityData::Places())
				{
					if (H == TEXT("tombs") ? P.Quarter == TEXT("garden_of_tombs")
						: (P.Typology == TEXT("migrant_tent") || P.Typology == TEXT("warchief_tent")))
					{
						Near.Add(&P);
					}
				}
				if (Near.Num() > 0)
				{
					const FSimCityPlace* P = Near[FMath::Min(Near.Num() - 1, FMath::FloorToInt(R0 * Near.Num()))];
					Out.Add({ P->Center + Along(360.f * R1) * (0.5f * FMath::Max(P->Size.X, P->Size.Y) + 800.f), 900.f });
				}
			}
		}
	}
	return Out;
}

bool ASimFauna::Blocked(FVector2D P) const
{
	const FSimCityFrame& Fr = SimCityData::Frame();
	return FVector2D::Distance(P, Fr.Center) < Fr.LagoonR - 100.f;  // never into the lagoon (Review Focus 2)
}

void ASimFauna::Spawn(int32 SpeciesIndex, FVector2D Home, float Radius, uint32 Seed)
{
	if (Blocked(Home))
	{
		return;
	}
	FSimAnimal& A = Animals.AddDefaulted_GetRef();
	A.Species = SpeciesIndex;
	A.Home = Home;
	A.Radius = Radius;
	const float Ang = 360.f * SimHash01(Seed, 11), D = Radius * 0.8f * FMath::Sqrt(SimHash01(Seed, 12));
	A.Pos = Home + Along(Ang) * D;
	if (Blocked(A.Pos))
	{
		A.Pos = Home;
	}
	A.Target = A.Pos;
	A.Heading = 360.f * SimHash01(Seed, 13);
	A.StateTime = 1.f + 4.f * SimHash01(Seed, 14);
	USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(this,
		FName(*FString::Printf(TEXT("Animal_%s_%d"), *Species[SpeciesIndex].Id.ToString(), Animals.Num() - 1)));
	C->SetupAttachment(RootComponent);
	C->SetSkeletalMesh(SpeciesMesh[SpeciesIndex]);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	C->bEnableUpdateRateOptimizations = true;
	C->RegisterComponent();
	AddInstanceComponent(C);
	Meshes.Add(C);
	CurrentClip.Add(INDEX_NONE);
}

void ASimFauna::Populate(int32 SimDay, int32 HerdHead)
{
	for (USkeletalMeshComponent* C : Meshes)
	{
		if (C != nullptr)
		{
			C->DestroyComponent();
		}
	}
	Meshes.Reset();
	Animals.Reset();
	CurrentClip.Reset();
	for (UInstancedStaticMeshComponent* C : FlockMeshes)
	{
		if (C != nullptr)
		{
			C->DestroyComponent();
		}
	}
	FlockMeshes.Reset();
	Flocks.Reset();
	LoadSpecies();
	LastDay = SimDay;
	LastHerd = HerdHead;
	StepCount = 0;  // every random draw hashes the step: the same day plays out the same
	for (int32 s = 0; s < Species.Num(); ++s)
	{
		const FSimFaunaSpecies& S = Species[s];
		if (!Skeletal(S))
		{
			if (!S.Model.IsEmpty())
			{
				AddFlocks(s, SimDay);  // the birds and the carp (Task 4)
			}
			continue;
		}
		if (SpeciesMesh[s] == nullptr)
		{
			continue;
		}
		const TArray<TPair<FVector2D, float>> Homes = HomesFor(S, s, SimDay);
		if (Homes.Num() == 0)
		{
			continue;
		}
		if (S.Group == TEXT("herd"))
		{
			// The kernel's herd, at one visible animal per ten head, spread over the pens and pastures (Review Focus 1).
			const int32 N = FMath::RoundToInt(HerdHead * S.HerdShare / HerdScale);
			for (int32 i = 0; i < N; ++i)
			{
				const TPair<FVector2D, float>& H = Homes[i % Homes.Num()];
				Spawn(s, H.Key, H.Value, HashCombine(GetTypeHash(S.Id), static_cast<uint32>(SimDay * 1000 + i)));
			}
			continue;
		}
		for (int32 g = 0; g < Homes.Num(); ++g)
		{
			const uint32 Seed = HashCombine(GetTypeHash(S.Id), static_cast<uint32>(SimDay * 97 + g));
			const int32 Size = S.GroupMin + FMath::FloorToInt(SimHash01(Seed, 5) * (S.GroupMax - S.GroupMin + 1));
			for (int32 i = 0; i < FMath::Max(1, Size); ++i)
			{
				Spawn(s, Homes[g].Key, Homes[g].Value, HashCombine(Seed, static_cast<uint32>(i)));
			}
		}
	}
	// -SimFaunaLineup (review shots, the "lineup" view): one of each species stands in a row in the street,
	// side-on to the camera, and stays there.
	if (FParse::Param(FCommandLine::Get(), TEXT("SimFaunaLineup")))
	{
		// Across the lineup camera's view (from (12900, 10300) toward (14600, 10900)), 8 m out, side-on.
		const FVector2D Eye(12900.f, 10300.f), Dir = FVector2D(1700.f, 600.f).GetSafeNormal(), Row(-Dir.Y, Dir.X);
		const FVector2D Start = Eye + Dir * 1000.f - Row * 200.f;
		int32 Slot = 0;
		for (int32 s = 0; s < Species.Num(); ++s)
		{
			FSimAnimal* A = Animals.FindByPredicate([s](const FSimAnimal& X) { return X.Species == s; });
			if (A != nullptr)
			{
				A->Home = A->Pos = A->Target = Start + Row * (150.f * Slot++);
				A->Radius = 1.f;
				A->Heading = FMath::RadiansToDegrees(FMath::Atan2(Row.Y, Row.X));
				A->State = ESimAnimalState::Idle;
				A->StateTime = 1e6f;
			}
		}
	}
	Step(0.f);
	int32 Birds = 0;
	for (const FSimFlock& F : Flocks)
	{
		Birds += F.Pos.Num();
	}
	UE_LOG(LogSimFauna, Log, TEXT("fauna: day %d, herd %d head: %d animals, %d birds and fish in %d flocks."),
		SimDay, HerdHead, Animals.Num(), Birds, Flocks.Num());
}

float ASimFauna::CurrentHour() const
{
	if (HourOverride >= 0.f)
	{
		return HourOverride;
	}
	const float H = USimWorldSubsystem::GetSimHourFor(GetWorld());
	return H < 0.f ? 12.f : H;
}

FVector2D ASimFauna::PlayerPos() const
{
	if (PlayerOverride.IsSet())
	{
		return PlayerOverride.GetValue();
	}
	if (UWorld* W = GetWorld())
	{
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			if (APawn* P = PC->GetPawn())
			{
				return FVector2D(P->GetActorLocation());
			}
		}
	}
	return FVector2D(1e9f, 1e9f);
}

void ASimFauna::Play(int32 Index, ESimAnimalState State)
{
	const int32 Clip = ClipFor(State);
	if (CurrentClip[Index] == Clip)
	{
		return;
	}
	UAnimationAsset* Anim = SpeciesClips[Animals[Index].Species * NumClips + Clip];
	if (Anim == nullptr)
	{
		Anim = SpeciesClips[Animals[Index].Species * NumClips];  // Idle
	}
	if (Anim != nullptr && Meshes[Index] != nullptr)
	{
		Meshes[Index]->PlayAnimation(Anim, true);
		CurrentClip[Index] = Clip;
	}
}

void ASimFauna::Step(float Dt)
{
	++StepCount;
	const float Hour = CurrentHour();
	const FVector2D Player = PlayerPos();
	// The 150 nearest animals within 120 m of the camera are drawn and animated; the rest wait, hidden.
	FVector2D View = Player;
	if (!PlayerOverride.IsSet() && GetWorld() != nullptr)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager != nullptr)
			{
				View = FVector2D(PC->PlayerCameraManager->GetCameraLocation());
			}
		}
	}
	TArray<TPair<float, int32>> ByDist;
	for (int32 i = 0; i < Animals.Num(); ++i)
	{
		ByDist.Add({ FVector2D::DistSquared(Animals[i].Pos, View), i });
	}
	ByDist.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	const bool bNoPlayer = View.X > 1e8f;  // no view (a test world): nothing to cull against
	TBitArray<> Near(bNoPlayer, Animals.Num());
	for (int32 k = 0; !bNoPlayer && k < ByDist.Num() && k < LiveCap; ++k)
	{
		Near[ByDist[k].Value] = ByDist[k].Key < CullCm * CullCm;
	}

	for (int32 i = 0; i < Animals.Num(); ++i)
	{
		FSimAnimal& A = Animals[i];
		const FSimFaunaSpecies& S = Species[A.Species];
		const bool bActive = S.IsActive(Hour);
		// The night's predators exist only in their hours; everything else sleeps at home outside its own.
		A.bVisible = Near[i] && (bActive || !S.IsNocturnal());
		if (Meshes[i] != nullptr)
		{
			Meshes[i]->SetVisibility(A.bVisible);
			Meshes[i]->bPauseAnims = !A.bVisible;
		}
		if (!bActive)
		{
			A.State = ESimAnimalState::Sleep;
			A.Pos = A.Home + (A.Pos - A.Home).GetClampedToMaxSize(A.Radius * 0.5f);  // settled in the pen
		}
		else
		{
			const float ToPlayer = FVector2D::Distance(A.Pos, Player);
			if (ToPlayer < FleeRadiusCm && A.State != ESimAnimalState::Flee)
			{
				A.State = ESimAnimalState::Flee;
				A.StateTime = 2.f;
				const FVector2D Away = (A.Pos - Player).GetSafeNormal();
				A.Target = A.Home + Away * A.Radius;
				OnAnimalCue.Broadcast(S.Id, S.Id.ToString().Contains(TEXT("dog")) || S.Id == TEXT("saluki") ? FName(TEXT("bark")) : FName(TEXT("flee")));
			}
			A.StateTime -= Dt;
			if (A.State == ESimAnimalState::Sleep || A.StateTime <= 0.f)
			{
				const float R = SimHash01(i, StepCount, 21);
				if (R < 0.35f)
				{
					A.State = ESimAnimalState::Idle;
					A.StateTime = 2.f + 4.f * SimHash01(i, StepCount, 22);
				}
				else if (R < 0.65f)
				{
					A.State = ESimAnimalState::Graze;
					A.StateTime = 4.f + 6.f * SimHash01(i, StepCount, 23);
				}
				else
				{
					A.State = ESimAnimalState::Walk;
					A.StateTime = 8.f;
					A.Target = A.Home + Along(360.f * SimHash01(i, StepCount, 24)) * A.Radius * FMath::Sqrt(SimHash01(i, StepCount, 25));
				}
			}
			if (A.State == ESimAnimalState::Walk || A.State == ESimAnimalState::Flee)
			{
				const FVector2D To = A.Target - A.Pos;
				const float Speed = A.State == ESimAnimalState::Flee ? RunCmS : WalkCmS;
				if (To.Size() < 30.f)
				{
					A.State = ESimAnimalState::Idle;
					A.StateTime = 2.f;
				}
				else
				{
					const FVector2D Next = A.Pos + To.GetSafeNormal() * FMath::Min(To.Size(), Speed * Dt);
					A.Heading = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
					if (!Blocked(Next))
					{
						A.Pos = Next;
					}
				}
			}
			A.Pos = A.Home + (A.Pos - A.Home).GetClampedToMaxSize(A.Radius);  // never leaves its home ground
		}
		if (A.bVisible && Meshes[i] != nullptr)
		{
			Meshes[i]->SetWorldLocationAndRotation(FVector(A.Pos, ASimEnvironment::TerrainHeight(A.Pos.X, A.Pos.Y)),
				FRotator(0.f, A.Heading + MeshYawOffset, 0.f));
			Play(i, A.State);
		}
	}
	StepFlocks(Dt, Hour, Player);
}

int32 ASimFauna::CountOf(FName SpeciesId, bool bVisibleOnly) const
{
	int32 N = 0;
	for (const FSimAnimal& A : Animals)
	{
		N += Species[A.Species].Id == SpeciesId && (!bVisibleOnly || A.bVisible);
	}
	return N;
}

void ASimFauna::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PollTimer += DeltaSeconds;
	if (PollTimer >= 1.f)
	{
		// A new day (or a raid on the herd) re-populates: the pasture never shows the old count (Review Focus 1).
		PollTimer = 0.f;
		UWorld* World = GetWorld();
		const int64 Day = USimWorldSubsystem::GetSimDayFor(World);
		const SimWorld* H = USimWorldSubsystem::GetSimHandleFor(World);
		const int32 Herd = H != nullptr ? sim_world_herd_head(H) : 0;
		if (Day >= 0 && (Day != LastDay || Herd != LastHerd))
		{
			Populate(static_cast<int32>(Day), Herd);
		}
	}
	Step(DeltaSeconds);
}

// --- Task 4: the birds (and the carp) -------------------------------------------------------------------

namespace
{
	struct FFlockKind
	{
		float Radius, MinZ, MaxZ, FlapHz;
		bool bGrounded, bSoar;
	};

	/** How each bird species keeps its flock (INVENTED; ledger). */
	FFlockKind FlockKindOf(const FName Id)
	{
		const FString S = Id.ToString();
		if (S == TEXT("dove")) { return { 2500.f, 300.f, 1500.f, 4.f, false, false }; }
		if (S == TEXT("vulture")) { return { 4000.f, 3000.f, 6000.f, 0.4f, false, true }; }
		if (S == TEXT("heron")) { return { 600.f, 0.f, 0.f, 2.f, true, false }; }
		if (S == TEXT("flamingo")) { return { 900.f, 0.f, 0.f, 2.5f, true, false }; }
		if (S == TEXT("goose") || S == TEXT("duck")) { return { 700.f, 0.f, 0.f, 3.f, true, false }; }
		if (S == TEXT("carp")) { return { 600.f, 0.f, 0.f, 0.f, true, false }; }
		if (S == TEXT("sparrow")) { return { 500.f, 0.f, 0.f, 8.f, true, false }; }
		return { 600.f, 0.f, 0.f, 5.f, true, false };  // crow
	}

	int32 FlockGroups(const FString& Habitat)
	{
		if (Habitat == TEXT("roof") || Habitat == TEXT("street_edge")) { return 3; }
		if (Habitat == TEXT("precinct") || Habitat == TEXT("water") || Habitat == TEXT("shore") || Habitat == TEXT("open")) { return 2; }
		if (Habitat == TEXT("tombs")) { return 1; }
		return 0;
	}

	constexpr float LiftCm = 400.f;      // the player this close to a standing bird: the flock lifts
	constexpr float ResettleS = 20.f;    // ...and comes down this long after the player has gone
	constexpr float SeparationCm = 150.f;
	constexpr int32 BirdBudget = 600;
}

void ASimFauna::AddFlocks(int32 SpeciesIndex, int32 SimDay)
{
	const FSimFaunaSpecies& S = Species[SpeciesIndex];
	const FString MeshName = S.Model.StartsWith(TEXT("fish_")) ? TEXT("SM_Fish_") + S.Model.Mid(5) : TEXT("SM_Bird_") + S.Model.Mid(5);
	const FString Pkg = FString::Printf(TEXT("%s/%s"), FaunaDir, *MeshName);
	if (!bUseFaunaMeshes || ForceMissingModels.Contains(S.Model) || !FPackageName::DoesPackageExist(Pkg))
	{
		return;
	}
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *Pkg, *MeshName));
	if (Mesh == nullptr)
	{
		return;
	}
	const FFlockKind K = FlockKindOf(S.Id);
	const FSimCityFrame& Fr = SimCityData::Frame();
	TArray<FSimDoorSlotInfo> Slots;
	ASimStreetBuilder::GetAllDoorSlots(Slots);
	Slots.Sort([](const FSimDoorSlotInfo& A, const FSimDoorSlotInfo& B) { return A.PlaceId.LexicalLess(B.PlaceId); });
	int32 Birds = 0;
	for (const FSimFlock& F : Flocks)
	{
		Birds += F.Pos.Num();
	}
	for (int32 h = 0; h < S.Habitats.Num(); ++h)
	{
		const FString& H = S.Habitats[h];
		for (int32 g = 0; g < FlockGroups(H); ++g)
		{
			const uint32 Seed = HashCombine(GetTypeHash(S.Id), static_cast<uint32>(SimDay * 53 + h * 7 + g));
			const float R0 = SimHash01(Seed, 1), R1 = SimHash01(Seed, 2);
			FVector2D At;
			float Ground = 0.f;
			if (H == TEXT("water") || H == TEXT("shore"))
			{
				const float Rad = H == TEXT("water") ? Fr.LagoonR * (0.3f + 0.4f * R1) : Fr.LagoonR - 250.f;
				At = Fr.Center + Along(360.f * R0) * Rad;
				Ground = H == TEXT("water") ? ASimEnvironment::WaterHeight() : ASimEnvironment::TerrainHeight(At.X, At.Y);
			}
			else if (H == TEXT("precinct") || H == TEXT("tombs"))
			{
				const FSimCityPlace* P = nullptr;
				for (const FSimCityPlace& X : SimCityData::Places())
				{
					if (H == TEXT("precinct") ? X.Typology == TEXT("ziggurat") : X.Quarter == TEXT("garden_of_tombs"))
					{
						P = &X;
						break;
					}
				}
				if (P == nullptr)
				{
					continue;
				}
				At = P->Center + Along(360.f * R0) * 800.f * R1;
				Ground = ASimEnvironment::TerrainHeight(At.X, At.Y);
			}
			else if (Slots.Num() > 0)  // roof, street_edge, open: over and before the houses
			{
				const FSimDoorSlotInfo& D = Slots[FMath::Min(Slots.Num() - 1, FMath::FloorToInt(R0 * Slots.Num()))];
				At = FVector2D(D.Location) + Along(D.OutwardYawDeg) * (H == TEXT("roof") ? -400.f : 300.f);
				Ground = H == TEXT("roof") ? 300.f : ASimEnvironment::TerrainHeight(At.X, At.Y);
			}
			else
			{
				continue;
			}
			const int32 Size = S.GroupMin + FMath::FloorToInt(SimHash01(Seed, 3) * (S.GroupMax - S.GroupMin + 1));
			if (Size <= 0 || Birds + Size > BirdBudget)
			{
				continue;
			}
			FSimFlock& F = Flocks.AddDefaulted_GetRef();
			F.Species = SpeciesIndex;
			F.Anchor = FVector(At, Ground);
			F.Radius = H == TEXT("roof") ? 900.f : K.Radius;
			F.MinZ = K.MinZ;
			F.MaxZ = H == TEXT("roof") ? 800.f : K.MaxZ;
			F.FlapHz = K.FlapHz;
			F.bGrounded = K.bGrounded;
			F.bSoar = K.bSoar;
			UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this,
				FName(*FString::Printf(TEXT("Flock_%s_%d"), *S.Id.ToString(), Flocks.Num() - 1)));
			Ism->SetupAttachment(RootComponent);
			Ism->SetStaticMesh(Mesh);
			Ism->NumCustomDataFloats = 1;
			Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Ism->SetCastShadow(false);
			Ism->RegisterComponent();
			AddInstanceComponent(Ism);
			FlockMeshes.Add(Ism);
			for (int32 b = 0; b < Size; ++b)
			{
				const float A = 360.f * SimHash01(Seed, 10 + b), D = F.Radius * 0.6f * FMath::Sqrt(SimHash01(Seed, 200 + b));
				const float Z = F.bGrounded ? 0.f : FMath::Lerp(F.MinZ, F.MaxZ, SimHash01(Seed, 400 + b));
				F.Pos.Add(FVector(At + Along(A) * D, F.Anchor.Z + Z));
				F.Vel.Add(F.bGrounded ? FVector::ZeroVector : FVector(Along(A + 90.f) * 400.f, 0.f));
				Ism->AddInstance(FTransform(F.Pos.Last()), true);
				Ism->SetCustomDataValue(b, 0, F.bGrounded ? 0.f : F.FlapHz, false);
			}
			Birds += Size;
		}
	}
}

void ASimFauna::StepFlocks(float Dt, float Hour, FVector2D Player)
{
	if (Dt <= 0.f)
	{
		return;
	}
	for (int32 f = 0; f < Flocks.Num(); ++f)
	{
		FSimFlock& F = Flocks[f];
		const FSimFaunaSpecies& S = Species[F.Species];
		const bool bActive = S.IsActive(Hour);
		const bool bCarp = S.Id == TEXT("carp");
		UInstancedStaticMeshComponent* Ism = FlockMeshes[f];
		// Outside its hours a flying flock is gone to roost; a standing one sleeps where it stands.
		if (Ism != nullptr)
		{
			Ism->SetVisibility(bActive || F.bGrounded);
		}
		if (!bActive)
		{
			continue;
		}
		const int32 N = F.Pos.Num();
		// Scatter: the player walks up to a standing flock -> it lifts, and settles 20 s after they have gone.
		if (F.bGrounded && !bCarp)
		{
			bool bNear = false;
			for (const FVector& P : F.Pos)
			{
				bNear |= FVector2D::Distance(FVector2D(P), Player) < LiftCm;
			}
			if (bNear)
			{
				if (F.LiftTimer <= 0.f)
				{
					OnAnimalCue.Broadcast(S.Id, TEXT("lift"));
					for (int32 i = 0; i < N; ++i)
					{
						const FVector2D Away = (FVector2D(F.Pos[i]) - Player).GetSafeNormal();
						F.Vel[i] = FVector(Away * 300.f, 450.f);
					}
				}
				F.LiftTimer = ResettleS;
			}
			else if (F.LiftTimer > 0.f)
			{
				F.LiftTimer = FMath::Max(0.f, F.LiftTimer - Dt);
			}
		}
		const bool bFlying = !F.bGrounded || F.LiftTimer > 0.f;
		const float MinZ = F.bGrounded ? 300.f : F.MinZ, MaxZ = F.bGrounded ? 1500.f : F.MaxZ;
		FVector Centre = FVector::ZeroVector, AvgVel = FVector::ZeroVector;
		for (int32 i = 0; i < N; ++i)
		{
			Centre += F.Pos[i] / N;
			AvgVel += F.Vel[i] / N;
		}
		for (int32 i = 0; i < N; ++i)
		{
			FVector& P = F.Pos[i];
			FVector& V = F.Vel[i];
			const uint32 Salt = StepCount * 131u + static_cast<uint32>(f * 1009 + i);
			if (bFlying)
			{
				FVector Sep = FVector::ZeroVector;
				for (int32 j = 0; j < N; ++j)
				{
					const FVector D = P - F.Pos[j];
					const float L = D.Size();
					if (j != i && L < SeparationCm && L > 1.f)
					{
						Sep += D / (L * L) * SeparationCm;
					}
				}
				FVector Steer = Sep * 300.f + (AvgVel - V) * 0.5f + (Centre - P) * 0.3f;
				const FVector2D FromAnchor = FVector2D(P) - FVector2D(F.Anchor);
				const float Out = FromAnchor.Size() / F.Radius;
				if (Out > 0.7f)
				{
					Steer += FVector(-FromAnchor.GetSafeNormal() * 600.f * (Out - 0.7f) * 4.f, 0.f);  // the leash
				}
				if (F.bSoar)
				{
					Steer += FVector(FVector2D(-FromAnchor.Y, FromAnchor.X).GetSafeNormal() * 200.f, 0.f);  // wide circles
				}
				const float Z = P.Z - F.Anchor.Z;
				Steer.Z += Z < MinZ ? (MinZ - Z) * 2.f : (Z > MaxZ ? (MaxZ - Z) * 2.f : 0.f);
				Steer += FVector(SimHash01(Salt, 1) - 0.5f, SimHash01(Salt, 2) - 0.5f, (SimHash01(Salt, 3) - 0.5f) * 0.3f) * 300.f;
				V += Steer * Dt;
				const float Speed = V.Size(), Lo = F.bSoar ? 500.f : 300.f, Hi = F.bSoar ? 800.f : 900.f;
				if (Speed > Hi) { V *= Hi / Speed; }
				else if (Speed < Lo && Speed > 1.f) { V *= Lo / Speed; }
				P += V * Dt;
			}
			else if (F.bGrounded)
			{
				// Standing, wading, floating or pecking: a slow wander, and down to the ground after a flight.
				const float Z = P.Z - F.Anchor.Z;
				if (Z > 1.f)
				{
					P.Z = F.Anchor.Z + FMath::Max(0.f, Z - 250.f * Dt);
					P += FVector(FVector2D(V) * Dt * 0.5f, 0.f);
					V *= 0.9f;
				}
				else
				{
					P.Z = F.Anchor.Z;
					if (SimHash01(Salt, 4) < 0.02f)
					{
						V = FVector(Along(360.f * SimHash01(Salt, 5)) * 30.f, 0.f);
					}
					P += FVector(FVector2D(V) * Dt, 0.f);
				}
			}
			// Carp: under the surface, one leaps now and then.
			float Draw = P.Z;
			if (bCarp)
			{
				const float Phase = FMath::Fmod(Hour * 3600.f / 7.f + i * 1.7f + f * 3.1f, 1.f);  // a leap every 7 s (sim time), staggered
				Draw = F.Anchor.Z - 40.f + (Phase < 0.12f ? 120.f * FMath::Sin(Phase / 0.12f * PI) : 0.f);
			}
			// Never out of the flock's ground: 1.2 x its radius (the tests allow 1.5).
			const FVector2D Off = FVector2D(P) - FVector2D(F.Anchor);
			if (Off.Size() > F.Radius * 1.2f)
			{
				const FVector2D In = FVector2D(F.Anchor) + Off.GetSafeNormal() * F.Radius * 1.2f;
				P.X = In.X;
				P.Y = In.Y;
			}
			if (Ism != nullptr)
			{
				const float Yaw = FVector2D(V).SizeSquared() > 1.f ? FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)) : 360.f * SimHash01(f, i, 7);
				Ism->UpdateInstanceTransform(i, FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, Draw)), true, false, true);
				Ism->SetCustomDataValue(i, 0, bFlying ? F.FlapHz : 0.f, false);
			}
		}
		if (Ism != nullptr)
		{
			Ism->MarkRenderStateDirty();
		}
	}
}
