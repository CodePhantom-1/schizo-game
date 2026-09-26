// SimCompass.h — the world's compass (D-026). UE is left-handed: with +X east,
// a real compass needs north at -Y (turning right from east must face south).
// Every coordinate stays as laid out; only what names a direction reads this.
#pragma once

#include "CoreMinimal.h"

namespace SimCompass
{
	inline const FVector North(0.f, -1.f, 0.f);
	inline const FVector East(1.f, 0.f, 0.f);

	/** A sky body's direction: Phase 0 rises in the east, PI/2 culminates in the
	 *  south, TiltDeg above the horizon, PI sets in the west. */
	inline FVector SunDir(float Phase, float TiltDeg)
	{
		const float T = FMath::DegreesToRadians(TiltDeg);
		return East * FMath::Cos(Phase) - North * (FMath::Sin(Phase) * FMath::Cos(T))
			+ FVector::UpVector * (FMath::Sin(Phase) * FMath::Sin(T));
	}
}
