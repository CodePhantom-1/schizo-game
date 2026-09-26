// SimGameMode.h — the playable slice's game mode.
// The street is W6-B's SimStreetBuilder (the Moon Gate Quarter from places.csv
// and the house kit); the residents and the sun are W6-C's director and
// day/night actors; the verbs and HUD live in the player controller. This
// class wires them together and keeps the sim clock on screen.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SimGameMode.generated.h"

UCLASS()
class SCHIZOGAME_API ASimGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASimGameMode();
	virtual void BeginPlay() override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName = TEXT("")) override;

protected:
	/** The sim day the clock shows (to avoid re-reading the C API every line). */
	int64 LastShownDay = -1;

	/** The street's PlayerStart: the only start spot this game recognises. */
	UPROPERTY()
	class APlayerStart* StreetStart = nullptr;

	/** The city around the quarter (terrain, walls, ziggurat, lighthouse...). */
	UPROPERTY()
	class ASimEnvironment* Environment = nullptr;

	// --- development captures (-SimShots=<dir>, see TickShots) ---
	void TickShots(float DeltaSeconds);
	int32 ShotStage = 0;  // 0 = not parsed, -1 = off/finished, 1 = settling, 2 = shooting
	int32 ShotIndex = 0;
	float ShotTimer = 0.f;
	bool bShotPrepared = false;
	FString ShotDir;
	TArray<float> ShotHours;
	TArray<int32> ShotViews;
	/** -SimShotScreens=MainMenu,Pause,… : screens captured after the views (UI tour). */
	TArray<FString> ShotScreens;
	UPROPERTY()
	class ACameraActor* ShotCamera = nullptr;
};
