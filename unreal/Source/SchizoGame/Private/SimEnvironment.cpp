// SimEnvironment.cpp — see SimEnvironment.h. All placement here is INVENTED
// scenery (D-018 ledger: docs/proposals/invented-ledger-environment.md),
// grounded in world-bible §5 (City of the Moon ~ Ur: temple of sun and moon,
// the Great Lighthouse, sea trade) and §2 (the drought).
#include "SimEnvironment.h"

#include "SimMeshKit.h"
#include "SimStreetBuilder.h"
#include "SimWorldSubsystem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimEnvironment, Log, All);

namespace
{
	// --- the city (cm; +X east, +Y north; the Moon Gate at the origin) ----
	constexpr float CityX0 = 0.f, CityX1 = 36000.f, CityY0 = -14000.f, CityY1 = 22000.f;
	constexpr float WallH = 950.f;
	constexpr float WaterZ = -120.f;
	const FVector2D ZigCenter(22000.f, 3000.f);
	const FVector2D LighthousePos(48500.f, 6000.f);
	constexpr float SeaGateY = 6000.f;
	const float CanalYs[] = { -21000.f, -9000.f, 9000.f, 21000.f };
	constexpr float CanalX = -30000.f;

	float Smooth(float A, float B, float X) { return FMath::SmoothStep(A, B, X); }
	float Perlin(float X, float Y) { return FMath::PerlinNoise2D(FVector2D(X, Y)); }

	float CityDist(float X, float Y)
	{
		const float Dx = FMath::Max3(CityX0 - X, 0.f, X - CityX1);
		const float Dy = FMath::Max3(CityY0 - Y, 0.f, Y - CityY1);
		return FMath::Sqrt(Dx * Dx + Dy * Dy);
	}
	float RiverY(float X) { return -34000.f + 5000.f * FMath::Sin(X / 30000.f) + 1800.f * FMath::Sin(X / 9000.f + 1.f); }
	float CoastX(float Y) { return 38500.f + 1500.f * FMath::Sin(Y / 20000.f) + 600.f * FMath::Sin(Y / 5000.f); }
	bool InFarmland(float X, float Y) { return X > -70000.f && X < -5000.f && FMath::Abs(Y) < 29000.f; }

	/** The quarter's street and its plots (SimStreetBuilder's layout). */
	bool InStreetZone(float X, float Y)
	{
		return (X > -500.f && X < 12600.f && FMath::Abs(Y) < 1400.f)
			|| (FMath::Abs(X - 11200.f) < 1500.f && Y > -1400.f && Y < 9400.f);
	}

	/** Grid coordinates: Fine steps within FineHalf of Center, then growing to Far. */
	void AxisCoords(float Center, float Fine, float FineHalf, float Far, float Growth, TArray<float>& Out)
	{
		TArray<float> Pos;
		float D = 0.f, Step = Fine;
		while (D < Far)
		{
			D += Step;
			Pos.Add(D);
			if (D > FineHalf)
			{
				Step *= Growth;
			}
		}
		Out.Reset();
		for (int32 i = Pos.Num() - 1; i >= 0; --i)
		{
			Out.Add(Center - Pos[i]);
		}
		Out.Add(Center);
		for (float P : Pos)
		{
			Out.Add(Center + P);
		}
	}

	// --- palette (sRGB) ---------------------------------------------------
	const FLinearColor Earth = SimRGB(150, 128, 102);
	const FLinearColor MudBrick = SimRGB(166, 138, 108);
	const FLinearColor MudTop = SimRGB(182, 156, 122);
	const FLinearColor BakedBrick = SimRGB(150, 114, 88);
	const FLinearColor Lapis = SimRGB(34, 66, 140);
	const FLinearColor Gold = SimRGB(214, 170, 64);
	const FLinearColor Stone = SimRGB(168, 150, 120);
	const FLinearColor Whitewash = SimRGB(222, 208, 178);
	const FLinearColor Bronze = SimRGB(92, 70, 42);

	FLinearColor Jit(const FLinearColor& C, float H, float Amount = 0.12f)
	{
		FLinearColor R = C * (1.f - Amount * 0.5f + Amount * H);
		R.A = C.A;
		return R;
	}

	FLinearColor TerrainColor(float X, float Y, float H, float Nz, uint32 Seed)
	{
		const float J = SimHash01(Seed, 91);
		FLinearColor C;
		const float Dr = FMath::Abs(Y - RiverY(X));
		const float Cx = CoastX(Y);
		if (H < -160.f)
		{
			C = SimRGB(70, 84, 66);  // bed under the water
		}
		else if (H < -30.f)
		{
			C = SimRGB(118, 100, 72);  // wet mud at the water's edge
		}
		else if (CityDist(X, Y) < 1.f)
		{
			// Packed earth, lighter where feet wear it (the street, the gate).
			C = InStreetZone(X, Y) ? SimRGB(170, 150, 122) : Earth;
		}
		else if (X > Cx - 1800.f)
		{
			C = SimRGB(208, 186, 138);  // beach
		}
		else if (X < 0.f && FMath::Abs(Y) < 520.f)
		{
			C = SimRGB(168, 146, 116);  // the road west
		}
		else if (Dr < 6800.f)
		{
			C = (SimHash01(Seed, 3) < 0.6f) ? SimRGB(92, 108, 52) : SimRGB(122, 122, 64);  // reed banks
		}
		else if (InFarmland(X, Y))
		{
			const int32 Px = FMath::FloorToInt((X + 100000.f) / 6000.f);
			const int32 Py = FMath::FloorToInt((Y + 100000.f) / 2600.f);
			const float Crop = SimHash01(Px, Py, 17);
			if (Crop < 0.30f) { C = SimRGB(88, 118, 42); }          // green barley
			else if (Crop < 0.52f) { C = SimRGB(190, 164, 84); }    // ripe barley
			else if (Crop < 0.76f) { C = SimRGB(160, 124, 82); }    // fallow
			else { C = SimRGB(184, 152, 110); }                     // cracked, dry (the drought)
		}
		else
		{
			const float Desert = FMath::Max(Smooth(62000.f, 90000.f, -X), Smooth(44000.f, 60000.f, -Y));
			C = FMath::Lerp(SimHash01(Seed, 5) < 0.3f ? SimRGB(150, 132, 84) : SimRGB(180, 150, 104),
				SimRGB(206, 172, 120), Desert);
			if (H > 12000.f)
			{
				C = FMath::Lerp(C, SimRGB(150, 128, 104), Smooth(12000.f, 30000.f, H));  // far hills
			}
		}
		if (Nz < 0.86f)
		{
			C *= 0.86f;  // steep faces read darker
		}
		return Jit(C, J, 0.1f);
	}
}

ASimEnvironment::ASimEnvironment()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	auto MakeMesh = [this, Root](const TCHAR* Name, bool bCollide, bool bShadow)
	{
		UProceduralMeshComponent* M = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		M->SetupAttachment(Root);
		M->bUseComplexAsSimpleCollision = true;
		M->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bCollide)
		{
			M->SetCollisionProfileName(TEXT("BlockAll"));
		}
		M->SetCastShadow(bShadow);
		return M;
	};
	Terrain = MakeMesh(TEXT("Terrain"), true, true);
	Water = MakeMesh(TEXT("Water"), false, false);
	Solid = MakeMesh(TEXT("Solid"), true, true);
	Foliage = MakeMesh(TEXT("Foliage"), false, true);
	Far = MakeMesh(TEXT("Far"), false, false);
	Fire = MakeMesh(TEXT("Fire"), false, false);
	NightGlow = MakeMesh(TEXT("NightGlow"), false, false);

	Blocks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CityBlocks"));
	Blocks->SetupAttachment(Root);
	Blocks->NumCustomDataFloats = 3;
	Blocks->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Blocks->SetCollisionProfileName(TEXT("BlockAll"));
}

