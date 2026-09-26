// SimRebind.h — A9: full remapping over the project's legacy action/axis
// mappings (UInputSettings). A key moves: binding it to one action removes it
// from any other, so no key ever does two things. Persisted per user
// (SaveKeyMappings); reset re-reads the project's own Config/DefaultInput.ini.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

namespace SimRebind
{
	struct FBinding
	{
		FName Name;          // action or axis name
		bool bAxis = false;
		float Scale = 1.f;   // axes: +1 / -1 (MoveForward W = +1, S = -1)
		FKey Key;            // invalid = unbound (it lost its key to another action)
	};

	/** Every action and axis direction the project defines, with its current key (sorted by name). */
	SCHIZOGAME_API TArray<FBinding> List();
	/** Binds NewKey to the action (or axis direction), replacing OldKey there and removing NewKey everywhere else. */
	SCHIZOGAME_API bool Rebind(FName Name, bool bAxis, float Scale, FKey OldKey, FKey NewKey);
	/** Every mapping back to Config/DefaultInput.ini. */
	SCHIZOGAME_API void ResetToDefaults();
}
