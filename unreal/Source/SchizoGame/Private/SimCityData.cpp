// SimCityData.cpp — AA4/AA5: see the header.
#include "SimCityData.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimCityData, Log, All);

namespace
{
	struct FCache
	{
		bool bLoaded = false;
		TArray<FSimCityPlace> Places;
		TMap<FName, int32> Index;
		FSimCityFrame Frame;
	};

	FCache& Cache()
	{
		static FCache C;
		return C;
	}

	/** Rows of a staged canon table as header -> value maps. */
	TArray<TMap<FString, FString>> ReadTable(const TCHAR* Table)
	{
		TArray<TMap<FString, FString>> Rows;
		TArray<FString> Lines;
		const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Sim/canon"), FString(Table) + TEXT(".csv"));
		if (!FFileHelper::LoadFileToStringArray(Lines, *Path) || Lines.Num() == 0)
		{
			UE_LOG(LogSimCityData, Error, TEXT("%s missing at %s — run tools/stage_canon_for_ue.py."), Table, *Path);
			return Rows;
		}
		const TArray<FString> Head = SimCityData::SplitCsv(Lines[0]);
		for (int32 i = 1; i < Lines.Num(); ++i)
		{
			if (Lines[i].IsEmpty())
			{
				continue;
			}
			const TArray<FString> F = SimCityData::SplitCsv(Lines[i]);
			TMap<FString, FString>& Row = Rows.AddDefaulted_GetRef();
			for (int32 c = 0; c < Head.Num() && c < F.Num(); ++c)
			{
				Row.Add(Head[c], F[c]);
			}
		}
		return Rows;
	}

	void Load()
	{
		FCache& C = Cache();
		if (C.bLoaded)
		{
			return;
		}
		C.bLoaded = true;
		for (const TMap<FString, FString>& R : ReadTable(TEXT("city_districts")))
		{
			if (R.FindRef(TEXT("id")) == TEXT("crescent_frame"))
			{
				C.Frame.Center = FVector2D(FCString::Atod(*R.FindRef(TEXT("cx_m"))), FCString::Atod(*R.FindRef(TEXT("cy_m")))) * 100.0;
				C.Frame.LagoonR = FCString::Atof(*R.FindRef(TEXT("r0_m"))) * 100.f;
				C.Frame.WallR = FCString::Atof(*R.FindRef(TEXT("r1_m"))) * 100.f;
				C.Frame.HornWestDeg = FCString::Atof(*R.FindRef(TEXT("a0_deg")));
				C.Frame.HornEastDeg = FCString::Atof(*R.FindRef(TEXT("a1_deg")));
			}
		}
		for (const TMap<FString, FString>& R : ReadTable(TEXT("places")))
		{
			if (R.FindRef(TEXT("city")) != TEXT("city_of_the_moon") || R.FindRef(TEXT("x_m")).IsEmpty())
			{
				continue;
			}
			FSimCityPlace& P = C.Places.AddDefaulted_GetRef();
			P.Id = FName(*R.FindRef(TEXT("id")));
			P.Kind = R.FindRef(TEXT("kind"));
			P.Name = R.FindRef(TEXT("name"));
			P.OwnerPersonId = R.FindRef(TEXT("owner_person_id"));
			P.BuildingId = R.FindRef(TEXT("building_id"));
			P.Quarter = R.FindRef(TEXT("quarter"));
			P.Typology = R.FindRef(TEXT("typology"));
			P.Wealth = R.FindRef(TEXT("wealth"));
			P.Center = FVector2D(FCString::Atod(*R.FindRef(TEXT("x_m"))), FCString::Atod(*R.FindRef(TEXT("y_m")))) * 100.0;
			P.YawDeg = FCString::Atof(*R.FindRef(TEXT("yaw_deg")));
			P.Size = FVector2D(FCString::Atod(*R.FindRef(TEXT("w_m"))), FCString::Atod(*R.FindRef(TEXT("d_m")))) * 100.0;
			C.Index.Add(P.Id, C.Places.Num() - 1);
		}
		UE_LOG(LogSimCityData, Log, TEXT("The crescent: %d places, centre (%.0f, %.0f) cm, lagoon %.0f, wall %.0f."),
			C.Places.Num(), C.Frame.Center.X, C.Frame.Center.Y, C.Frame.LagoonR, C.Frame.WallR);
	}
}

namespace SimCityData
{
	const TArray<FSimCityPlace>& Places()
	{
		Load();
		return Cache().Places;
	}

	const FSimCityFrame& Frame()
	{
		Load();
		return Cache().Frame;
	}

	const FSimCityPlace* Find(FName Id)
	{
		Load();
		const int32* I = Cache().Index.Find(Id);
		return I ? &Cache().Places[*I] : nullptr;
	}

	TArray<FString> SplitCsv(const FString& Line)
	{
		TArray<FString> Out;
		FString Cur;
		bool bQuoted = false;
		for (int32 i = 0; i < Line.Len(); ++i)
		{
			const TCHAR Ch = Line[i];
			if (Ch == TEXT('"'))
			{
				if (bQuoted && i + 1 < Line.Len() && Line[i + 1] == TEXT('"'))
				{
					Cur.AppendChar(TEXT('"'));
					++i;
				}
				else
				{
					bQuoted = !bQuoted;
				}
			}
			else if (Ch == TEXT(',') && !bQuoted)
			{
				Out.Add(Cur);
				Cur.Reset();
			}
			else
			{
				Cur.AppendChar(Ch);
			}
		}
		Out.Add(Cur);
		return Out;
	}
}
