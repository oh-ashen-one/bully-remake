// OWNER: sm-visual
// Runtime scene fix-ups for the imported Bullworth level.
//
// 1) Overlapping scenes. The importer places every original scene at its native coordinates; the go-kart race minigame
//    tracks (iMGRaceA/B/C, scene-local around the origin) therefore sit stacked 0-100 m ABOVE the campus. They are
//    invisible from below (single-sided) but at the golden-hour sun angle they cast the whole back half of the campus --
//    parking lot, auto shop yard, football field -- into solid shadow (near-black frames in Ch1 missions), and their
//    collision catches ground traces/teleports. Hidden in game: not rendered, no shadow, no collision.
//
// 2) Characters indoors. Interior pockets (scenes isc_* / ischool) are on lighting channel 1 only, lit by the rig's
//    channel-1 interior key/fill (tools/ue/lighting.py); the sun is channel 0. Characters are channel 0 by default, so
//    inside a pocket they were lit by the low amber sun straight THROUGH the walls (channel-1 walls cast no channel-0
//    shadow) while the room around them sat in the dim interior light. Every character standing on an interior floor is
//    moved to channel 1 (lit like the room), and back to channel 0 outdoors. The floor's br_scene tag decides.
//
// Config: DefaultGame.ini [/Script/BullyRemake.BullySceneFixups] HiddenScenes / InteriorScenePrefixes (keep the prefixes
// equal to tools/ue/lighting_variants.json interior.scene_prefixes). -BRNoSceneFixups disables both (A/B evidence).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BullySceneFixups.generated.h"

class ACharacter;

UCLASS(config = Game)
class UBullySceneFixups : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** br_scene:<name> tags (exact match, case-insensitive) whose static meshes are hidden in game. */
	UPROPERTY(config)
	TArray<FString> HiddenScenes;

	/** br_scene:<prefix>* floors that put a character on the interior lighting channel. */
	UPROPERTY(config)
	TArray<FString> InteriorScenePrefixes;

	/** True if this character is currently lit as indoors (channel 1). */
	bool IsIndoors(const ACharacter* C) const;

private:
	void UpdateCharacterChannels();
	/** -1 unknown (no tagged floor under him), 0 outdoors, 1 on an interior floor. */
	int32 ClassifyFloor(const ACharacter* C) const;
	TMap<TWeakObjectPtr<ACharacter>, bool> Indoors;
	double NextUpdate = 0.0;
	bool bBegun = false;
};