void ASimEnvironment::LoadMaterials()
{
	auto Mid = [this](const TCHAR* Path, const FLinearColor& Color, float Scalar, const TCHAR* ScalarName) -> UMaterialInterface*
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, Path);
		if (Base == nullptr)
		{
			UE_LOG(LogSimEnvironment, Error, TEXT("style material missing: %s (run tools/art/ue_make_style_materials.py)"), Path);
			return nullptr;
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		if (ScalarName != nullptr)
		{
			M->SetScalarParameterValue(ScalarName, Scalar);
		}
		return M;
	};
	FlatMat = Mid(TEXT("/Game/Art/Style/M_Flat.M_Flat"), FLinearColor::White, 0.f, nullptr);
	WindMat = Mid(TEXT("/Game/Art/Style/M_FlatWind.M_FlatWind"), FLinearColor::White, 16.f, TEXT("WindAmplitude"));
	WaterMat = Mid(TEXT("/Game/Art/Style/M_Water.M_Water"), FLinearColor(0.018f, 0.075f, 0.085f), 12.f, TEXT("WaveAmplitude"));
	GlowMat = Mid(TEXT("/Game/Art/Style/M_Glow.M_Glow"), FLinearColor(1.f, 0.42f, 0.1f), 40.f, TEXT("Intensity"));
	WindowGlowMat = Mid(TEXT("/Game/Art/Style/M_Glow.M_Glow"), FLinearColor(1.f, 0.55f, 0.22f), 6.f, TEXT("Intensity"));
	MoonMat = Mid(TEXT("/Game/Art/Style/M_Glow.M_Glow"), FLinearColor(1.f, 0.86f, 0.52f), 2.5f, TEXT("Intensity"));
}

ASimEnvironment* ASimEnvironment::BuildWorld(UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASimEnvironment* Env = World->SpawnActor<ASimEnvironment>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Env != nullptr)
	{
		// The street builder only lays its own ground when nothing tagged
		// SimGround stands: the terrain is the ground now.
		Env->Tags.Add(FName(TEXT("SimGround")));
		Env->Build();
	}
	return Env;
}

void ASimEnvironment::Build()
{
	const double T0 = FPlatformTime::Seconds();
	LoadMaterials();
	BuildTerrain();
	BuildWater();

	FSimMeshKit SolidK, FoliageK, FarK, FireK, MoonK, NightK;
	BuildWalls(SolidK, FireK);
	// The gate's crescent moon: its own glow section (MoonMat), see BuildWalls.
	{
		const float Zc = 1230.f, R = 185.f, Ri = 158.f, Lift = 72.f, Xf = -642.f;
		TArray<FVector> O, I;
		const int32 Steps = 14;
		for (int32 s = 0; s <= Steps; ++s)
		{
			const float Th = FMath::DegreesToRadians(200.f + 140.f * s / Steps);
			O.Add(FVector(Xf, R * FMath::Cos(Th), Zc + R * FMath::Sin(Th)));
			I.Add(FVector(Xf, Ri * FMath::Cos(Th), Zc + Lift + Ri * FMath::Sin(Th)));
		}
		I[0] = O[0];
		I[Steps] = O[Steps];
		for (int32 s = 0; s < Steps; ++s)
		{
			MoonK.Quad(O[s], O[s + 1], I[s + 1], I[s], FLinearColor::White, -FVector::ForwardVector);
		}
	}
	BuildZiggurat(SolidK, FireK);
	BuildLighthouse(SolidK, FireK);
	BuildCity(FoliageK, NightK);
	BuildCountryside(SolidK, FoliageK);
	BuildBoats(SolidK, FoliageK);
	BuildFar(FarK);

	SolidK.Commit(Solid, 0, true);
	Solid->SetMaterial(0, FlatMat);
	FoliageK.Commit(Foliage, 0, false);
	Foliage->SetMaterial(0, WindMat);
	FarK.Commit(Far, 0, false);
	Far->SetMaterial(0, FlatMat);
	FireK.Commit(Fire, 0, false);
	Fire->SetMaterial(0, GlowMat);
	MoonK.Commit(Fire, 1, false);
	Fire->SetMaterial(1, MoonMat);
	NightK.Commit(NightGlow, 0, false);
	NightGlow->SetMaterial(0, WindowGlowMat);

	UE_LOG(LogSimEnvironment, Log,
		TEXT("The City of the Moon stands: solid %d tris, foliage %d, far %d, fire %d, lit windows %d, %d blocks, %d night lights (%.2f s)."),
		SolidK.NumTris(), FoliageK.NumTris(), FarK.NumTris(), FireK.NumTris(), NightK.NumTris(),
		Blocks->GetInstanceCount(), NightLights.Num(), FPlatformTime::Seconds() - T0);
}

float ASimEnvironment::TerrainHeight(float X, float Y)
{
	const float D = CityDist(X, Y);
	const float N = Perlin(X / 9000.f, Y / 9000.f) * 0.6f + Perlin(X / 3100.f, Y / 3100.f) * 0.3f + Perlin(X / 900.f, Y / 900.f) * 0.1f;
	float H = 40.f + (N * 0.5f + 0.5f) * FMath::Lerp(60.f, 450.f, Smooth(0.f, 60000.f, D));

	// Dunes: the desert west beyond the fields and south beyond the river;
	// a gentler rolling steppe north.
	float Desert = FMath::Max(Smooth(62000.f, 90000.f, -X), Smooth(44000.f, 60000.f, -Y));
	Desert = FMath::Max(Desert, Smooth(40000.f, 70000.f, Y) * 0.45f);
	const float Ridge = 1.f - FMath::Abs(Perlin(X / 6500.f + 3.1f, Y / 2600.f - 1.7f));
	H += Desert * Ridge * Ridge * 1600.f;

	// Far hills on every landward horizon.
	const float Rad = FVector2D(X - 15000.f, Y - 3000.f).Size();
	H += Smooth(250000.f, 900000.f, Rad) * (Perlin(X / 60000.f, Y / 60000.f) * 0.5f + 0.5f) * 40000.f
		* (1.f - Smooth(40000.f, 120000.f, X));

	// The walled city and a 40 m apron stand on flat ground.
	H = FMath::Lerp(0.f, H, Smooth(0.f, 4000.f, D));

	// The road west out of the Moon Gate.
	if (X < 0.f)
	{
		H = FMath::Lerp(H, 15.f, 1.f - Smooth(350.f, 900.f, FMath::Abs(Y)));
	}
	// Irrigation canals across the fields.
	if (X > -68000.f && X < -2500.f)
	{
		for (float Cy : CanalYs)
		{
			H = FMath::Lerp(H, -260.f, 1.f - Smooth(260.f, 520.f, FMath::Abs(Y - Cy)));
		}
		if (FMath::Abs(Y) < 21500.f)
		{
			H = FMath::Lerp(H, -260.f, 1.f - Smooth(260.f, 520.f, FMath::Abs(X - CanalX)));
		}
	}
	// The river, south of the walls, running east to the sea.
	H = FMath::Lerp(H, -480.f, 1.f - Smooth(2200.f, 5200.f, FMath::Abs(Y - RiverY(X))));
	// The sea.
	const float Cx = CoastX(Y);
	H = FMath::Lerp(H, -900.f, Smooth(Cx, Cx + 2600.f, X));
	return H;
}

