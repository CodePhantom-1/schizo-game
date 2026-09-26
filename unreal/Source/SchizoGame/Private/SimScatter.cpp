// SimScatter.cpp — Stage V batch 2 Task 5: the city wears the scatter, withering and flowering with
// the kernel. scatter.csv is written by tools/art/scatter.py (checked current in CI); the meshes by
// tools/art/ue_import_scatter.py. A fresh clone before that import stands nothing and logs one line.
#include "SimScatter.h"

#include "SimCityData.h"
#include "SimEnvironment.h"
#include "SimMeshKit.h"
#include "SimWorldSubsystem.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimScatter, Log, All);

namespace
{
	const TCHAR* ScatterDir = TEXT("/Game/Art/Scatter");
	const FName WitherName(TEXT("Wither"));

	/** mesh id -> its flora group (the first flora.csv row that names it). */
	TMap<FString, FString> MeshGroups()
	{
		TMap<FString, FString> Out;
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Sim/canon/flora.csv")));
		if (Lines.Num() == 0)
		{
			return Out;
		}
		const TArray<FString> Head = SimCityData::SplitCsv(Lines[0]);
		const int32 GroupCol = Head.IndexOfByKey(TEXT("group")), MeshesCol = Head.IndexOfByKey(TEXT("meshes"));
		for (int32 i = 1; i < Lines.Num(); ++i)
		{
			const TArray<FString> F = SimCityData::SplitCsv(Lines[i]);
			if (GroupCol == INDEX_NONE || MeshesCol == INDEX_NONE || F.Num() <= FMath::Max(GroupCol, MeshesCol))
			{
				continue;
			}
			TArray<FString> Meshes;
			F[MeshesCol].ParseIntoArray(Meshes, TEXT(";"));
			for (const FString& M : Meshes)
			{
				if (!Out.Contains(M))
				{
					Out.Add(M, F[GroupCol]);
				}
			}
		}
		return Out;
	}

	/** Cull distance (cm, 0 = never) by flora group: grass and flowers near, shrubs and props farther, trees never. */
	float CullFor(const FString& Group)
	{
		if (Group == TEXT("grass") || Group == TEXT("flower"))
		{
			return 6000.f;
		}
		if (Group == TEXT("tree"))
		{
			return 0.f;
		}
		return 15000.f;
	}

	/** A flora.csv row, as the countryside scatter reads it. */
	struct FSimFloraRow
	{
		FString Id, Group, Seasons;
		TArray<FString> Habitats, Meshes;
		float Per100m2 = 0.f, ScaleMin = 1.f, ScaleMax = 1.f;
	};

	TArray<FSimFloraRow> ReadFlora()
	{
		TArray<FSimFloraRow> Out;
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Sim/canon/flora.csv")));
		if (Lines.Num() == 0)
		{
			return Out;
		}
		const TArray<FString> Head = SimCityData::SplitCsv(Lines[0]);
		auto Col = [&Head](const TCHAR* Name) { return Head.IndexOfByKey(FString(Name)); };
		const int32 CId = Col(TEXT("id")), CGroup = Col(TEXT("group")), CHab = Col(TEXT("habitats")), CSeason = Col(TEXT("seasons")),
			CMesh = Col(TEXT("meshes")), CDens = Col(TEXT("per_100m2")), CMin = Col(TEXT("scale_min")), CMax = Col(TEXT("scale_max"));
		for (int32 i = 1; i < Lines.Num(); ++i)
		{
			const TArray<FString> F = SimCityData::SplitCsv(Lines[i]);
			if (F.Num() < Head.Num() || CId == INDEX_NONE || CMax == INDEX_NONE)
			{
				continue;
			}
			FSimFloraRow& R = Out.AddDefaulted_GetRef();
			R.Id = F[CId];
			R.Group = F[CGroup];
			R.Seasons = F[CSeason];
			F[CHab].ParseIntoArray(R.Habitats, TEXT(";"));
			F[CMesh].ParseIntoArray(R.Meshes, TEXT(";"));
			R.Per100m2 = FCString::Atof(*F[CDens]);
			R.ScaleMin = FCString::Atof(*F[CMin]);
			R.ScaleMax = FCString::Atof(*F[CMax]);
		}
		return Out;
	}

	// The countryside (V-B3): a jittered 4 m grid over the land round the crescent, plus the tells.
	constexpr float CountryCellCm = 400.f;
	constexpr float RoadClearCm = 600.f;   // 6 m either side of the road west (Review Focus 5)
	constexpr int32 CountryBudget = 60000;
	const FVector2D CountryMin(-90000.f, -60000.f), CountryMax(60000.f, 70000.f);

