// OWNER: campaign-spine
// The overworld mission-start marker: a box trigger plus a placeholder beacon (a stretched cylinder, gold = story
// mission, red = side mission) spawned by UMissionStarSubsystem for every available campaign row. Like
// AMissionTrigger, containment is polled in Tick (point-in-box on the player pawn) rather than physics overlap
// events, so teleports and -nullrhi runs behave identically. Entering the box while no mission is active starts
// the mission through the campaign's availability gate (no dev override — a refused start just logs and the star
// stays up).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionStar.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPointLightComponent;

UCLASS()
class AMissionStar : public AActor
{
	GENERATED_BODY()

public:
	AMissionStar();

	/** Campaign table id this star starts on entry. */
	FName MissionId;
	/** From the row's "star" field: "gold" = story, "red" = side. */
	bool bGold = true;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	TObjectPtr<UBoxComponent> Box;
	TObjectPtr<UStaticMeshComponent> Beacon;   // tall light beam (readable from across the grounds)
	TObjectPtr<UStaticMeshComponent> Core;     // the spinning, bobbing "star" gem at the foot of the beam
	TObjectPtr<UPointLightComponent> Glow;     // warm pool of light on the ground around the marker
	double SpawnedAt = 0.0;
	bool bPlayerInside = false;
	double NextStartLogAt = 0.0;   // throttle the "start refused" log while standing in a star
};
