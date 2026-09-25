// SimMeshKit.cpp — see SimMeshKit.h.
#include "SimMeshKit.h"

#include "ProceduralMeshComponent.h"

namespace
{
	// UE renders the face whose (B-A)x(C-A) points AWAY from the viewer as
	// front in its left-handed space, so indices go out as A, C, B for the
	// computed normal to face outward.
	constexpr bool kEmitReversed = true;

	FVector Rot2(const FVector2D& P, float YawDeg)
	{
		const float R = FMath::DegreesToRadians(YawDeg);
		const float C = FMath::Cos(R), S = FMath::Sin(R);
		return FVector(P.X * C - P.Y * S, P.X * S + P.Y * C, 0.f);
	}
}

void FSimMeshKit::Tri3(const FVector& A, const FVector& B, const FVector& C,
	const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC, const FVector& OutHint)
{
	FVector N = FVector::CrossProduct(B - A, C - A);
	if (N.SizeSquared() < 1e-6f)
	{
		return;  // degenerate
	}
	N.Normalize();
	FVector P1 = B, P2 = C;
	FLinearColor K1 = CB, K2 = CC;
	if ((N | OutHint) < 0.f)
	{
		Swap(P1, P2);
		Swap(K1, K2);
		N = -N;
	}
	const int32 Base = Verts.Num();
	Verts.Add(A); Verts.Add(P1); Verts.Add(P2);
	Colors.Add(CA); Colors.Add(K1); Colors.Add(K2);
	Normals.Add(N); Normals.Add(N); Normals.Add(N);
	if (kEmitReversed)
	{
		Tris.Add(Base); Tris.Add(Base + 2); Tris.Add(Base + 1);
	}
	else
	{
		Tris.Add(Base); Tris.Add(Base + 1); Tris.Add(Base + 2);
	}
}

void FSimMeshKit::Tri(const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Color, const FVector& OutHint)
{
	Tri3(A, B, C, Color, Color, Color, OutHint);
}

void FSimMeshKit::Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Color, const FVector& OutHint)
{
	Tri(A, B, C, Color, OutHint);
	Tri(A, C, D, Color, OutHint);
}

void FSimMeshKit::Block(const FVector& BaseCenter, const FVector2D& BaseHalf, const FVector2D& TopHalf, float Height,
	float YawDeg, const FLinearColor& Side, const FLinearColor& Top, bool bBottom, const FVector2D& TopOffset)
{
	const FVector2D Corners[4] = { {-1, -1}, {1, -1}, {1, 1}, {-1, 1} };
	FVector Lo[4], Hi[4];
	for (int32 i = 0; i < 4; ++i)
	{
		Lo[i] = BaseCenter + Rot2(Corners[i] * BaseHalf, YawDeg);
		Hi[i] = BaseCenter + Rot2(Corners[i] * TopHalf + TopOffset, YawDeg) + FVector(0.f, 0.f, Height);
	}
	const FVector Mid = BaseCenter + FVector(Rot2(TopOffset * 0.5f, YawDeg).X, Rot2(TopOffset * 0.5f, YawDeg).Y, Height * 0.5f);
	for (int32 i = 0; i < 4; ++i)
	{
		const int32 j = (i + 1) % 4;
		const FVector FaceMid = (Lo[i] + Lo[j] + Hi[i] + Hi[j]) * 0.25f;
		Quad(Lo[i], Lo[j], Hi[j], Hi[i], Side, FaceMid - Mid);
	}
	Quad(Hi[0], Hi[1], Hi[2], Hi[3], Top, FVector::UpVector);
	if (bBottom)
	{
		Quad(Lo[0], Lo[1], Lo[2], Lo[3], Side, -FVector::UpVector);
	}
}