void ASimEnvironment::BuildTerrain()
{
	TArray<float> Xs, Ys;
	AxisCoords(15000.f, 1500.f, 48000.f, 1500000.f, 1.08f, Xs);
	AxisCoords(3000.f, 1500.f, 48000.f, 1500000.f, 1.08f, Ys);
	const int32 NX = Xs.Num(), NY = Ys.Num();
	TArray<float> Hs;
	Hs.SetNumUninitialized(NX * NY);
	for (int32 j = 0; j < NY; ++j)
	{
		for (int32 i = 0; i < NX; ++i)
		{
			Hs[j * NX + i] = TerrainHeight(Xs[i], Ys[j]);
		}
	}
	FSimMeshKit K;
	for (int32 j = 0; j + 1 < NY; ++j)
	{
		for (int32 i = 0; i + 1 < NX; ++i)
		{
			const FVector P00(Xs[i], Ys[j], Hs[j * NX + i]);
			const FVector P10(Xs[i + 1], Ys[j], Hs[j * NX + i + 1]);
			const FVector P01(Xs[i], Ys[j + 1], Hs[(j + 1) * NX + i]);
			const FVector P11(Xs[i + 1], Ys[j + 1], Hs[(j + 1) * NX + i + 1]);
			auto Emit = [&](const FVector& A, const FVector& B, const FVector& C, uint32 Salt)
			{
				const FVector Ctr = (A + B + C) / 3.f;
				const FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
				const float Nz = FMath::Abs(N.Z);
				K.Tri(A, B, C, TerrainColor(Ctr.X, Ctr.Y, Ctr.Z, Nz, (i * 7919u) ^ (j * 104729u) ^ Salt), FVector::UpVector);
			};
			if ((i + j) % 2 == 0)
			{
				Emit(P00, P10, P11, 1);
				Emit(P00, P11, P01, 2);
			}
			else
			{
				Emit(P00, P10, P01, 3);
				Emit(P10, P11, P01, 4);
			}
		}
	}
	K.Commit(Terrain, 0, true);
	Terrain->SetMaterial(0, FlatMat);
	UE_LOG(LogSimEnvironment, Log, TEXT("terrain: %d x %d grid, %d tris"), NX, NY, K.NumTris());
}

void ASimEnvironment::BuildWater()
{
	TArray<float> Xs, Ys;
	AxisCoords(15000.f, 700.f, 50000.f, 1500000.f, 1.1f, Xs);
	AxisCoords(3000.f, 700.f, 50000.f, 1500000.f, 1.1f, Ys);
	FSimMeshKit K;
	for (int32 j = 0; j + 1 < Ys.Num(); ++j)
	{
		for (int32 i = 0; i + 1 < Xs.Num(); ++i)
		{
			// Skip cells that are dry land everywhere (well above the water).
			const float Hmin = FMath::Min(
				FMath::Min(TerrainHeight(Xs[i], Ys[j]), TerrainHeight(Xs[i + 1], Ys[j])),
				FMath::Min(TerrainHeight(Xs[i], Ys[j + 1]), TerrainHeight(Xs[i + 1], Ys[j + 1])));
			if (Hmin > 60.f)
			{
				continue;
			}
			K.Quad(FVector(Xs[i], Ys[j], WaterZ), FVector(Xs[i + 1], Ys[j], WaterZ),
				FVector(Xs[i + 1], Ys[j + 1], WaterZ), FVector(Xs[i], Ys[j + 1], WaterZ),
				FLinearColor::White, FVector::UpVector);
		}
	}
	K.Commit(Water, 0, false);
	Water->SetMaterial(0, WaterMat);
}

void ASimEnvironment::BuildWalls(FSimMeshKit& K, FSimMeshKit& Glow)
{
	TSet<FIntPoint> Towers;
	auto Tower = [&](const FVector2D& P)
	{
		const FIntPoint Key(FMath::RoundToInt(P.X / 100.f), FMath::RoundToInt(P.Y / 100.f));
		if (Towers.Contains(Key))
		{
			return;
		}
		Towers.Add(Key);
		const float H = 1250.f;
		const float J = SimHash01(Key.X, Key.Y, 5);
		K.Block(FVector(P, 0.f), FVector2D(560.f), FVector2D(500.f), H, 0.f, Jit(MudBrick, J), MudTop);
		for (int32 s = 0; s < 4; ++s)
		{
			for (int32 m = 0; m < 4; ++m)
			{
				const float T = -375.f + 250.f * m;
				FVector2D Off;
				switch (s)
				{
				case 0: Off = FVector2D(T, -460.f); break;
				case 1: Off = FVector2D(460.f, T); break;
				case 2: Off = FVector2D(-T, 460.f); break;
				default: Off = FVector2D(-460.f, -T); break;
				}
				K.Box(FVector(P + Off, H), FVector2D(55.f), 120.f, 0.f, Jit(MudBrick, J), MudTop);
			}
		}
	};
	auto Run = [&](const FVector2D& A, const FVector2D& B, const FVector2D& Outer, bool bTowerA, bool bTowerB)
	{
		const FVector2D Dir = (B - A).GetSafeNormal();
		const float L = (B - A).Size();
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		const int32 N = FMath::Max(1, FMath::RoundToInt(L / 2000.f));
		const float Seg = L / N;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D C = A + Dir * (Seg * (i + 0.5f));
			const float J = SimHash01(FMath::RoundToInt(C.X), FMath::RoundToInt(C.Y), 11);
			K.Block(FVector(C, 0.f), FVector2D(Seg * 0.5f + 4.f, 240.f), FVector2D(Seg * 0.5f + 4.f, 200.f), WallH, Yaw,
				Jit(MudBrick, J), MudTop);
		}
		for (float S = 180.f; S < L - 100.f; S += 360.f)
		{
			const FVector2D P = A + Dir * S + Outer * 160.f;
			K.Box(FVector(P, WallH), FVector2D(90.f, 38.f), 115.f, Yaw, MudBrick, MudTop);
		}
		for (float S = 4000.f; S < L - 1500.f; S += 4000.f)
		{
			Tower(A + Dir * S + Outer * 120.f);
		}
		if (bTowerA) { Tower(A); }
		if (bTowerB) { Tower(B); }
	};
	const FVector2D W(-1.f, 0.f), E(1.f, 0.f), S(0.f, -1.f), N(0.f, 1.f);
	Run(FVector2D(CityX0, CityY0), FVector2D(CityX0, -1300.f), W, true, false);
	Run(FVector2D(CityX0, 1300.f), FVector2D(CityX0, CityY1), W, false, true);
	Run(FVector2D(CityX0, CityY0), FVector2D(CityX1, CityY0), S, true, true);
	Run(FVector2D(CityX0, CityY1), FVector2D(CityX1, CityY1), N, true, true);
	Run(FVector2D(CityX1, CityY0), FVector2D(CityX1, SeaGateY - 700.f), E, true, false);
	Run(FVector2D(CityX1, SeaGateY + 700.f), FVector2D(CityX1, CityY1), E, false, true);

	// --- the Moon Gate: two massive towers, the lintel over the passage, a
	// lapis band and the crescent (glow section). The street's own gate place
	// (the pillars and the door) stands inside the passage.
	// Front face at GateFront (outside), back face at GateBack: a short,
	// deep-shadowed passage the street opens straight into.
	constexpr float GateFront = -620.f, GateBack = 120.f;
	const float GateMid = (GateFront + GateBack) * 0.5f, GateHalf = (GateBack - GateFront) * 0.5f;
	for (float Side : { -1.f, 1.f })
	{
		K.Box(FVector(GateMid, 840.f * Side, 0.f), FVector2D(GateHalf, 540.f), 1500.f, 0.f, BakedBrick, MudTop);
		for (int32 m = 0; m < 5; ++m)
		{
			K.Box(FVector(GateFront + 40.f, 840.f * Side - 420.f + 210.f * m, 1500.f), FVector2D(40.f, 62.f), 130.f, 0.f, BakedBrick, MudTop);
			K.Box(FVector(GateBack - 40.f, 840.f * Side - 420.f + 210.f * m, 1500.f), FVector2D(40.f, 62.f), 130.f, 0.f, BakedBrick, MudTop);
		}
		// Braziers flanking the approach.
		const FVector Post(GateFront - 380.f, 560.f * Side, 0.f);
		K.Prism(Post, 26.f, 22.f, 6, 170.f, Bronze, Bronze);
		K.Prism(Post + FVector(0, 0, 170.f), 40.f, 72.f, 8, 34.f, Bronze, SimRGB(40, 30, 20));
		AddFire(Glow, Post + FVector(0, 0, 196.f), 1.f, 60.f, 2200.f, true);
	}
	K.Box(FVector(GateMid, 0.f, 760.f), FVector2D(GateHalf, 320.f), 740.f, 0.f, BakedBrick, MudTop);
	// Lapis-glazed band across the gate front, gold rims, and the lapis
	// field behind the crescent (the crescent is a glow section, Build()).
	K.Box(FVector(GateFront - 6.f, 0.f, 950.f), FVector2D(12.f, 1382.f), 110.f, 0.f, Lapis, Lapis);
	K.Box(FVector(GateFront - 10.f, 0.f, 1060.f), FVector2D(12.f, 1382.f), 18.f, 0.f, Gold, Gold);
	K.Box(FVector(GateFront - 10.f, 0.f, 932.f), FVector2D(12.f, 1382.f), 18.f, 0.f, Gold, Gold);
	K.Box(FVector(GateFront - 8.f, 0.f, 1078.f), FVector2D(12.f, 250.f), 340.f, 0.f, Lapis, Lapis);

	// --- the sea gate onto the quay.
	for (float Side : { -1.f, 1.f })
	{
		K.Block(FVector(CityX1 + 60.f, SeaGateY + 1150.f * Side, 0.f), FVector2D(540.f, 440.f), FVector2D(500.f, 400.f),
			1300.f, 0.f, MudBrick, MudTop);
	}
	K.Block(FVector(CityX1 + 60.f, SeaGateY, 700.f), FVector2D(520.f, 720.f), FVector2D(500.f, 720.f), 600.f, 0.f, MudBrick, MudTop);
}