	/** The countryside's density against flora.csv's per_100m2 (the table's numbers are the city's). */
	float HabitatDensity(const TCHAR* Habitat)
	{
		const FString H(Habitat);
		if (H == TEXT("desert")) { return 0.2f; }   // open dune scrub, not a meadow
		if (H == TEXT("levee")) { return 0.5f; }    // groves along the banks, not a forest
		if (H == TEXT("tell_top")) { return 0.05f; }  // sherd patches on a 250-600 m mound, not a carpet
		return 1.f;
	}

	bool Floats(const FString& MeshId)
	{
		return MeshId.Contains(TEXT("lily"));  // lilies ride the water; reeds root in the shallows
	}
}

bool ASimScatter::bUseScatterMeshes = true;

ASimScatter::ASimScatter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f;  // the drought and the season change by the day, not the frame
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

FString ASimScatter::ScatterCsvPath()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Sim/scatter.csv"));
}

bool ASimScatter::MeshExists(const FString& MeshId)
{
	return FPackageName::DoesPackageExist(FString::Printf(TEXT("%s/SM_F_%s"), ScatterDir, *MeshId));
}

FSimScatterResult ASimScatter::BuildScatter(UWorld* World)
{
	if (World == nullptr)
	{
		return FSimScatterResult();
	}
	for (TActorIterator<ASimScatter> It(World); It; ++It)
	{
		return It->LastResult;  // one scatter per world
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASimScatter* S = World->SpawnActor<ASimScatter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (S == nullptr)
	{
		UE_LOG(LogSimScatter, Warning, TEXT("BuildScatter: spawn failed."));
		return FSimScatterResult();
	}
	S->LastResult = S->Build();
	S->ApplyWorldState(FMath::Max(0, USimWorldSubsystem::GetSimDroughtFor(World)), USimWorldSubsystem::GetSimSeasonFor(World));
	return S->LastResult;
}

int32 ASimScatter::HismFor(const FString& Mesh, const FString& SeasonsCsv, bool bCountry)
{
	const FString Key = FString::Printf(TEXT("%s|%s|%s"), bCountry ? TEXT("C") : TEXT("T"), *Mesh, *SeasonsCsv);
	if (const int32* Found = HismByKey.Find(Key))
	{
		return *Found;
	}
	if (!bUseScatterMeshes || MissingMeshes.Contains(Mesh))
	{
		MissingMeshes.Add(Mesh);
		return INDEX_NONE;
	}
	UStaticMesh* SM = MeshExists(Mesh)
		? LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/SM_F_%s.SM_F_%s"), ScatterDir, *Mesh, *Mesh))
		: nullptr;
	if (SM == nullptr)
	{
		MissingMeshes.Add(Mesh);
		return INDEX_NONE;
	}
	const FString Group = MeshGroupOf.FindRef(Mesh);
	UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(
		this, FName(*FString::Printf(TEXT("%s_%s_%d"), bCountry ? TEXT("Country") : TEXT("Scatter"), *Mesh, Hisms.Num())));
	H->SetupAttachment(RootComponent);
	H->SetMobility(EComponentMobility::Static);
	H->SetStaticMesh(SM);
	float Cull = CullFor(Group);
	if (bCountry && (Group == TEXT("crop") || Group == TEXT("grass")))
	{
		Cull = 25000.f;  // the fields read from the road: 250 m
	}
	H->SetCullDistances(Cull > 0.f ? Cull * 0.8f : 0.f, Cull);
	const bool bSmall = Group == TEXT("grass") || Group == TEXT("flower") || Group == TEXT("water") || Group == TEXT("crop");
	H->SetCastShadow(!bSmall);  // blades and petals: shadows cost more than they show
	if (SM->GetBodySetup() == nullptr || SM->GetBodySetup()->AggGeom.GetElementCount() == 0)
	{
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // imported without a box: walk through
	}
	H->RegisterComponent();
	AddInstanceComponent(H);
	TArray<FString> Seasons;
	if (SeasonsCsv != TEXT("all"))
	{
		SeasonsCsv.ParseIntoArray(Seasons, TEXT(";"));
	}
	const int32 Index = Hisms.Add(H);
	HismSeasons.Add(Seasons);
	HismByKey.Add(Key, Index);
	StandingMeshes.Add(Mesh);
	return Index;
}

FSimScatterResult ASimScatter::Build()
{
	FSimScatterResult R;
	const FString MpcPath = FString::Printf(TEXT("%s/MPC_World"), ScatterDir);
	if (FPackageName::DoesPackageExist(MpcPath))
	{
		WorldParams = LoadObject<UMaterialParameterCollection>(nullptr, *(MpcPath + TEXT(".MPC_World")));
	}
	MeshGroupOf = MeshGroups();
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *ScatterCsvPath()) || Lines.Num() < 2)
	{
		UE_LOG(LogSimScatter, Warning, TEXT("scatter: %s missing or empty — run tools/art/scatter.py."), *ScatterCsvPath());
	}
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		const TArray<FString> F = SimCityData::SplitCsv(Lines[i]);
		if (F.Num() < 7)
		{
			continue;
		}
		const FString& Mesh = F[0];
		const int32 H = HismFor(Mesh, F[6], false);
		if (H == INDEX_NONE)
		{
			continue;
		}
		const float X = FCString::Atof(*F[1]), Y = FCString::Atof(*F[2]);
		const float Z = Floats(Mesh) ? ASimEnvironment::WaterHeight() : ASimEnvironment::TerrainHeight(X, Y);
		const float Scale = FCString::Atof(*F[4]);
		Hisms[H]->AddInstance(FTransform(FRotator(0.f, FCString::Atof(*F[3]), 0.f), FVector(X, Y, Z), FVector(Scale)), true);
		++R.NumInstances;
	}
	ScatterCountryside(R);
	R.NumMeshes = StandingMeshes.Num();
	R.NumMissingMeshes = MissingMeshes.Num();
	if (R.NumMissingMeshes > 0)
	{
		// One line, however many rows: a fresh clone before ue_import_scatter.py misses them all.
		UE_LOG(LogSimScatter, Log, TEXT("scatter: %d mesh(es) not imported, their rows skipped — run tools/art/ue_import_scatter.py."),
			R.NumMissingMeshes);
	}
	UE_LOG(LogSimScatter, Log, TEXT("scatter: %d city + %d countryside instances of %d meshes in %d HISMs."),
		R.NumInstances, R.NumCountryside, R.NumMeshes, Hisms.Num());
	return R;
}