void FSimMeshKit::Prism(const FVector& BaseCenter, float RBase, float RTop, int32 Sides, float Height,
	const FLinearColor& Side, const FLinearColor& Top, float YawDeg, bool bBottom)
{
	Sides = FMath::Max(3, Sides);
	const FVector TopC = BaseCenter + FVector(0.f, 0.f, Height);
	const FVector Mid = BaseCenter + FVector(0.f, 0.f, Height * 0.5f);
	for (int32 i = 0; i < Sides; ++i)
	{
		const float A0 = FMath::DegreesToRadians(YawDeg) + 2.f * PI * i / Sides;
		const float A1 = FMath::DegreesToRadians(YawDeg) + 2.f * PI * (i + 1) / Sides;
		const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0.f), D1(FMath::Cos(A1), FMath::Sin(A1), 0.f);
		const FVector L0 = BaseCenter + D0 * RBase, L1 = BaseCenter + D1 * RBase;
		const FVector H0 = TopC + D0 * RTop, H1 = TopC + D1 * RTop;
		const FVector Out = (D0 + D1);
		if (RTop > 0.5f)
		{
			Quad(L0, L1, H1, H0, Side, Out);
			Tri(TopC, H0, H1, Top, FVector::UpVector);
		}
		else
		{
			Tri(L0, L1, TopC, Side, Out + FVector(0, 0, 0.3f));
		}
		if (bBottom)
		{
			Tri(BaseCenter, L0, L1, Side, -FVector::UpVector);
		}
	}
	(void)Mid;
}

void FSimMeshKit::Rock(const FVector& Center, float Radius, float Squash, uint32 Seed, const FLinearColor& Color)
{
	// Rings: bottom point, 2 rings, top point — 7 around. Jittered radii.
	FSimRand R(Seed);
	constexpr int32 N = 7;
	const float Heights[2] = { -0.25f, 0.45f };
	FVector Ring[2][N];
	for (int32 k = 0; k < 2; ++k)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const float A = 2.f * PI * (i + (k ? 0.5f : 0.f)) / N + R.Range(-0.2f, 0.2f);
			const float Rr = Radius * R.Range(0.75f, 1.15f) * (k ? 0.8f : 1.f);
			Ring[k][i] = Center + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, Heights[k] * Radius * Squash);
		}
	}
	const FVector Top = Center + FVector(R.Range(-0.2f, 0.2f) * Radius, R.Range(-0.2f, 0.2f) * Radius, Radius * Squash * R.Range(0.8f, 1.1f));
	const FVector Bot = Center - FVector(0, 0, Radius * Squash * 0.6f);
	for (int32 i = 0; i < N; ++i)
	{
		const int32 j = (i + 1) % N;
		const float Shade = R.Range(0.85f, 1.1f);
		const FLinearColor C = Color * Shade;
		Tri(Ring[0][i], Ring[0][j], Ring[1][i], C, (Ring[0][i] + Ring[0][j] + Ring[1][i]) / 3.f - Center);
		Tri(Ring[0][j], Ring[1][j], Ring[1][i], C * 0.95f, (Ring[0][j] + Ring[1][j] + Ring[1][i]) / 3.f - Center);
		Tri(Ring[1][i], Ring[1][j], Top, Color * R.Range(1.0f, 1.2f), (Ring[1][i] + Ring[1][j] + Top) / 3.f - Center);
		Tri(Ring[0][i], Ring[0][j], Bot, C * 0.7f, (Ring[0][i] + Ring[0][j] + Bot) / 3.f - Center);
	}
}

void FSimMeshKit::Mountain(const FVector& BaseCenter, float Radius, float Height, int32 Sides, uint32 Seed,
	const FLinearColor& RockC, const FLinearColor& SnowC, float SnowFrom)
{
	FSimRand R(Seed);
	constexpr int32 Levels = 4;
	TArray<TArray<FVector>> Rings;
	for (int32 L = 0; L < Levels; ++L)
	{
		const float T = static_cast<float>(L) / Levels;
		const float Rr = Radius * FMath::Pow(1.f - T, 1.25f);
		TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
		for (int32 i = 0; i < Sides; ++i)
		{
			const float A = 2.f * PI * i / Sides + (L % 2) * PI / Sides;
			const float J = L == 0 ? R.Range(0.8f, 1.2f) : R.Range(0.7f, 1.25f);
			Ring.Add(BaseCenter + FVector(FMath::Cos(A) * Rr * J, FMath::Sin(A) * Rr * J,
				Height * T + (L ? R.Range(-0.05f, 0.05f) * Height : -Height * 0.02f)));
		}
	}
	const FVector Peak = BaseCenter + FVector(R.Range(-0.1f, 0.1f) * Radius, R.Range(-0.1f, 0.1f) * Radius, Height);
	auto ColorAt = [&](const FVector& P, float Shade)
	{
		const float T = (P.Z - BaseCenter.Z) / Height;
		return (T > SnowFrom ? SnowC : RockC) * Shade;
	};
	for (int32 L = 0; L < Levels; ++L)
	{
		const TArray<FVector>& A = Rings[L];
		for (int32 i = 0; i < Sides; ++i)
		{
			const int32 j = (i + 1) % Sides;
			const float Shade = R.Range(0.88f, 1.08f);
			if (L + 1 < Levels)
			{
				const TArray<FVector>& B = Rings[L + 1];
				const FVector C1 = (A[i] + A[j] + B[i]) / 3.f;
				Tri(A[i], A[j], B[i], ColorAt(C1, Shade), C1 - BaseCenter - FVector(0, 0, Height * 0.3f));
				const FVector C2 = (A[j] + B[j] + B[i]) / 3.f;
				Tri(A[j], B[j], B[i], ColorAt(C2, Shade * 0.96f), C2 - BaseCenter - FVector(0, 0, Height * 0.3f));
			}
			else
			{
				const FVector C1 = (A[i] + A[j] + Peak) / 3.f;
				Tri(A[i], A[j], Peak, ColorAt(C1, Shade), C1 - BaseCenter - FVector(0, 0, Height * 0.3f));
			}
		}
	}
}