void ASimEnvironment::BuildZiggurat(FSimMeshKit& K, FSimMeshKit& Glow)
{
	const FVector C(ZigCenter, 0.f);
	const FLinearColor T1 = SimRGB(140, 104, 78), T2 = SimRGB(156, 120, 90), T3 = SimRGB(170, 134, 100);
	const FLinearColor TopC = SimRGB(186, 158, 124), StairC = SimRGB(182, 152, 118);

	// Three battered terraces, the upper two set back from the front (-X).
	K.Block(C, FVector2D(2300.f, 3200.f), FVector2D(2150.f, 3050.f), 1100.f, 0.f, T1, TopC);
	K.Block(C + FVector(300.f, 0, 1100.f), FVector2D(1600.f, 2300.f), FVector2D(1500.f, 2200.f), 700.f, 0.f, T2, TopC);
	K.Block(C + FVector(500.f, 0, 1800.f), FVector2D(1000.f, 1500.f), FVector2D(950.f, 1450.f), 500.f, 0.f, T3, TopC);
	// Buttress ribs on the first terrace's faces (the Ur ziggurat's pilasters).
	for (float Y = -2900.f; Y <= 2900.f; Y += 580.f)
	{
		for (float Side : { -1.f, 1.f })
		{
			const float Xf = C.X + 2300.f * Side;
			K.Block(FVector(Xf, C.Y + Y, 0.f), FVector2D(70.f, 80.f), FVector2D(70.f, 80.f), 1060.f, 0.f, T1 * 0.9f, TopC,
				false, FVector2D(-150.f * Side, 0.f));
		}
	}
	for (float X = -2000.f; X <= 2000.f; X += 580.f)
	{
		for (float Side : { -1.f, 1.f })
		{
			K.Block(FVector(C.X + X, C.Y + 3200.f * Side, 0.f), FVector2D(80.f, 70.f), FVector2D(80.f, 70.f), 1060.f, 0.f,
				T1 * 0.9f, TopC, false, FVector2D(0.f, -150.f * Side));
		}
	}
	// The shrine of the moon on top: lapis walls, gold rim, a dark door.
	const FVector Sh = C + FVector(500.f, 0, 2300.f);
	K.Box(Sh, FVector2D(600.f, 820.f), 560.f, 0.f, Lapis, TopC);
	K.Box(Sh + FVector(0, 0, 560.f), FVector2D(640.f, 860.f), 40.f, 0.f, Gold, Gold);
	K.Box(Sh + FVector(-610.f, 0, 0), FVector2D(14.f, 110.f), 280.f, 0.f, SimRGB(26, 20, 16), SimRGB(26, 20, 16));

	// The triple stair: one straight up the front, two along the face.
	const float Rise = 30.f;
	const float FrontX = C.X - 2300.f;
	{
		const int32 Steps = FMath::RoundToInt(1100.f / Rise);
		const float Run = 3300.f / Steps;
		for (int32 s = 0; s < Steps; ++s)
		{
			K.Box(FVector(FrontX - 3300.f + Run * (s + 0.5f), C.Y, 0.f), FVector2D(Run * 0.5f + 1.f, 340.f), Rise * (s + 1), 0.f,
				StairC, Jit(StairC, (s % 2) ? 0.9f : 0.3f));
		}
		// Up to the second terrace.
		const float X0 = FrontX + 200.f, X1 = C.X + 300.f - 1600.f;
		const int32 Steps2 = FMath::RoundToInt(700.f / Rise);
		const float Run2 = (X1 - X0) / Steps2;
		for (int32 s = 0; s < Steps2; ++s)
		{
			K.Box(FVector(X0 + Run2 * (s + 0.5f), C.Y, 1100.f), FVector2D(Run2 * 0.5f + 1.f, 260.f), Rise * (s + 1), 0.f,
				StairC, Jit(StairC, (s % 2) ? 0.9f : 0.3f));
		}
		// The gatehouse where the three stairs meet.
		K.Box(FVector(FrontX + 60.f, C.Y - 520.f, 1100.f), FVector2D(160.f, 160.f), 420.f, 0.f, T2, TopC);
		K.Box(FVector(FrontX + 60.f, C.Y + 520.f, 1100.f), FVector2D(160.f, 160.f), 420.f, 0.f, T2, TopC);
	}
	for (float Side : { -1.f, 1.f })
	{
		const int32 Steps = FMath::RoundToInt(1100.f / Rise);
		const float Run = 2500.f / Steps;
		for (int32 s = 0; s < Steps; ++s)
		{
			const float Y = C.Y + Side * (3150.f - Run * (s + 0.5f));
			K.Box(FVector(FrontX - 230.f, Y, 0.f), FVector2D(240.f, Run * 0.5f + 1.f), Rise * (s + 1), 0.f,
				StairC, Jit(StairC, (s % 2) ? 0.9f : 0.3f));
		}
	}
	// Braziers at the shrine's corners and at its door.
	for (const FVector2D& P : { FVector2D(-900.f, -1300.f), FVector2D(-900.f, 1300.f), FVector2D(1350.f, -1300.f), FVector2D(1350.f, 1300.f) })
	{
		const FVector At = C + FVector(500.f + P.X, P.Y, 2300.f);
		K.Prism(At, 50.f, 80.f, 8, 60.f, Bronze, SimRGB(40, 30, 20));
		AddFire(Glow, At + FVector(0, 0, 60.f), 1.3f, 120.f, 4000.f, false);
	}

	// The precinct wall (temenos) with its gate facing the quarter.
	const float Px0 = C.X - 7000.f, Px1 = C.X + 6000.f, Py0 = C.Y - 7000.f, Py1 = C.Y + 7000.f;
	const FLinearColor Pl = SimRGB(206, 178, 132), PlTop = SimRGB(216, 190, 146);
	auto PWall = [&](const FVector2D& A, const FVector2D& B)
	{
		const FVector2D M = (A + B) * 0.5f;
		const FVector2D H((FMath::Abs(B.X - A.X) * 0.5f) + 60.f, (FMath::Abs(B.Y - A.Y) * 0.5f) + 60.f);
		K.Box(FVector(M, 0.f), H, 480.f, 0.f, Pl, PlTop);
	};
	PWall(FVector2D(Px0, Py0), FVector2D(Px0, C.Y - 700.f));
	PWall(FVector2D(Px0, C.Y + 700.f), FVector2D(Px0, Py1));
	PWall(FVector2D(Px0, Py0), FVector2D(Px1, Py0));
	PWall(FVector2D(Px0, Py1), FVector2D(Px1, Py1));
	PWall(FVector2D(Px1, Py0), FVector2D(Px1, Py1));
	for (float Side : { -1.f, 1.f })
	{
		K.Box(FVector(Px0, C.Y + 800.f * Side, 0.f), FVector2D(200.f, 200.f), 700.f, 0.f, Pl, Lapis);
		AddFire(Glow, FVector(Px0 - 260.f, C.Y + 800.f * Side, 260.f), 0.8f, 40.f, 1800.f, false);
		K.Prism(FVector(Px0 - 260.f, C.Y + 800.f * Side, 0.f), 18.f, 16.f, 6, 250.f, Bronze, Bronze);
	}
}

