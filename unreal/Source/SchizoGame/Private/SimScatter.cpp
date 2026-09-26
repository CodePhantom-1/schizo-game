// SimScatter.cpp — Stage V batch 2 Task 5: the city wears the scatter, withering and flowering with
// the kernel. scatter.csv is written by tools/art/scatter.py (checked current in CI); the meshes by
// tools/art/ue_import_scatter.py. A fresh clone before that import stands nothing and logs one line.
#include "SimScatter.h"

#include "SimCityData.h"
#include "SimEnvironment.h"
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

FSimScatterResult ASimScatter::Build()
{
	FSimScatterResult R;
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *ScatterCsvPath()) || Lines.Num() < 2)
	{
		UE_LOG(LogSimScatter, Warning, TEXT("scatter: %s missing or empty — run tools/art/scatter.py."), *ScatterCsvPath());
		return R;
	}
	const FString MpcPath = FString::Printf(TEXT("%s/MPC_World"), ScatterDir);
	if (FPackageName::DoesPackageExist(MpcPath))
	{
		WorldParams = LoadObject<UMaterialParameterCollection>(nullptr, *(MpcPath + TEXT(".MPC_World")));
	}
	const TMap<FString, FString> Groups = MeshGroups();
	TMap<FString, int32> ByKey;  // "mesh|seasons" -> index into Hisms
	TSet<FString> Missing, Standing;
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		const TArray<FString> F = SimCityData::SplitCsv(Lines[i]);
		if (F.Num() < 7)
		{
			continue;
		}
		const FString& Mesh = F[0];
		if (!bUseScatterMeshes || Missing.Contains(Mesh))
		{
			Missing.Add(Mesh);
			continue;
		}
		const FString Key = Mesh + TEXT("|") + F[6];
		int32* Found = ByKey.Find(Key);
		if (Found == nullptr)
		{
			UStaticMesh* SM = MeshExists(Mesh)
				? LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/SM_F_%s.SM_F_%s"), ScatterDir, *Mesh, *Mesh))
				: nullptr;
			if (SM == nullptr)
			{
				Missing.Add(Mesh);
				continue;
			}
			const FString Group = Groups.FindRef(Mesh);
			UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(
				this, FName(*FString::Printf(TEXT("Scatter_%s_%d"), *Mesh, Hisms.Num())));
			H->SetupAttachment(RootComponent);
			H->SetMobility(EComponentMobility::Static);
			H->SetStaticMesh(SM);
			const float Cull = CullFor(Group);
			H->SetCullDistances(Cull > 0.f ? Cull * 0.8f : 0.f, Cull);
			const bool bSmall = Group == TEXT("grass") || Group == TEXT("flower") || Group == TEXT("water");
			H->SetCastShadow(!bSmall);  // blades and petals: shadows cost more than they show
			if (SM->GetBodySetup() == nullptr || SM->GetBodySetup()->AggGeom.GetElementCount() == 0)
			{
				H->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // imported without a box: walk through
			}
			H->RegisterComponent();
			AddInstanceComponent(H);
			TArray<FString> Seasons;
			if (F[6] != TEXT("all"))
			{
				F[6].ParseIntoArray(Seasons, TEXT(";"));
			}
			Found = &ByKey.Add(Key, Hisms.Add(H));
			HismSeasons.Add(Seasons);
			Standing.Add(Mesh);
		}
		const float X = FCString::Atof(*F[1]), Y = FCString::Atof(*F[2]);
		const float Z = Floats(Mesh) ? ASimEnvironment::WaterHeight() : ASimEnvironment::TerrainHeight(X, Y);
		const float Scale = FCString::Atof(*F[4]);
		Hisms[*Found]->AddInstance(FTransform(FRotator(0.f, FCString::Atof(*F[3]), 0.f), FVector(X, Y, Z), FVector(Scale)), true);
		++R.NumInstances;
	}
	R.NumMeshes = Standing.Num();
	R.NumMissingMeshes = Missing.Num();
	if (R.NumMissingMeshes > 0)
	{
		// One line, however many rows: a fresh clone before ue_import_scatter.py misses them all.
		UE_LOG(LogSimScatter, Log, TEXT("scatter: %d mesh(es) not imported, their rows skipped — run tools/art/ue_import_scatter.py."),
			R.NumMissingMeshes);
	}
	UE_LOG(LogSimScatter, Log, TEXT("scatter: %d instances of %d meshes in %d HISMs."), R.NumInstances, R.NumMeshes, Hisms.Num());
	return R;
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