void FSimMeshKit::Palm(const FVector& Base, float Height, uint32 Seed)
{
	FSimRand R(Seed);
	const float LeanA = R.Range(0.f, 2.f * PI);
	const FVector LeanDir(FMath::Cos(LeanA), FMath::Sin(LeanA), 0.f);
	const float Lean = R.Range(0.05f, 0.22f) * Height;
	constexpr int32 Segs = 6;
	constexpr int32 Sides = 5;
	const FLinearColor BarkA = SimRGB(112, 86, 60), BarkB = SimRGB(90, 68, 48);
	auto TrunkAt = [&](float T)
	{
		return Base + LeanDir * Lean * T * T + FVector(0, 0, Height * T);
	};
	for (int32 s = 0; s < Segs; ++s)
	{
		const float T0 = static_cast<float>(s) / Segs, T1 = static_cast<float>(s + 1) / Segs;
		const FVector C0 = TrunkAt(T0), C1 = TrunkAt(T1);
		const float R0 = FMath::Lerp(26.f, 15.f, T0) * (0.8f + Height / 1500.f), R1 = FMath::Lerp(26.f, 15.f, T1) * (0.8f + Height / 1500.f);
		for (int32 i = 0; i < Sides; ++i)
		{
			const float A0 = 2.f * PI * i / Sides + s * 0.4f, A1 = 2.f * PI * (i + 1) / Sides + s * 0.4f;
			const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
			FLinearColor K0 = (s % 2 ? BarkA : BarkB), K1 = K0;
			K0.A = 0.12f * T0 * T0;
			K1.A = 0.12f * T1 * T1;
			Tri3(C0 + D0 * R0, C0 + D1 * R0, C1 + D1 * R1, K0, K0, K1, D0 + D1);
			Tri3(C0 + D0 * R0, C1 + D1 * R1, C1 + D0 * R1, K0, K1, K1, D0 + D1);
		}
	}
	// Crown: drooping fronds (two-sided material, so one face each).
	const FVector Top = TrunkAt(1.f);
	const int32 Fronds = 9 + static_cast<int32>(R.Next() * 4.f);
	for (int32 f = 0; f < Fronds; ++f)
	{
		const float A = 2.f * PI * f / Fronds + R.Range(-0.2f, 0.2f);
		const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
		const FVector SideV(-Dir.Y, Dir.X, 0.f);
		const float Len = Height * R.Range(0.32f, 0.45f);
		const float Rise = R.Range(0.15f, 0.55f);
		const float Dry = R.Next();  // drought: some fronds brown at the tips
		const FLinearColor Green = Dry < 0.25f ? SimRGB(140, 122, 58) : SimRGB(static_cast<uint8>(58 + R.Next() * 20), static_cast<uint8>(92 + R.Next() * 24), 34);
		const FLinearColor Tip = Dry < 0.5f ? SimRGB(156, 132, 66) : SimRGB(92, 118, 44);
		FVector Prev = Top;
		float PrevW = 8.f;
		FLinearColor PrevC = Green;
		PrevC.A = 0.35f;
		constexpr int32 FSegs = 3;
		for (int32 k = 1; k <= FSegs; ++k)
		{
			const float T = static_cast<float>(k) / FSegs;
			const FVector P = Top + Dir * Len * T + FVector(0, 0, Len * (Rise * T - 0.9f * T * T));
			const float W = (k == FSegs) ? 2.f : Len * 0.16f * (1.f - 0.4f * T);
			FLinearColor C = FMath::Lerp(Green, Tip, T);
			C.A = 0.35f + 0.65f * T;
			const FVector Up = FVector::CrossProduct(P - Prev, SideV).GetSafeNormal() * -1.f;
			const FVector Hint = FVector(0, 0, 1) + Up * 0.1f;
			Tri3(Prev - SideV * PrevW, Prev + SideV * PrevW, P + SideV * W, PrevC, PrevC, C, Hint);
			Tri3(Prev - SideV * PrevW, P + SideV * W, P - SideV * W, PrevC, C, C, Hint);
			Prev = P;
			PrevW = W;
			PrevC = C;
		}
	}
	// A few date clusters under the crown.
	for (int32 d = 0; d < 3; ++d)
	{
		const float A = R.Range(0.f, 2.f * PI);
		const FVector P = Top + FVector(FMath::Cos(A) * 25.f, FMath::Sin(A) * 25.f, -45.f);
		Rock(P, 16.f, 1.3f, Seed * 7 + d, SimRGB(176, 92, 36));
	}
}