void ASimEnvironment::BuildLighthouse(FSimMeshKit& K, FSimMeshKit& Glow)
{
	// The mole from the quay out to the lighthouse, and the quay itself.
	const float QuayX = 38400.f;
	K.Block(FVector((QuayX + LighthousePos.X) * 0.5f, SeaGateY, -900.f), FVector2D((LighthousePos.X - QuayX) * 0.5f, 720.f),
		FVector2D((LighthousePos.X - QuayX) * 0.5f, 650.f), 980.f, 0.f, Stone, SimRGB(178, 162, 132));
	K.Block(FVector(QuayX - 500.f, 3000.f, -900.f), FVector2D(900.f, 18500.f), FVector2D(820.f, 18500.f), 960.f, 0.f,
		Stone, SimRGB(178, 162, 132));
	// Bollards along the quay edge.
	for (float Y = -14000.f; Y < 20000.f; Y += 1600.f)
	{
		K.Prism(FVector(QuayX + 230.f, Y, 60.f), 22.f, 18.f, 6, 70.f, SimRGB(96, 80, 60), SimRGB(96, 80, 60));
	}

	const FVector B(LighthousePos, -900.f);
	const FLinearColor WW = Whitewash, WW2 = SimRGB(206, 190, 158), WW3 = SimRGB(196, 180, 146);
	K.Block(B, FVector2D(1500.f), FVector2D(1380.f), 1000.f, 0.f, Stone, SimRGB(178, 162, 132));
	const FVector T1 = B + FVector(0, 0, 1000.f);
	K.Block(T1, FVector2D(900.f), FVector2D(740.f), 2700.f, 0.f, WW, WW2);
	// Window slits on the first tier.
	for (int32 f = 0; f < 4; ++f)
	{
		const float Yaw = 90.f * f;
		const FVector Dir = FRotator(0, Yaw, 0).Vector();
		for (int32 w = 0; w < 3; ++w)
		{
			const float Z = 500.f + 700.f * w;
			const float Inset = 900.f - 160.f * (Z / 2700.f);
			K.Box(T1 + Dir * (Inset + 2.f) + FVector(0, 0, Z), FVector2D(8.f, 26.f), 110.f, Yaw, SimRGB(30, 26, 22), SimRGB(30, 26, 22));
		}
	}
	const FVector T2 = T1 + FVector(0, 0, 2700.f);
	K.Block(T2, FVector2D(760.f), FVector2D(760.f), 60.f, 0.f, WW2, WW2);  // gallery
	K.Prism(T2 + FVector(0, 0, 60.f), 620.f, 560.f, 8, 1500.f, WW2, WW3, 22.5f);
	const FVector T3 = T2 + FVector(0, 0, 1560.f);
	K.Prism(T3, 600.f, 600.f, 8, 50.f, WW3, WW3, 22.5f);
	K.Prism(T3 + FVector(0, 0, 50.f), 400.f, 380.f, 12, 800.f, WW3, WW3);
	const FVector Top = T3 + FVector(0, 0, 850.f);
	// Bronze fire bowl on four legs, and the fire.
	K.Prism(Top, 420.f, 420.f, 12, 30.f, Bronze, Bronze);
	for (int32 l = 0; l < 4; ++l)
	{
		const FVector Dir = FRotator(0, 45.f + 90.f * l, 0).Vector();
		K.Prism(Top + Dir * 300.f + FVector(0, 0, 30.f), 22.f, 22.f, 5, 260.f, Bronze, Bronze);
	}
	K.Prism(Top + FVector(0, 0, 290.f), 400.f, 400.f, 12, 30.f, Bronze, Gold);
	K.Prism(Top + FVector(0, 0, 30.f), 300.f, 360.f, 10, 110.f, Bronze, SimRGB(30, 22, 14));
	AddFire(Glow, Top + FVector(0, 0, 140.f), 4.f, 30000.f, 90000.f, false);

	Beacon = NewObject<USpotLightComponent>(this, TEXT("Beacon"));
	Beacon->SetupAttachment(RootComponent);
	Beacon->SetMobility(EComponentMobility::Movable);
	Beacon->IntensityUnits = ELightUnits::Candelas;
	Beacon->SetIntensity(900000.f);
	Beacon->SetAttenuationRadius(400000.f);
	Beacon->SetOuterConeAngle(7.f);
	Beacon->SetInnerConeAngle(3.f);
	Beacon->SetLightColor(FLinearColor(1.f, 0.72f, 0.42f));
	Beacon->SetCastShadows(false);
	Beacon->SetVolumetricScatteringIntensity(8.f);
	Beacon->RegisterComponent();
	Beacon->SetWorldLocation(Top + FVector(0, 0, 300.f));
	Beacon->SetWorldRotation(FRotator(-3.f, 180.f, 0.f));
}

