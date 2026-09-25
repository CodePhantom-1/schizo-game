// SimMeshKit.h — a tiny low-poly modelling kit for code-built scenery (D-023:
// low-poly, flat-shaded, vertex-coloured — the Valheim look without assets).
//
// Every triangle gets its own three vertices, so each face is flat-shaded
// with a single colour: the faceted look is the style, not a shortcut.
// Faces are oriented by an outward hint (the shape's centre or "up"), so
// callers never think about winding. Vertex-colour alpha carries the wind
// weight for M_FlatWind (0 = rooted, 1 = free tip); M_Flat ignores it.
#pragma once

#include "CoreMinimal.h"

class UProceduralMeshComponent;

/** sRGB 0..255 -> the linear colour the vertex stream stores (M_Flat reads it raw). */
inline FLinearColor SimRGB(uint8 R, uint8 G, uint8 B, float Alpha = 0.f)
{
	FLinearColor C = FLinearColor::FromSRGBColor(FColor(R, G, B));
	C.A = Alpha;
	return C;
}

struct SCHIZOGAME_API FSimMeshKit
{
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FLinearColor> Colors;

	/** One flat triangle, flipped if needed so its normal agrees with OutHint. */
	void Tri(const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Color, const FVector& OutHint);
	/** Per-vertex colours (e.g. wind alpha rising to a tip); normal still flat. */
	void Tri3(const FVector& A, const FVector& B, const FVector& C,
		const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC, const FVector& OutHint);
	/** A-B-C-D in loop order (either direction). */
	void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Color, const FVector& OutHint);

	/** A battered block: rectangle BaseHalf at BaseCenter (bottom), TopHalf at
	 *  BaseCenter.Z + Height, both rotated by Yaw. TopOffset shifts the top
	 *  rectangle (a lean / setback). Bottom face only when bBottom. */
	void Block(const FVector& BaseCenter, const FVector2D& BaseHalf, const FVector2D& TopHalf, float Height,
		float YawDeg, const FLinearColor& Side, const FLinearColor& Top, bool bBottom = false,
		const FVector2D& TopOffset = FVector2D::ZeroVector);
	/** Straight box: bottom centre, half extents, height. */
	void Box(const FVector& BaseCenter, const FVector2D& Half, float Height, float YawDeg,
		const FLinearColor& Side, const FLinearColor& Top)
	{
		Block(BaseCenter, Half, Half, Height, YawDeg, Side, Top);
	}
	/** An N-sided prism / frustum / cone (RTop = 0). */
	void Prism(const FVector& BaseCenter, float RBase, float RTop, int32 Sides, float Height,
		const FLinearColor& Side, const FLinearColor& Top, float YawDeg = 0.f, bool bBottom = false);
	/** A jittered low-poly boulder (a squashed, noisy octagonal bipyramid-ish lump). */
	void Rock(const FVector& Center, float Radius, float Squash, uint32 Seed, const FLinearColor& Color);
	/** A far mountain: jittered rings climbing to a peak, snow above SnowFrom (0..1 of height). */
	void Mountain(const FVector& BaseCenter, float Radius, float Height, int32 Sides, uint32 Seed,
		const FLinearColor& Rock, const FLinearColor& Snow, float SnowFrom);
	/** A date palm: leaning, segmented trunk and a crown of drooping fronds (wind alpha set). */
	void Palm(const FVector& Base, float Height, uint32 Seed);
	/** A clump of reed blades (wind alpha set). */
	void Reeds(const FVector& Base, float Height, int32 Blades, uint32 Seed, const FLinearColor& Color);
	/** A grass / barley tuft (wind alpha set). */
	void Tuft(const FVector& Base, float Height, int32 Blades, uint32 Seed, const FLinearColor& Color);

	int32 NumTris() const { return Tris.Num() / 3; }
	bool IsEmpty() const { return Tris.Num() == 0; }

	/** Uploads into Section of Mesh (replacing it). */
	void Commit(UProceduralMeshComponent* Mesh, int32 Section, bool bCollision) const;
};

/** Deterministic 0..1 hash noise (the scenery must lay out the same every run). */
inline float SimHash01(uint32 A, uint32 B = 0, uint32 C = 0)
{
	uint32 H = A * 0x8da6b343u ^ B * 0xd8163841u ^ C * 0xcb1ab31fu;
	H ^= H >> 13;
	H *= 0x5bd1e995u;
	H ^= H >> 15;
	return static_cast<float>(H & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
}

/** A small deterministic stream of 0..1 numbers from a seed. */
struct FSimRand
{
	uint32 State;
	explicit FSimRand(uint32 Seed) : State(Seed * 747796405u + 2891336453u) {}
	float Next()
	{
		State = State * 747796405u + 2891336453u;
		uint32 W = ((State >> ((State >> 28u) + 4u)) ^ State) * 277803737u;
		W = (W >> 22u) ^ W;
		return static_cast<float>(W & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
	}
	float Range(float Lo, float Hi) { return Lo + (Hi - Lo) * Next(); }
};