void ASimScatter::ScatterCountryside(FSimScatterResult& R)
{
	ASimEnvironment::LoadFrame();
	const TArray<FSimFloraRow> Flora = ReadFlora();
	FVector2D Gate(6570.f, 470.f);
	if (const FSimCityPlace* G = SimCityData::Find(TEXT("moon_gate_place")))
	{
		Gate = G->Center;
	}
	// Tommy's countryside palms, bucketed by 10 m for the 3 m keep-clear test.
	TMap<FIntPoint, TArray<FVector2D>> Palms;
	for (const FVector2D& P : ASimEnvironment::GetCountryPalms())
	{
		Palms.FindOrAdd(FIntPoint(FMath::FloorToInt(P.X / 1000.f), FMath::FloorToInt(P.Y / 1000.f))).Add(P);
	}
	auto Clear = [&](float X, float Y)
	{
		if (X < Gate.X && FMath::Abs(Y - Gate.Y) < RoadClearCm)
		{
			return false;  // the road west, the player's route (Review Focus 5)
		}
		const FIntPoint C(FMath::FloorToInt(X / 1000.f), FMath::FloorToInt(Y / 1000.f));
		for (int32 dx = -1; dx <= 1; ++dx)
		{
			for (int32 dy = -1; dy <= 1; ++dy)
			{
				if (const TArray<FVector2D>* Near = Palms.Find(C + FIntPoint(dx, dy)))
				{
					for (const FVector2D& P : *Near)
					{
						if (FVector2D::Distance(P, FVector2D(X, Y)) < 300.f)
						{
							return false;
						}
					}
				}
			}
		}
		return true;
	};

	auto Visit = [&](int32 IX, int32 IY)
	{
		const float X = (IX + SimHash01(IX, IY, 201)) * CountryCellCm, Y = (IY + SimHash01(IX, IY, 202)) * CountryCellCm;
		const float Z = ASimEnvironment::TerrainHeight(X, Y);
		const ESimGround Kind = ASimEnvironment::GroundKind(X, Y, Z, 1.f, static_cast<uint32>(IX * 7919 + IY));
		const TCHAR* Habitat = nullptr;
		if (Kind == ESimGround::Tell)
		{
			Habitat = TEXT("tell_top");
		}
		else if (ASimEnvironment::LeveeAt(X, Y) > 0.5f && Kind != ESimGround::Street && Kind != ESimGround::Road && Kind != ESimGround::Bed)
		{
			Habitat = TEXT("levee");
		}
		else if (Kind == ESimGround::Irrigated) { Habitat = TEXT("field"); }
		else if (Kind == ESimGround::ReedMud) { Habitat = TEXT("bank"); }
		else if (Kind == ESimGround::Dune || Kind == ESimGround::Sand) { Habitat = TEXT("desert"); }
		if (Habitat == nullptr)
		{
			return;
		}
		const float Thin = HabitatDensity(Habitat);
		for (int32 k = 0; k < Flora.Num(); ++k)
		{
			const FSimFloraRow& F = Flora[k];
			if (!F.Habitats.Contains(Habitat) || F.Meshes.Num() == 0)
			{
				continue;
			}
			const float Expect = F.Per100m2 / 100.f * (CountryCellCm * CountryCellCm / 10000.f) * Thin;
			const int32 N = FMath::FloorToInt(Expect) + (SimHash01(IX, IY, 300 + k) < FMath::Frac(Expect) ? 1 : 0);
			for (int32 c = 0; c < N && R.NumCountryside < CountryBudget; ++c)
			{
				const int32 S = 400 + k * 16 + c * 4;
				const float Px = X + (SimHash01(IX, IY, S) - 0.5f) * CountryCellCm * 0.8f;
				const float Py = Y + (SimHash01(IX, IY, S + 1) - 0.5f) * CountryCellCm * 0.8f;
				if (!Clear(Px, Py))
				{
					continue;
				}
				const FString& Mesh = F.Meshes[FMath::Min(F.Meshes.Num() - 1, FMath::FloorToInt(SimHash01(IX, IY, S + 2) * F.Meshes.Num()))];
				const int32 H = HismFor(Mesh, F.Seasons, true);
				if (H == INDEX_NONE)
				{
					continue;
				}
				const float Scale = FMath::Lerp(F.ScaleMin, F.ScaleMax, SimHash01(IX, IY, S + 3));
				const float Yaw = 360.f * SimHash01(IX, IY, S + 5);
				const float Pz = Floats(Mesh) ? ASimEnvironment::WaterHeight() : ASimEnvironment::TerrainHeight(Px, Py);
				Hisms[H]->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(Px, Py, Pz), FVector(Scale)), true);
				++R.NumCountryside;
				R.CountrysideByHabitat.FindOrAdd(Habitat)++;
			}
		}
	};

	// The land round the crescent (the fields, the river banks, the desert south and west)...
	const FIntPoint Lo(FMath::FloorToInt(CountryMin.X / CountryCellCm), FMath::FloorToInt(CountryMin.Y / CountryCellCm));
	const FIntPoint Hi(FMath::FloorToInt(CountryMax.X / CountryCellCm), FMath::FloorToInt(CountryMax.Y / CountryCellCm));
	for (int32 IY = Lo.Y; IY < Hi.Y; ++IY)
	{
		for (int32 IX = Lo.X; IX < Hi.X; ++IX)
		{
			Visit(IX, IY);
		}
	}
	// ...and the tells out on the horizon (their sherds).
	for (const FSimTell& T : ASimEnvironment::GetTells())
	{
		for (int32 IY = FMath::FloorToInt((T.Center.Y - T.Radius) / CountryCellCm); IY * CountryCellCm < T.Center.Y + T.Radius; ++IY)
		{
			for (int32 IX = FMath::FloorToInt((T.Center.X - T.Radius) / CountryCellCm); IX * CountryCellCm < T.Center.X + T.Radius; ++IX)
			{
				if (IX < Lo.X || IX >= Hi.X || IY < Lo.Y || IY >= Hi.Y)
				{
					Visit(IX, IY);
				}
			}
		}
	}
	if (R.NumCountryside >= CountryBudget)
	{
		UE_LOG(LogSimScatter, Warning, TEXT("scatter: the countryside hit its %d-instance budget."), CountryBudget);
	}
}

void ASimScatter::ApplyWorldState(int32 DroughtStage, const FString& Season)
{
	Wither = FMath::Clamp(DroughtStage / 4.f, 0.f, 1.f);
	LastDrought = DroughtStage;
	LastSeason = Season;
	if (WorldParams != nullptr && GetWorld() != nullptr)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), WorldParams, WitherName, Wither);
	}
	for (int32 i = 0; i < Hisms.Num(); ++i)
	{
		const bool bInSeason = HismSeasons[i].Num() == 0 || Season.IsEmpty() || HismSeasons[i].Contains(Season);
		Hisms[i]->SetVisibility(bInSeason);
	}
}

void ASimScatter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	const int64 Day = USimWorldSubsystem::GetSimDayFor(World);
	if (Day < 0)
	{
		return;  // the sim world does not exist yet
	}
	// The day turns the season; sim.Drought changes the drought mid-day: follow both.
	const int32 Drought = USimWorldSubsystem::GetSimDroughtFor(World);
	if (Day != LastDay || Drought != LastDrought)
	{
		LastDay = Day;
		ApplyWorldState(FMath::Max(0, Drought), USimWorldSubsystem::GetSimSeasonFor(World));
	}
}