void ASimEnvironment::BuildCity(FSimMeshKit& Plants, FSimMeshKit& Windows)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Blocks->SetStaticMesh(Cube);
	Blocks->SetMaterial(0, FlatMat);

	const FLinearColor Walls[] = {
		SimRGB(200, 178, 146), SimRGB(186, 162, 130), SimRGB(214, 196, 166), SimRGB(172, 146, 116),
		SimRGB(226, 216, 194), SimRGB(206, 182, 146), SimRGB(180, 136, 108) };
	const float Cell = 1000.f;
	const float PxMin = ZigCenter.X - 7800.f, PxMax = ZigCenter.X + 6800.f, PyMin = ZigCenter.Y - 7800.f, PyMax = ZigCenter.Y + 7800.f;
	int32 Houses = 0;
	for (float X = CityX0 + 1500.f; X < CityX1 - 1500.f; X += Cell)
	{
		for (float Y = CityY0 + 1500.f; Y < CityY1 - 1500.f; Y += Cell)
		{
			const int32 Ix = FMath::RoundToInt(X / Cell), Iy = FMath::RoundToInt(Y / Cell);
			// Avenues: every sixth row and column stays open.
			if (Ix % 6 == 0 || Iy % 6 == 0)
			{
				continue;
			}
			if (InStreetZone(X, Y) || (X < 3200.f && FMath::Abs(Y) < 3200.f)
				|| (X > PxMin && X < PxMax && Y > PyMin && Y < PyMax))
			{
				continue;
			}
			FSimRand R((Ix * 73856093u) ^ (Iy * 19349663u));
			const float Yard = R.Next();
			if (Yard < 0.1f)
			{
				// An open yard: a palm, sometimes two.
				Plants.Palm(FVector(X + R.Range(-300.f, 300.f), Y + R.Range(-300.f, 300.f), 0.f), R.Range(650.f, 1050.f), Ix * 31 + Iy);
				continue;
			}
			const float W = R.Range(640.f, 900.f), D = R.Range(640.f, 900.f);
			const float Cx = X + R.Range(-1.f, 1.f) * (Cell - W) * 0.5f, Cy = Y + R.Range(-1.f, 1.f) * (Cell - D) * 0.5f;
			const float H = R.Range(300.f, 430.f);
			const FLinearColor Col = Jit(Walls[FMath::Min(6, static_cast<int32>(R.Next() * 7.f))], R.Next(), 0.1f);
			AddBlock(FVector(Cx, Cy, 0.f), FVector(W, D, H), 0.f, Col);
			// Parapet ring.
			const FLinearColor Par = Col * 0.92f;
			AddBlock(FVector(Cx, Cy - D * 0.5f + 10.f, H), FVector(W, 20.f, 40.f), 0.f, Par);
			AddBlock(FVector(Cx, Cy + D * 0.5f - 10.f, H), FVector(W, 20.f, 40.f), 0.f, Par);
			AddBlock(FVector(Cx - W * 0.5f + 10.f, Cy, H), FVector(20.f, D - 40.f, 40.f), 0.f, Par);
			AddBlock(FVector(Cx + W * 0.5f - 10.f, Cy, H), FVector(20.f, D - 40.f, 40.f), 0.f, Par);
			// An upper room on some roofs.
			if (R.Next() < 0.3f)
			{
				const float Uw = W * R.Range(0.35f, 0.55f), Ud = D * R.Range(0.35f, 0.55f);
				AddBlock(FVector(Cx + (W - Uw) * 0.5f * R.Range(-1.f, 1.f), Cy + (D - Ud) * 0.5f * R.Range(-1.f, 1.f), H),
					FVector(Uw, Ud, R.Range(220.f, 280.f)), 0.f, Col * 1.03f);
			}
			// A reed sunshade on posts.
			else if (R.Next() < 0.25f)
			{
				const FVector Sc(Cx + R.Range(-100.f, 100.f), Cy + R.Range(-100.f, 100.f), H + 190.f);
				AddBlock(Sc, FVector(260.f, 220.f, 8.f), R.Range(0.f, 90.f), SimRGB(186, 158, 92));
				AddBlock(Sc + FVector(-110.f, -90.f, -190.f), FVector(10.f, 10.f, 190.f), 0.f, SimRGB(92, 64, 40));
				AddBlock(Sc + FVector(110.f, 90.f, -190.f), FVector(10.f, 10.f, 190.f), 0.f, SimRGB(92, 64, 40));
			}
			// A dark doorway on one side, and a lamp-lit window at night.
			const int32 Face = static_cast<int32>(R.Next() * 4.f) % 4;
			const FVector2D Dirs[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
			const FVector2D Fd = Dirs[Face];
			const float Ext = (Fd.X != 0.f) ? W * 0.5f : D * 0.5f;
			const FVector DoorAt(Cx + Fd.X * (Ext + 1.f), Cy + Fd.Y * (Ext + 1.f), 0.f);
			AddBlock(DoorAt, (Fd.X != 0.f) ? FVector(6.f, 95.f, 195.f) : FVector(95.f, 6.f, 195.f), 0.f, SimRGB(34, 26, 20));
			if (R.Next() < 0.22f)
			{
				const FVector Side = (Fd.X != 0.f) ? FVector(0, 1, 0) : FVector(1, 0, 0);
				const FVector Wc = DoorAt + FVector(Fd.X * 2.f, Fd.Y * 2.f, 0.f) + Side * (Ext * 0.5f) + FVector(0, 0, 175.f);
				const FVector Hx = Side * 22.f, Hz(0, 0, 28.f);
				Windows.Quad(Wc - Hx - Hz, Wc + Hx - Hz, Wc + Hx + Hz, Wc - Hx + Hz, FLinearColor::White, FVector(Fd, 0.f));
			}
			++Houses;
		}
	}
	// Palms along the precinct avenue and on the quay.
	for (float X = 3500.f; X < ZigCenter.X - 7400.f; X += 1400.f)
	{
		for (float Side : { -1.f, 1.f })
		{
			Plants.Palm(FVector(X, ZigCenter.Y + 900.f * Side, 0.f), 900.f + 150.f * SimHash01(FMath::RoundToInt(X), FMath::RoundToInt(Side) + 2), FMath::RoundToInt(X) + (Side > 0 ? 1 : 0));
		}
	}
	for (float Y = -12000.f; Y < 20000.f; Y += 2600.f)
	{
		if (FMath::Abs(Y - SeaGateY) > 1500.f)
		{
			Plants.Palm(FVector(37200.f + 300.f * SimHash01(FMath::RoundToInt(Y), 4), Y, 0.f), 1000.f, FMath::RoundToInt(Y) + 77);
		}
	}
	UE_LOG(LogSimEnvironment, Log, TEXT("city: %d houses"), Houses);
}

