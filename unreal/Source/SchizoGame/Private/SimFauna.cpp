// SimFauna.cpp — Stage V batch 4 Task 3: herds, work beasts, city animals and the night's predators.
#include "SimFauna.h"

#include "SimCityData.h"
#include "SimEnvironment.h"
#include "SimMeshKit.h"
#include "SimStreetBuilder.h"
#include "SimWorldSubsystem.h"
#include "sim/CApiWild.h"

#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
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
	PrimaryActorTick.TickInterval = 0.2f;  // the herd thinks at 5 Hz
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
	LoadSpecies();
	LastDay = SimDay;
	LastHerd = HerdHead;
	for (int32 s = 0; s < Species.Num(); ++s)
	{
		const FSimFaunaSpecies& S = Species[s];
		if (!Skeletal(S) || SpeciesMesh[s] == nullptr)
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
	UE_LOG(LogSimFauna, Log, TEXT("fauna: day %d, herd %d head: %d animals."), SimDay, HerdHead, Animals.Num());
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