void FSimMeshKit::Reeds(const FVector& Base, float Height, int32 Blades, uint32 Seed, const FLinearColor& Color)
{
	FSimRand R(Seed);
	for (int32 b = 0; b < Blades; ++b)
	{
		const FVector P = Base + FVector(R.Range(-45.f, 45.f), R.Range(-45.f, 45.f), 0.f);
		const float A = R.Range(0.f, 2.f * PI);
		const FVector SideV(FMath::Cos(A) * 4.f, FMath::Sin(A) * 4.f, 0.f);
		const float H = Height * R.Range(0.6f, 1.15f);
		const FVector Tip = P + FVector(R.Range(-30.f, 30.f), R.Range(-30.f, 30.f), H);
		FLinearColor Root = Color * 0.75f, TipC = FMath::Lerp(Color, SimRGB(196, 170, 104), R.Next() * 0.6f);
		Root.A = 0.f;
		TipC.A = 1.f;
		Tri3(P - SideV, P + SideV, Tip, Root, Root, TipC, FVector(SideV.Y, -SideV.X, 0.f));
		// Plumed heads on some reeds.
		if (R.Next() < 0.3f)
		{
			FLinearColor Head = SimRGB(150, 118, 78, 1.f);
			Tri3(Tip - SideV * 2.f, Tip + SideV * 2.f, Tip + FVector(0, 0, 30), Head, Head, Head, FVector(SideV.Y, -SideV.X, 0.f));
		}
	}
}

void FSimMeshKit::Tuft(const FVector& Base, float Height, int32 Blades, uint32 Seed, const FLinearColor& Color)
{
	FSimRand R(Seed);
	for (int32 b = 0; b < Blades; ++b)
	{
		const float A = R.Range(0.f, 2.f * PI);
		const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.f);
		const FVector SideV(-Out.Y * 5.f, Out.X * 5.f, 0.f);
		const FVector P = Base + Out * R.Range(0.f, 12.f);
		const FVector Tip = P + Out * R.Range(10.f, 30.f) + FVector(0, 0, Height * R.Range(0.7f, 1.2f));
		FLinearColor Root = Color * 0.7f, TipC = Color * R.Range(1.0f, 1.25f);
		Root.A = 0.f;
		TipC.A = 0.8f;
		Tri3(P - SideV, P + SideV, Tip, Root, Root, TipC, Out);
	}
}

void FSimMeshKit::Commit(UProceduralMeshComponent* Mesh, int32 Section, bool bCollision) const
{
	if (Mesh == nullptr)
	{
		return;
	}
	const TArray<FVector2D> UV0;
	const TArray<FProcMeshTangent> Tangents;
	Mesh->CreateMeshSection_LinearColor(Section, Verts, Tris, Normals, UV0, Colors, Tangents, bCollision, false);
}