void ASimEnvironment::BuildCountryside(FSimMeshKit& K, FSimMeshKit& Plants)
{
	int32 Palms = 0, ReedClumps = 0, Tufts = 0, Rocks = 0;
	// The road west: palms either side, a bridge over the cross canal.
	for (float X = -2600.f; X > -90000.f; X -= 1800.f)
	{
		for (float Side : { -1.f, 1.f })
		{
			if (SimHash01(FMath::RoundToInt(X), FMath::RoundToInt(Side) + 5) < 0.7f && FMath::Abs(X - CanalX) > 900.f)
			{
				const float Y = Side * (800.f + 250.f * SimHash01(FMath::RoundToInt(X), 9));
				Plants.Palm(FVector(X, Y, TerrainHeight(X, Y) - 10.f), 850.f + 350.f * SimHash01(FMath::RoundToInt(X), FMath::RoundToInt(Side) + 7), FMath::RoundToInt(-X) * 3 + (Side > 0));
				++Palms;
			}
		}
	}
	K.Box(FVector(CanalX, 0.f, -40.f), FVector2D(700.f, 520.f), 70.f, 0.f, SimRGB(150, 114, 80), SimRGB(166, 132, 94));
	for (float Side : { -1.f, 1.f })
	{
		K.Box(FVector(CanalX, 470.f * Side, 30.f), FVector2D(700.f, 30.f), 60.f, 0.f, SimRGB(140, 104, 72), SimRGB(160, 124, 88));
	}

	// Orchards and crops in the fields; palms and reeds along the canals.
	for (float X = -68000.f; X < -2500.f; X += 6000.f)
	{
		for (float Y = -28000.f; Y < 28000.f; Y += 2600.f)
		{
			const int32 Px = FMath::FloorToInt((X + 100000.f) / 6000.f), Py = FMath::FloorToInt((Y + 100000.f) / 2600.f);
			const float Cx = Px * 6000.f - 100000.f, Cy = Py * 2600.f - 100000.f;
			const float Crop = SimHash01(Px, Py, 17);
			FSimRand R(Px * 9176u + Py * 131u);
			if (SimHash01(Px, Py, 23) < 0.16f)
			{
				for (float Ox = 500.f; Ox < 6000.f; Ox += 1000.f)
				{
					for (float Oy = 450.f; Oy < 2600.f; Oy += 900.f)
					{
						const float Pxw = Cx + Ox + R.Range(-150.f, 150.f), Pyw = Cy + Oy + R.Range(-150.f, 150.f);
						if (FMath::Abs(Pyw) < 1100.f || TerrainHeight(Pxw, Pyw) < 10.f)
						{
							continue;
						}
						Plants.Palm(FVector(Pxw, Pyw, TerrainHeight(Pxw, Pyw) - 10.f), R.Range(700.f, 1200.f), Px * 1000 + Py * 7 + FMath::RoundToInt(Ox + Oy));
						++Palms;
					}
				}
			}
			else if (Crop < 0.52f && FVector2D(Cx, Cy).Size() < 32000.f)
			{
				const FLinearColor TuftC = Crop < 0.3f ? SimRGB(96, 128, 44) : SimRGB(198, 172, 88);
				for (float Ox = 150.f; Ox < 6000.f; Ox += 330.f)
				{
					for (float Oy = 150.f; Oy < 2600.f; Oy += 330.f)
					{
						const float Pxw = Cx + Ox + R.Range(-120.f, 120.f), Pyw = Cy + Oy + R.Range(-120.f, 120.f);
						const float H = TerrainHeight(Pxw, Pyw);
						if (H < 10.f || FMath::Abs(Pyw) < 1000.f || Tufts > 16000)
						{
							continue;
						}
						Plants.Tuft(FVector(Pxw, Pyw, H - 3.f), R.Range(45.f, 75.f), 4, Px * 1000003u + Py * 7919u + static_cast<uint32>(Ox * 3.f + Oy), TuftC);
						++Tufts;
					}
				}
			}
		}
	}
	for (float Cy : CanalYs)
	{
		for (float X = -67500.f; X < -3000.f; X += 520.f)
		{
			for (float Side : { -1.f, 1.f })
			{
				const float H = SimHash01(FMath::RoundToInt(X), FMath::RoundToInt(Cy), FMath::RoundToInt(Side) + 3);
				const FVector P(X + 200.f * H, Cy + Side * (560.f + 90.f * H), 0.f);
				if (H < 0.55f)
				{
					Plants.Reeds(FVector(P.X, P.Y, TerrainHeight(P.X, P.Y) - 5.f), 170.f, 6, FMath::RoundToInt(X) * 13 + (Side > 0), SimRGB(118, 128, 56));
					++ReedClumps;
				}
				else if (H > 0.93f)
				{
					Plants.Palm(FVector(P.X, P.Y + Side * 250.f, TerrainHeight(P.X, P.Y + Side * 250.f) - 10.f), 950.f, FMath::RoundToInt(X) + 555);
					++Palms;
				}
			}
		}
	}
	// The river's reed banks and palms.
	for (float X = -150000.f; X < 38000.f; X += 260.f)
	{
		for (float Side : { -1.f, 1.f })
		{
			const float H = SimHash01(FMath::RoundToInt(X), FMath::RoundToInt(Side) + 40);
			const float Off = 2300.f + 900.f * SimHash01(FMath::RoundToInt(X), FMath::RoundToInt(Side) + 41);
			const FVector P(X, RiverY(X) + Side * Off, 0.f);
			const float Z = TerrainHeight(P.X, P.Y);
			if (Z < -200.f || Z > 200.f)
			{
				continue;
			}
			if (H < 0.62f)
			{
				Plants.Reeds(FVector(P.X, P.Y, Z - 5.f), 220.f, 8, FMath::RoundToInt(X) * 7 + (Side > 0), SimRGB(110, 124, 52));
				++ReedClumps;
			}
			else if (H > 0.95f)
			{
				const float Py = RiverY(X) + Side * (Off + 1200.f);
				Plants.Palm(FVector(X, Py, TerrainHeight(X, Py) - 10.f), 1000.f, FMath::RoundToInt(X) + 999);
				++Palms;
			}
		}
	}
	// Rocks in the desert and the steppe.
	FSimRand R(4242);
	for (int32 i = 0; i < 1400 && Rocks < 520; ++i)
	{
		const float X = R.Range(-160000.f, 60000.f), Y = R.Range(-150000.f, 160000.f);
		if (CityDist(X, Y) < 6000.f || InFarmland(X, Y) || FMath::Abs(Y - RiverY(X)) < 7000.f || X > CoastX(Y) - 2500.f)
		{
			continue;
		}
		const float Rad = FMath::Lerp(40.f, 420.f, FMath::Pow(R.Next(), 3.f));
		K.Rock(FVector(X, Y, TerrainHeight(X, Y) + Rad * 0.15f), Rad, R.Range(0.45f, 0.8f), i + 17,
			Jit(SimRGB(168, 138, 104), R.Next(), 0.15f));
		++Rocks;
	}
	UE_LOG(LogSimEnvironment, Log, TEXT("countryside: %d palms, %d reed clumps, %d tufts, %d rocks"), Palms, ReedClumps, Tufts, Rocks);
}

void ASimEnvironment::BuildBoats(FSimMeshKit& K, FSimMeshKit& Sails)
{
	FSimRand R(777);
	const FLinearColor Hull = SimRGB(104, 74, 46), HullTop = SimRGB(132, 98, 62);
	auto Boat = [&](const FVector& At, float Yaw, float Len, bool bSail, bool bReed)
	{
		const FLinearColor H = bReed ? SimRGB(176, 150, 86) : Hull;
		const FLinearColor Ht = bReed ? SimRGB(192, 166, 98) : HullTop;
		K.Block(At + FVector(0, 0, -80.f), FVector2D(Len * 0.42f, Len * 0.08f), FVector2D(Len * 0.5f, Len * 0.12f), 140.f, Yaw, H, Ht);
		const FVector Fwd = FRotator(0, Yaw, 0).Vector();
		for (float S : { -1.f, 1.f })
		{
			K.Block(At + Fwd * (Len * 0.47f * S) + FVector(0, 0, 50.f), FVector2D(Len * 0.05f, Len * 0.05f),
				FVector2D(Len * 0.02f, Len * 0.03f), bReed ? 170.f : 110.f, Yaw, H, Ht, false, FVector2D(Len * 0.05f * S, 0.f));
		}
		if (bSail)
		{
			const FVector Mast = At + FVector(0, 0, 60.f);
			K.Prism(Mast, 12.f, 9.f, 5, Len * 0.9f, SimRGB(86, 62, 40), SimRGB(86, 62, 40));
			const FVector Side = FRotator(0, Yaw + 90.f, 0).Vector();
			FLinearColor Cloth = SimRGB(214, 198, 160, 0.15f);
			FLinearColor ClothTip = SimRGB(214, 198, 160, 0.35f);
			const FVector A = Mast + FVector(0, 0, Len * 0.85f) - Fwd * Len * 0.28f;
			const FVector Bp = Mast + FVector(0, 0, Len * 0.85f) + Fwd * Len * 0.28f;
			const FVector C = Mast + FVector(0, 0, Len * 0.25f) + Fwd * Len * 0.3f + Side * 40.f;
			const FVector D = Mast + FVector(0, 0, Len * 0.25f) - Fwd * Len * 0.3f + Side * 40.f;
			Sails.Tri3(A, Bp, C, Cloth, Cloth, ClothTip, Side);
			Sails.Tri3(A, C, D, Cloth, ClothTip, ClothTip, Side);
		}
	};
	// Merchant ships along the quay and at anchor in the harbour.
	for (int32 i = 0; i < 9; ++i)
	{
		const float Y = -11000.f + 3300.f * i;
		if (FMath::Abs(Y - SeaGateY) < 1600.f)
		{
			continue;
		}
		const bool bAnchored = i % 3 == 2;
		const FVector At(bAnchored ? 43000.f + R.Range(-1500.f, 3000.f) : 39400.f, Y + R.Range(-400.f, 400.f), WaterZ);
		Boat(At, bAnchored ? R.Range(0.f, 360.f) : 90.f + R.Range(-6.f, 6.f), R.Range(1400.f, 2200.f), true, false);
	}
	// Reed boats on the river.
	for (int32 i = 0; i < 7; ++i)
	{
		const float X = -60000.f + 12000.f * i + R.Range(-3000.f, 3000.f);
		Boat(FVector(X, RiverY(X) + R.Range(-900.f, 900.f), WaterZ), R.Range(-10.f, 10.f), R.Range(500.f, 800.f), false, true);
	}
}

