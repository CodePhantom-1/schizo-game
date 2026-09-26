// SimCityData.h — AA4/AA5 (D-025): the crescent city as the engine reads it.
// One source for the street builder, the environment and the NPC resolver:
// db/canon/places.csv (every building's footprint, from tools/city_layout.py)
// and city_districts.csv's crescent_frame row (the centre, the lagoon edge,
// the wall). Loaded once from the staged canon (Content/Sim/canon). Units are
// centimetres (the canon's metres × 100); +X east, +Y north.
#pragma once

#include "CoreMinimal.h"

struct FSimCityPlace
{
	FName Id;
	FString Kind;
	FString Name;
	FString OwnerPersonId;
	FString BuildingId;
	FString Quarter;
	FString Typology;
	FString Wealth;
	FVector2D Center = FVector2D::ZeroVector;  // cm
	float YawDeg = 0.f;                          // the width runs along this heading
	FVector2D Size = FVector2D::ZeroVector;     // cm: X width along YawDeg, Y depth
};

struct FSimCityFrame
{
	FVector2D Center = FVector2D(24000.0, 2000.0);
	float LagoonR = 10500.f;
	float WallR = 17500.f;
	float HornWestDeg = 195.f;  // the wall runs from here over the north...
	float HornEastDeg = -15.f;  // ...to here
};

namespace SimCityData
{
	SCHIZOGAME_API const TArray<FSimCityPlace>& Places();
	SCHIZOGAME_API const FSimCityFrame& Frame();
	SCHIZOGAME_API const FSimCityPlace* Find(FName Id);
	/** Splits one CSV line, honouring double quotes ("a, b" stays one field). */
	SCHIZOGAME_API TArray<FString> SplitCsv(const FString& Line);
}
