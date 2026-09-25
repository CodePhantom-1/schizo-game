// SimTablet.h — the first readable thing (Phase 4 slice).
// A clay tablet on the street: unreadable wedges until the player knows the
// script. The canon text is the zisurru gloss (notes L198, [A]).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimTablet.generated.h"

UCLASS()
class SCHIZOGAME_API ASimTablet : public AActor
{
	GENERATED_BODY()

public:
	ASimTablet();

	/** The Use verb, from the player controller's trace. */
	void Interact();

	/** The canon line the tablet carries (notes L198). */
	UPROPERTY(EditAnywhere, Category = "Sim")
	FString CanonText = TEXT("SAG.BA.SAG.BA — the zisurru gloss: the flour-line that guards a doorway.");

	/** Debug/act-scripting literacy (the Scripts skill is a later wave). */
	static bool PlayerReads();
};