void ASimEnvironment::BuildFar(FSimMeshKit& K)
{
	// The mountains of the north (world-bible §8: "rivalling the Himalayas"):
	// far, huge, snow-capped silhouettes in the haze.
	FSimRand R(9001);
	for (int32 i = 0; i < 16; ++i)
	{
		const float X = -2400000.f + 4800000.f * (i + R.Range(0.1f, 0.9f)) / 16.f;
		const float Y = R.Range(1400000.f, 2200000.f);
		const float H = R.Range(160000.f, 380000.f);
		K.Mountain(FVector(X, Y, -30000.f), H * R.Range(1.6f, 2.4f), H, 9 + (i % 4), 300 + i,
			SimRGB(84, 86, 94), SimRGB(206, 210, 220), R.Range(0.66f, 0.78f));
	}
	// Desert mesas west and south.
	for (int32 i = 0; i < 14; ++i)
	{
		const float A = FMath::DegreesToRadians(R.Range(150.f, 290.f));
		const float Dist = R.Range(250000.f, 700000.f);
		const FVector At(15000.f + FMath::Cos(A) * Dist, 3000.f + FMath::Sin(A) * Dist, -5000.f);
		const float Hw = R.Range(8000.f, 40000.f);
		K.Block(At, FVector2D(Hw, Hw * R.Range(0.4f, 1.f)), FVector2D(Hw * 0.8f, Hw * R.Range(0.3f, 0.8f)),
			R.Range(12000.f, 45000.f), R.Range(0.f, 90.f), SimRGB(184, 128, 88), SimRGB(200, 150, 104));
	}
}

void ASimEnvironment::AddBlock(const FVector& BottomCenter, const FVector& Size, float YawDeg, const FLinearColor& Color)
{
	const FTransform T(FRotator(0.f, YawDeg, 0.f), BottomCenter + FVector(0, 0, Size.Z * 0.5f), Size / 100.f);
	const int32 I = Blocks->AddInstance(T, true);
	Blocks->SetCustomDataValue(I, 0, Color.R, false);
	Blocks->SetCustomDataValue(I, 1, Color.G, false);
	Blocks->SetCustomDataValue(I, 2, Color.B, false);
}

UPointLightComponent* ASimEnvironment::AddNightLight(const FVector& At, float Candela, float Radius, const FLinearColor& Color, bool bShadows)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetupAttachment(RootComponent);
	L->SetMobility(EComponentMobility::Movable);
	L->IntensityUnits = ELightUnits::Candelas;
	L->SetIntensity(Candela);
	L->SetAttenuationRadius(Radius);
	L->SetLightColor(Color);
	L->SetCastShadows(bShadows);
	L->SetSourceRadius(12.f);
	L->SetVolumetricScatteringIntensity(2.f);
	L->RegisterComponent();
	L->SetWorldLocation(At);
	NightLights.Add(L);
	NightLightBase.Add(Candela);
	return L;
}

void ASimEnvironment::AddFire(FSimMeshKit& Glow, const FVector& At, float Size, float Candela, float Radius, bool bShadows)
{
	FSimRand R(FMath::RoundToInt(At.X * 3.f + At.Y * 7.f + At.Z));
	Glow.Prism(At, 34.f * Size, 0.f, 6, 90.f * Size, FLinearColor::White, FLinearColor::White, R.Range(0.f, 60.f));
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector O(R.Range(-18.f, 18.f) * Size, R.Range(-18.f, 18.f) * Size, 0.f);
		Glow.Prism(At + O, 16.f * Size, 0.f, 5, R.Range(55.f, 120.f) * Size, FLinearColor::White, FLinearColor::White, R.Range(0.f, 70.f));
	}
	AddNightLight(At + FVector(0, 0, 70.f * Size), Candela, Radius, FLinearColor(1.f, 0.55f, 0.24f), bShadows);
}

void ASimEnvironment::BuildStreetDressing()
{
	TArray<FSimDoorSlotInfo> Slots;
	ASimStreetBuilder::GetAllDoorSlots(Slots);
	FSimMeshKit Posts, Flames;
	int32 N = 0;
	for (const FSimDoorSlotInfo& S : Slots)
	{
		if (S.Kind == TEXT("gate"))
		{
			continue;
		}
		const FVector Out = FRotator(0.f, S.OutwardYawDeg, 0.f).Vector();
		const FVector Side(-Out.Y, Out.X, 0.f);
		const FVector Base = S.Location - Out * 30.f + Side * 110.f;
		Posts.Prism(Base, 9.f, 7.f, 5, 185.f, SimRGB(84, 60, 40), SimRGB(84, 60, 40));
		Posts.Prism(Base + FVector(0, 0, 185.f), 14.f, 20.f, 6, 12.f, Bronze, SimRGB(40, 30, 20));
		Flames.Prism(Base + FVector(0, 0, 197.f), 10.f, 0.f, 5, 26.f, FLinearColor::White, FLinearColor::White);
		AddNightLight(Base + FVector(0, 0, 230.f), 14.f, 1100.f, FLinearColor(1.f, 0.58f, 0.28f), false);
		++N;
	}
	// Lamp posts and flames get their own mesh actors' sections on this actor.
	UProceduralMeshComponent* PostMesh = NewObject<UProceduralMeshComponent>(this, TEXT("StreetLampPosts"));
	PostMesh->SetupAttachment(RootComponent);
	PostMesh->RegisterComponent();
	Posts.Commit(PostMesh, 0, false);
	PostMesh->SetMaterial(0, FlatMat);
	Flames.Commit(NightGlow, 1, false);
	NightGlow->SetMaterial(1, GlowMat);
	UE_LOG(LogSimEnvironment, Log, TEXT("street: %d door lamps"), N);
}

void ASimEnvironment::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	const float Hour = USimWorldSubsystem::GetSimHourFor(this);
	if (Hour < 0.f)
	{
		return;
	}
	// Lamps are lit at dusk (schedules.csv evening_meal_and_lamps) and put out at dawn.
	const bool bNight = Hour >= 17.6f || Hour < 6.3f;
	if (bNight != bNightShown)
	{
		bNightShown = bNight;
		NightGlow->SetVisibility(bNight);
		for (UPointLightComponent* L : NightLights)
		{
			if (L != nullptr)
			{
				L->SetVisibility(bNight);
			}
		}
		if (Beacon != nullptr)
		{
			Beacon->SetVisibility(bNight);
		}
	}
	if (bNight)
	{
		for (int32 i = 0; i < NightLights.Num(); ++i)
		{
			if (NightLights[i] != nullptr)
			{
				const float F = 0.84f + 0.1f * FMath::Sin(Clock * 11.f + i * 1.7f) + 0.06f * FMath::Sin(Clock * 23.f + i * 0.9f);
				NightLights[i]->SetIntensity(NightLightBase[i] * F);
			}
		}
		if (Beacon != nullptr)
		{
			Beacon->SetWorldRotation(FRotator(-3.f, FMath::Fmod(Clock * 24.f, 360.f), 0.f));
		}
	}
}
