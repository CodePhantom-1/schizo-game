// SimEnvironment.cpp — see SimEnvironment.h. All placement here is INVENTED
// scenery (D-018 ledger: docs/proposals/invented-ledger-environment.md),
// grounded in world-bible §5 (City of the Moon ~ Ur: temple of sun and moon,
// the Great Lighthouse, sea trade) and §2 (the drought).
#include "SimEnvironment.h"
#include "Misc/PackageName.h"
#include "SimCityData.h"

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
	// --- the city: the crescent round the lagoon (AA4, D-025; cm; +X east,
	// +Y south, north is -Y (D-026: UE is left-handed); the old Moon Gate's spot is the origin). The frame and the
	// monuments' places are read from the canon (SimCityData) in Build().
	constexpr float WallH = 950.f;
	constexpr float WaterZ = -120.f;
	FVector2D CityO(24000.f, 2000.f);
	float LagoonR = 10500.f, WallR = 17500.f, HornW = 195.f, HornE = -15.f;
	constexpr float CitadelA0 = 144.f, CitadelA1 = 122.f, CitadelR = 20500.f;  // the Prophet's citadel bulges out
	FVector2D ZigCenter(24000.f, 16000.f);
	FVector2D LighthousePos(44770.f, -3560.f);
	FVector2D GatePos(6570.f, 470.f);   // the Moon Gate, in the outer wall
	float GateYaw = 5.f;                // local +X runs from outside to inside
	FVector2D SeaGatePos(41330.f, 4440.f);
	float SeaGateYaw = 188.f;

	void LoadCityFrame()
	{
		const FSimCityFrame& F = SimCityData::Frame();
		CityO = F.Center;
		LagoonR = F.LagoonR;
		WallR = F.WallR;
		HornW = F.HornWestDeg;
		HornE = F.HornEastDeg;
		if (const FSimCityPlace* Z = SimCityData::Find(TEXT("temple_front_place"))) { ZigCenter = Z->Center; }
		if (const FSimCityPlace* L = SimCityData::Find(TEXT("great_lighthouse_place"))) { LighthousePos = L->Center; }
		// A gate's place yaw runs along the wall; its passage (local +X, outside to inside) is 90° less.
		if (const FSimCityPlace* G = SimCityData::Find(TEXT("moon_gate_place"))) { GatePos = G->Center; GateYaw = G->YawDeg - 90.f; }
		if (const FSimCityPlace* S = SimCityData::Find(TEXT("sea_gate_place"))) { SeaGatePos = S->Center; SeaGateYaw = S->YawDeg - 90.f; }
	}

	/** Degrees of P round the crescent's centre (0 = east, 90 = +Y = south), in (-180, 180]. */
	float CityAngle(float X, float Y) { return FMath::RadiansToDegrees(FMath::Atan2(Y - CityO.Y, X - CityO.X)); }
	float CityRadius(float X, float Y) { return FVector2D(X - CityO.X, Y - CityO.Y).Size(); }
	/** True between the horns, going over the south (+Y) (the built crescent). */
	bool InCrescentArc(float A) { const float A360 = A < HornE ? A + 360.f : A; return A360 <= HornW; }
	/** Point on the crescent: angle (deg) and radius from the centre. */
	FVector2D CityPoint(float ADeg, float R) { const float A = FMath::DegreesToRadians(ADeg); return CityO + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R; }
	const float CanalYs[] = { -21000.f, -9000.f, 9000.f, 21000.f };
	constexpr float CanalX = -30000.f;

	float Smooth(float A, float B, float X) { return FMath::SmoothStep(A, B, X); }
	float Perlin(float X, float Y) { return FMath::PerlinNoise2D(FVector2D(X, Y)); }

	/** How far P lies outside the walls (0 inside the crescent, its lagoon and the citadel). */
	float CityDist(float X, float Y)
	{
		const float A = CityAngle(X, Y);
		const float Edge = (A <= CitadelA0 && A >= CitadelA1) ? CitadelR : WallR;
		return FMath::Max(0.f, CityRadius(X, Y) - (Edge + 300.f));
	}
	float RiverY(float X) { return -34000.f + 5000.f * FMath::Sin(X / 30000.f) + 1800.f * FMath::Sin(X / 9000.f + 1.f); }
	float CoastX(float Y) { return 42500.f + 1500.f * FMath::Sin(Y / 20000.f) + 600.f * FMath::Sin(Y / 5000.f); }
	bool InFarmland(float X, float Y) { return X > -70000.f && X < -5000.f && FMath::Abs(Y) < 29000.f; }

	/** The ring street between the lagoon-side and the wall-side rows, worn by feet. */
	bool InStreetZone(float X, float Y)
	{
		const float R = CityRadius(X, Y);
		return R > LagoonR + 2600.f && R < LagoonR + 4400.f && InCrescentArc(CityAngle(X, Y));
	}

	/** Tommy's countryside palms (BuildCountryside), recorded for the scatter to keep clear of (V-B3). */
	TArray<FVector2D> GCountryPalms;

	void PlantPalm(FSimMeshKit& Plants, const FVector& Base, float Height, uint32 Seed)
	{
		GCountryPalms.Add(FVector2D(Base));
		Plants.Palm(Base, Height, Seed);
	}

	/** The open land before any landform or mask: rolling ground, dunes, far hills (Tommy's). */
	float BaseHeight(float X, float Y)
	{
		const float D = CityDist(X, Y);
		const float N = Perlin(X / 9000.f, Y / 9000.f) * 0.6f + Perlin(X / 3100.f, Y / 3100.f) * 0.3f + Perlin(X / 900.f, Y / 900.f) * 0.1f;
		float H = 40.f + (N * 0.5f + 0.5f) * FMath::Lerp(60.f, 450.f, Smooth(0.f, 60000.f, D));

		// Dunes: the desert west beyond the fields and south (+Y, D-026) behind the
		// crescent; a gentler rolling steppe north beyond the river.
		float Desert = FMath::Max(Smooth(62000.f, 90000.f, -X), Smooth(44000.f, 60000.f, Y));
		Desert = FMath::Max(Desert, Smooth(40000.f, 70000.f, -Y) * 0.45f);
		const float Ridge = 1.f - FMath::Abs(Perlin(X / 6500.f + 3.1f, Y / 2600.f - 1.7f));
		H += Desert * Ridge * Ridge * 1600.f;

		// Far hills on every landward horizon.
		const float Rad = FVector2D(X - 15000.f, Y - 3000.f).Size();
		H += Smooth(250000.f, 900000.f, Rad) * (Perlin(X / 60000.f, Y / 60000.f) * 0.5f + 0.5f) * 40000.f
			* (1.f - Smooth(40000.f, 120000.f, X));
		return H;
	}

	// --- V-B3 landforms ---------------------------------------------------------------------------
	/** Five tells, seeded: 3-4.5 km out on the landward side (south, west, north), 250-600 m across,
	 *  8-18 m high, each 500 m + its radius clear of the road west, the river, the fields and the sea,
	 *  and of each other. Cached per city frame (the frame moves the road and the centre). */
	const TArray<FSimTell>& TellList()
	{
		static TArray<FSimTell> Out;
		static FVector2D ForO(TNumericLimits<float>::Max()), ForGate(TNumericLimits<float>::Max());
		if (ForO == CityO && ForGate == GatePos)
		{
			return Out;
		}
		ForO = CityO;
		ForGate = GatePos;
		Out.Reset();
		for (int32 k = 0; Out.Num() < 5 && k < 500; ++k)
		{
			const float A = FMath::DegreesToRadians(100.f + 160.f * SimHash01(k, 71));
			const float Dist = 300000.f + 150000.f * SimHash01(k, 72);
			const FVector2D C = CityO + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Dist;
			const float R = 25000.f + 35000.f * SimHash01(k, 73);
			const float Clear = 50000.f + R;
			const bool bNearFields = C.X > -70000.f - Clear && C.X < -5000.f + Clear && FMath::Abs(C.Y) < 29000.f + Clear;
			if ((C.X < GatePos.X + Clear && FMath::Abs(C.Y - GatePos.Y) < Clear) || FMath::Abs(C.Y - RiverY(C.X)) < Clear
				|| bNearFields || C.X > CoastX(C.Y) - Clear)
			{
				continue;
			}
			bool bNear = false;
			for (const FSimTell& T : Out)
			{
				bNear |= FVector2D::Distance(T.Center, C) < T.Radius + R + 20000.f;
			}
			if (!bNear)
			{
				float Level = 0.f;  // the mean ground on the ring round its foot: the tell's plateau
				for (int32 q = 0; q < 16; ++q)
				{
					const FVector2D P = C + FVector2D(1.3f * R, 0.f).GetRotated(22.5f * q);
					Level += BaseHeight(P.X, P.Y) / 16.f;
				}
				Out.Add({ C, R, 800.f + 1000.f * SimHash01(k, 74), Level });
			}
		}
		return Out;
	}

	/** 1 on a tell's flat top, falling to 0 at its foot (the strongest tell wins); OutTell is that tell. */
	float TellWeight(float X, float Y, const FSimTell** OutTell = nullptr)
	{
		float Best = 0.f;
		for (const FSimTell& T : TellList())
		{
			const float W = 1.f - Smooth(0.6f * T.Radius, T.Radius, FVector2D::Distance(T.Center, FVector2D(X, Y)));
			if (W > Best)
			{
				Best = W;
				if (OutTell != nullptr)
				{
					*OutTell = &T;
				}
			}
		}
		return Best;
	}

	/** 1 on the crest of a levee beside the river or a canal, 0 beyond ~45 m (it reads at the 15 m grid). */
	float LeveeWeight(float X, float Y)
	{
		auto Bank = [](float DistFromEdge) { return DistFromEdge > 0.f ? 1.f - Smooth(300.f, 4500.f, DistFromEdge) : 0.f; };
		float W = Bank(FMath::Abs(Y - RiverY(X)) - 5200.f);  // the river's banks end 52 m out
		if (X > -68000.f && X < -2500.f)
		{
			for (float Cy : CanalYs)
			{
				W = FMath::Max(W, Bank(FMath::Abs(Y - Cy) - 520.f));
			}
			if (FMath::Abs(Y) < 21500.f)
			{
				W = FMath::Max(W, Bank(FMath::Abs(X - CanalX) - 520.f));
			}
		}
		return W;
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
		else if (X < GatePos.X && FMath::Abs(Y - GatePos.Y) < 520.f)
		{
			C = SimRGB(168, 146, 116);  // the road west from the Moon Gate
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
			const float Desert = FMath::Max(Smooth(62000.f, 90000.f, -X), Smooth(44000.f, 60000.f, Y));
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

	/** V-B3: the ground kind of the same triangle — TerrainColor's regions in its order (the colour
	 *  stays TerrainColor's; this only picks M_Terrain's detail cell). */
	ESimGround GroundKindOf(float X, float Y, float H, float Nz, uint32 Seed)
	{
		if (H < -160.f)
		{
			return ESimGround::Bed;
		}
		if (H < -30.f)
		{
			return ESimGround::ReedMud;  // wet mud at the water's edge
		}
		if (CityDist(X, Y) < 1.f)
		{
			return ESimGround::Street;  // the packed earth of the crescent
		}
		if (X > CoastX(Y) - 1800.f)
		{
			return ESimGround::Beach;
		}
		if (X < GatePos.X && FMath::Abs(Y - GatePos.Y) < 520.f)
		{
			return ESimGround::Road;
		}
		if (FMath::Abs(Y - RiverY(X)) < 6800.f)
		{
			return ESimGround::ReedMud;  // reed banks
		}
		if (TellWeight(X, Y) > 0.35f)
		{
			return ESimGround::Tell;  // V-B3: the mound's top and upper slopes
		}
		if (InFarmland(X, Y))
		{
			if (Perlin(X / 2500.f + 7.3f, Y / 2500.f - 2.1f) * 0.5f + 0.5f > 0.75f)
			{
				return ESimGround::Salt;  // V-B3: salt pans in the fields (the drought spreads them in M_Terrain)
			}
			const int32 Px = FMath::FloorToInt((X + 100000.f) / 6000.f);
			const int32 Py = FMath::FloorToInt((Y + 100000.f) / 2600.f);
			const float Crop = SimHash01(Px, Py, 17);
			return Crop < 0.52f ? ESimGround::Irrigated : (Crop < 0.76f ? ESimGround::Silt : ESimGround::Cracked);
		}
		if (H > 12000.f)
		{
			return Nz < 0.86f ? ESimGround::Rock : ESimGround::Hills;
		}
		if (Nz < 0.86f)
		{
			return ESimGround::Gravel;
		}
		const float Desert = FMath::Max(Smooth(62000.f, 90000.f, -X), Smooth(44000.f, 60000.f, Y));
		return Desert > 0.6f ? ESimGround::Dune : (Desert > 0.25f ? ESimGround::Sand : ESimGround::Silt);
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
	// V-B3: the terrain's detail material (tools/art/ue_make_terrain_material.py); look first, a miss logs.
	TerrainMat = FPackageName::DoesPackageExist(TEXT("/Game/Art/Terrain/M_Terrain"))
		? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Terrain/M_Terrain.M_Terrain")) : nullptr;
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
	LoadCityFrame();
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
			// In the gate's frame (GatePos, GateYaw): local X = Xf, local Y along the wall.
			const FVector2D Po = GatePos + FVector2D(Xf, R * FMath::Cos(Th)).GetRotated(GateYaw);
			const FVector2D Pi = GatePos + FVector2D(Xf, Ri * FMath::Cos(Th)).GetRotated(GateYaw);
			O.Add(FVector(Po, Zc + R * FMath::Sin(Th)));
			I.Add(FVector(Pi, Zc + Lift + Ri * FMath::Sin(Th)));
		}
		I[0] = O[0];
		I[Steps] = O[Steps];
		for (int32 s = 0; s < Steps; ++s)
		{
			MoonK.Quad(O[s], O[s + 1], I[s + 1], I[s], FLinearColor::White, -FVector::ForwardVector.RotateAngleAxis(GateYaw, FVector::UpVector));
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

FLinearColor ASimEnvironment::SampleTerrainColor(float X, float Y, float H, float Nz, uint32 Seed)
{
	return TerrainColor(X, Y, H, Nz, Seed);
}

ESimGround ASimEnvironment::GroundKind(float X, float Y, float H, float Nz, uint32 Seed)
{
	return GroundKindOf(X, Y, H, Nz, Seed);
}

const TArray<FVector2D>& ASimEnvironment::GetCountryPalms()
{
	return GCountryPalms;
}

float ASimEnvironment::LeveeAt(float X, float Y)
{
	return LeveeWeight(X, Y);
}

TArray<FSimTell> ASimEnvironment::GetTells()
{
	return TellList();
}

float ASimEnvironment::RiverCentreY(float X)
{
	return RiverY(X);
}

void ASimEnvironment::LoadFrame()
{
	LoadCityFrame();
}

float ASimEnvironment::WaterHeight()
{
	return WaterZ;
}

float ASimEnvironment::TerrainHeight(float X, float Y)
{
	const float D = CityDist(X, Y);
	float H = BaseHeight(X, Y);

	// --- V-B3 landforms: before the apron, lagoon, road, canals, river and sea below, so their masks win.
	// Levees: a 70 cm bank beside the river and each canal (the date groves stand high and dry).
	H += 70.f * LeveeWeight(X, Y);
	// Tells: flat-topped mounds of old settlements on the landward horizon.
	{
		const FSimTell* Tell = nullptr;
		const float W = TellWeight(X, Y, &Tell);
		if (Tell != nullptr)
		{
			H = FMath::Lerp(H, Tell->Level + Tell->Height, W);  // its plateau, then its height
		}
	}
	// Erosion gullies: five shallow (80 cm) meandering runnels down the dunes south.
	{
		const float South = Smooth(44000.f, 60000.f, Y);
		if (South > 0.f)
		{
			for (int32 G = 0; G < 5; ++G)
			{
				const float Gx = -60000.f + 30000.f * G + 8000.f * SimHash01(G, 81) + 6000.f * Perlin(Y / 9000.f, G * 3.7f);
				H -= 80.f * South * (1.f - Smooth(600.f, 3000.f, FMath::Abs(X - Gx)));
			}
		}
	}

	// The walled city and a 40 m apron stand on flat ground.
	H = FMath::Lerp(0.f, H, Smooth(0.f, 4000.f, D));

	// The lagoon of the two waters inside the crescent, with a channel north to the
	// river (the fresh water) and one east to the sea (the salt) (D-025).
	{
		const float R = CityRadius(X, Y);
		H = FMath::Lerp(H, -420.f, 1.f - Smooth(LagoonR - 900.f, LagoonR - 150.f, R));
		if (Y < CityO.Y && Y > RiverY(X))
		{
			H = FMath::Lerp(H, -420.f, 1.f - Smooth(500.f, 1100.f, FMath::Abs(X - CityO.X)));
		}
		const float SeaChannelY = CityO.Y - LagoonR * 0.78f;
		if (X > CityO.X)
		{
			H = FMath::Lerp(H, -420.f, 1.f - Smooth(700.f, 1500.f, FMath::Abs(Y - SeaChannelY)));
		}
	}
	// The road west out of the Moon Gate.
	if (X < GatePos.X)
	{
		H = FMath::Lerp(H, 15.f, 1.f - Smooth(350.f, 900.f, FMath::Abs(Y - GatePos.Y)));
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
	// The river, north of the walls, running east to the sea.
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
				const uint32 Seed = (i * 7919u) ^ (j * 104729u) ^ Salt;
				FLinearColor Col = TerrainColor(Ctr.X, Ctr.Y, Ctr.Z, Nz, Seed);
				Col.A = static_cast<uint8>(GroundKindOf(Ctr.X, Ctr.Y, Ctr.Z, Nz, Seed)) / 15.f;  // M_Terrain's detail cell
				K.Tri(A, B, C, Col, FVector::UpVector);
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
	Terrain->SetMaterial(0, TerrainMat != nullptr ? TerrainMat.Get() : FlatMat.Get());
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
	// --- the crescent's walls (AA4, D-025): the outer arc from the western horn over the
	// south (+Y) to the eastern horn, bulging round the Prophet's citadel; the horns closed
	// radially down to the lagoon; the two gates stand in the outer wall.
	auto Gap = [](const FVector2D& P, const FVector2D& G) { return (P - G).Size() < 1150.f; };
	auto ArcWall = [&](float A0, float A1, float R)
	{
		const float Span = FMath::Abs(A1 - A0);
		const int32 N = FMath::Max(1, FMath::RoundToInt(FMath::DegreesToRadians(Span) * R / 1800.f));
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D A = CityPoint(FMath::Lerp(A0, A1, float(i) / N), R);
			const FVector2D B = CityPoint(FMath::Lerp(A0, A1, float(i + 1) / N), R);
			if (Gap((A + B) * 0.5f, GatePos) || Gap((A + B) * 0.5f, SeaGatePos))
			{
				continue;  // the gates stand here
			}
			const FVector2D Mid = (A + B) * 0.5f;
			const FVector2D Out = (Mid - CityO).GetSafeNormal();
			Run(A, B, Out, i % 3 == 0, false);
		}
	};
	auto RadialWall = [&](float Ang, float R0, float R1)
	{
		const FVector2D A = CityPoint(Ang, R0), B = CityPoint(Ang, R1);
		const float Side = (Ang > 90.f || Ang < -90.f) ? -1.f : 1.f;
		const FVector2D Out = FVector2D(-(B - A).Y, (B - A).X).GetSafeNormal() * Side;
		Run(A, B, Out, false, true);
	};
	ArcWall(HornW, CitadelA0, WallR);
	RadialWall(CitadelA0, WallR, CitadelR);
	ArcWall(CitadelA0, CitadelA1, CitadelR);
	RadialWall(CitadelA1, WallR, CitadelR);
	ArcWall(CitadelA1, HornE, WallR);
	RadialWall(HornW, LagoonR + 200.f, WallR);
	RadialWall(HornE, LagoonR + 200.f, WallR);

	// The lagoon's stone embankment: a low quay all round the inner curve.
	{
		const int32 N = 96;
		for (int32 i = 0; i < N; ++i)
		{
			const float A0 = FMath::Lerp(HornE, HornW, float(i) / N), A1 = FMath::Lerp(HornE, HornW, float(i + 1) / N);
			const FVector2D A = CityPoint(A0, LagoonR), B = CityPoint(A1, LagoonR);
			const FVector2D M = (A + B) * 0.5f;
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
			K.Block(FVector(M, WaterZ - 300.f), FVector2D((B - A).Size() * 0.5f + 20.f, 160.f), FVector2D((B - A).Size() * 0.5f + 20.f, 150.f),
				300.f + 60.f - WaterZ, Yaw, Stone, SimRGB(178, 162, 132));
		}
	}

	// A gate's local frame: +X from outside to inside, +Y along the wall.
	auto Frame = [](const FVector2D& Pos, float Yaw)
	{
		return [Pos, Yaw](float Lx, float Ly, float Z)
		{
			const FVector2D R = FVector2D(Lx, Ly).GetRotated(Yaw);
			return FVector(Pos + R, Z);
		};
	};

	// --- the Moon Gate: two massive towers, the lintel over the passage, a
	// lapis band and the crescent (glow section, Build()). Built in its own
	// frame so it stands wherever the canon puts it in the wall.
	{
		const auto G = Frame(GatePos, GateYaw);
		constexpr float GateFront = -620.f, GateBack = 120.f;
		const float GateMid = (GateFront + GateBack) * 0.5f, GateHalf = (GateBack - GateFront) * 0.5f;
		for (float Side : { -1.f, 1.f })
		{
			K.Box(G(GateMid, 840.f * Side, 0.f), FVector2D(GateHalf, 540.f), 1500.f, GateYaw, BakedBrick, MudTop);
			for (int32 m = 0; m < 5; ++m)
			{
				K.Box(G(GateFront + 40.f, 840.f * Side - 420.f + 210.f * m, 1500.f), FVector2D(40.f, 62.f), 130.f, GateYaw, BakedBrick, MudTop);
				K.Box(G(GateBack - 40.f, 840.f * Side - 420.f + 210.f * m, 1500.f), FVector2D(40.f, 62.f), 130.f, GateYaw, BakedBrick, MudTop);
			}
			// Braziers flanking the approach.
			const FVector Post = G(GateFront - 380.f, 560.f * Side, 0.f);
			K.Prism(Post, 26.f, 22.f, 6, 170.f, Bronze, Bronze);
			K.Prism(Post + FVector(0, 0, 170.f), 40.f, 72.f, 8, 34.f, Bronze, SimRGB(40, 30, 20));
			AddFire(Glow, Post + FVector(0, 0, 196.f), 1.f, 60.f, 2200.f, true);
		}
		K.Box(G(GateMid, 0.f, 760.f), FVector2D(GateHalf, 320.f), 740.f, GateYaw, BakedBrick, MudTop);
		// Lapis-glazed band across the gate front, gold rims, and the lapis
		// field behind the crescent.
		K.Box(G(GateFront - 6.f, 0.f, 950.f), FVector2D(12.f, 1382.f), 110.f, GateYaw, Lapis, Lapis);
		K.Box(G(GateFront - 10.f, 0.f, 1060.f), FVector2D(12.f, 1382.f), 18.f, GateYaw, Gold, Gold);
		K.Box(G(GateFront - 10.f, 0.f, 932.f), FVector2D(12.f, 1382.f), 18.f, GateYaw, Gold, Gold);
		K.Box(G(GateFront - 8.f, 0.f, 1078.f), FVector2D(12.f, 250.f), 340.f, GateYaw, Lapis, Lapis);
	}

	// --- the sea gate onto the quay and the lighthouse mole.
	{
		const auto G = Frame(SeaGatePos, SeaGateYaw);
		for (float Side : { -1.f, 1.f })
		{
			K.Block(G(0.f, 1150.f * Side, 0.f), FVector2D(540.f, 440.f), FVector2D(500.f, 400.f), 1300.f, SeaGateYaw, MudBrick, MudTop);
		}
		K.Block(G(0.f, 0.f, 700.f), FVector2D(520.f, 720.f), FVector2D(500.f, 720.f), 600.f, SeaGateYaw, MudBrick, MudTop);
	}
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

	// The precinct is the Sacred Mound quarter itself (D-025): its buildings stand round
	// the ziggurat (places.csv), so no temenos wall is drawn here.
}

void ASimEnvironment::BuildLighthouse(FSimMeshKit& K, FSimMeshKit& Glow)
{
	// The mole from the eastern horn out to the lighthouse, and the quay under the sea gate.
	{
		const FVector2D Horn = CityPoint(HornE, WallR);
		const FVector2D Dir = (LighthousePos - Horn).GetSafeNormal();
		const float L = (LighthousePos - Horn).Size();
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		K.Block(FVector((Horn + LighthousePos) * 0.5f, -900.f), FVector2D(L * 0.5f, 720.f), FVector2D(L * 0.5f, 650.f), 980.f, Yaw,
			Stone, SimRGB(178, 162, 132));
		const float QuayX = SeaGatePos.X + 700.f;
		K.Block(FVector(QuayX, SeaGatePos.Y, -900.f), FVector2D(700.f, 5200.f), FVector2D(640.f, 5200.f), 960.f, 0.f,
			Stone, SimRGB(178, 162, 132));
		for (float Y = SeaGatePos.Y - 4800.f; Y < SeaGatePos.Y + 4800.f; Y += 1600.f)
		{
			K.Prism(FVector(QuayX + 560.f, Y, 60.f), 22.f, 18.f, 6, 70.f, SimRGB(96, 80, 60), SimRGB(96, 80, 60));
		}
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
	// AA4 (D-025): the city's buildings are the crescent's places, built by
	// SimStreetBuilder from places.csv. Here: their lamp-lit windows at night,
	// on the face toward the ring street, and palms along that street.
	static const TSet<FString> NoWindows = { TEXT("market_square"), TEXT("market_stall"), TEXT("cookshop"), TEXT("scribe_booth"),
		TEXT("well_house"), TEXT("street_shrine"), TEXT("moon_pool"), TEXT("necropolis"), TEXT("brickyard"), TEXT("cattle_pen"),
		TEXT("wharf"), TEXT("fish_market"), TEXT("floating_shrine"), TEXT("field"), TEXT("pasture"), TEXT("city_gate"),
		TEXT("ziggurat"), TEXT("lighthouse"), TEXT("granary") };
	const float RowSplit = (LagoonR + WallR) * 0.5f;
	int32 Lit = 0;
	for (const FSimCityPlace& P : SimCityData::Places())
	{
		if (NoWindows.Contains(P.Typology) || P.Quarter == TEXT("beyond_the_gate"))
		{
			continue;
		}
		const uint32 H = FCrc::StrCrc32(*P.Id.ToString());
		const float Chance = P.Wealth == TEXT("poor") ? 0.35f : (P.Wealth == TEXT("modest") ? 0.6f : 0.9f);  // lamp oil costs
		if ((H % 1000) / 1000.f > Chance)
		{
			continue;
		}
		// The street-facing face: +local Y for the lagoon-side row, -local Y otherwise.
		const bool bInner = (P.Center - CityO).Size() < RowSplit;
		const FVector2D Across = FVector2D(0.f, bInner ? 1.f : -1.f).GetRotated(P.YawDeg);
		const FVector2D Along = FVector2D(1.f, 0.f).GetRotated(P.YawDeg);
		const float Off = (H % 7) / 7.f * 0.5f - 0.25f;  // not every window centred
		const FVector2D At = P.Center + Across * (P.Size.Y * 0.5f + 3.f) + Along * (P.Size.X * Off);
		const FVector Wc(At, 175.f);
		const FVector Hx = FVector(Along, 0.f) * 22.f, Hz(0, 0, 28.f);
		Windows.Quad(Wc - Hx - Hz, Wc + Hx - Hz, Wc + Hx + Hz, Wc - Hx + Hz, FLinearColor::White, FVector(Across, 0.f));
		++Lit;
	}
	// Palms along the ring street, where no building stands.
	int32 Palms = 0;
	for (float A = HornE + 3.f; A < HornW - 3.f; A += 4.5f)
	{
		const FVector2D At = CityPoint(A, LagoonR + 3500.f + 400.f * FMath::Sin(A));
		bool bClear = true;
		for (const FSimCityPlace& P : SimCityData::Places())
		{
			if ((P.Center - At).Size() < FMath::Max(P.Size.X, P.Size.Y) * 0.5f + 400.f)
			{
				bClear = false;
				break;
			}
		}
		if (bClear && (At - ZigCenter).Size() > 4500.f)
		{
			Plants.Palm(FVector(At, 0.f), 850.f + 250.f * SimHash01(FMath::RoundToInt(A * 10.f), 3), FMath::RoundToInt(A * 10.f));
			++Palms;
		}
	}
	UE_LOG(LogSimEnvironment, Log, TEXT("city: %d places lit at night, %d street palms"), Lit, Palms);
}

void ASimEnvironment::BuildCountryside(FSimMeshKit& K, FSimMeshKit& Plants)
{
	int32 Palms = 0, ReedClumps = 0, Tufts = 0, Rocks = 0;
	GCountryPalms.Reset();  // V-B3: the scatter keeps 3 m clear of every palm planted here
	// The road west: palms either side, a bridge over the cross canal.
	for (float X = -2600.f; X > -90000.f; X -= 1800.f)
	{
		for (float Side : { -1.f, 1.f })
		{
			if (SimHash01(FMath::RoundToInt(X), FMath::RoundToInt(Side) + 5) < 0.7f && FMath::Abs(X - CanalX) > 900.f)
			{
				const float Y = Side * (800.f + 250.f * SimHash01(FMath::RoundToInt(X), 9));
				PlantPalm(Plants, FVector(X, Y, TerrainHeight(X, Y) - 10.f), 850.f + 350.f * SimHash01(FMath::RoundToInt(X), FMath::RoundToInt(Side) + 7), FMath::RoundToInt(-X) * 3 + (Side > 0));
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
						PlantPalm(Plants, FVector(Pxw, Pyw, TerrainHeight(Pxw, Pyw) - 10.f), R.Range(700.f, 1200.f), Px * 1000 + Py * 7 + FMath::RoundToInt(Ox + Oy));
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
					PlantPalm(Plants, FVector(P.X, P.Y + Side * 250.f, TerrainHeight(P.X, P.Y + Side * 250.f) - 10.f), 950.f, FMath::RoundToInt(X) + 555);
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
				PlantPalm(Plants, FVector(X, Py, TerrainHeight(X, Py) - 10.f), 1000.f, FMath::RoundToInt(X) + 999);
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
		const float Y = SeaGatePos.Y - 9000.f + 2500.f * i;
		if (FMath::Abs(Y - SeaGatePos.Y) < 1600.f)
		{
			continue;
		}
		const bool bAnchored = i % 3 == 2 || FMath::Abs(Y - SeaGatePos.Y) > 5000.f;
		const FVector At(bAnchored ? SeaGatePos.X + 5500.f + R.Range(-1500.f, 3000.f) : SeaGatePos.X + 2200.f, Y + R.Range(-400.f, 400.f), WaterZ);
		Boat(At, bAnchored ? R.Range(0.f, 360.f) : 90.f + R.Range(-6.f, 6.f), R.Range(1400.f, 2200.f), true, false);
	}
	// Reed boats and fishing skiffs on the lagoon.
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = R.Range(0.f, 360.f), Rr = R.Range(1500.f, LagoonR - 1600.f);
		const FVector2D P = CityPoint(A, Rr);
		Boat(FVector(P, WaterZ), R.Range(0.f, 360.f), R.Range(500.f, 900.f), false, true);
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
		const float Y = -R.Range(1400000.f, 2200000.f);  // north is -Y (D-026)
		const float H = R.Range(160000.f, 380000.f);
		K.Mountain(FVector(X, Y, -30000.f), H * R.Range(1.6f, 2.4f), H, 9 + (i % 4), 300 + i,
			SimRGB(84, 86, 94), SimRGB(206, 210, 220), R.Range(0.66f, 0.78f));
	}
	// Desert mesas west and south.
	for (int32 i = 0; i < 14; ++i)
	{
		const float A = FMath::DegreesToRadians(R.Range(70.f, 210.f));  // west and south (+Y)
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
		// Every door keeps its flame; only the first MaxDoorLights cast real light
		// (a whole crescent of dynamic lights would sink the integrated GPU).
		constexpr int32 MaxDoorLights = 32;
		if (N < MaxDoorLights && (FCrc::StrCrc32(*S.PlaceId.ToString()) % 3) == 0)
		{
			AddNightLight(Base + FVector(0, 0, 230.f), 14.f, 1100.f, FLinearColor(1.f, 0.58f, 0.28f), false);
		}
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
