// OWNER: sm-humandrive
// Collision the original does not have, switched off at world begin play in the imported Bullworth level.
//
// The importer makes every render triangle collision (complex-as-simple; the game's own Coll/ data is not parsed). Two
// kinds of imported geometry were solid where a player of the original walks and shoots freely:
//
// 1) Light cards. The original's additive light effects -- the football field's floodlight shine (DL_SC1d_N, one
//    349 x 93 m mesh), lamp beams, window coronas, bloom cards -- are pure visuals. Here they were walls in the air:
//    every slingshot pellet fired from the 1_04 tree at the jocks stopped on the floodlight shine 4 m out, pawns snapped
//    onto it and ground traces read it as a floor. Every static mesh component whose materials are ALL additive light
//    cards (additive blend AND a base texture from the light-effect set in the .cpp) gets NoCollision. Additive
//    grass/water/court-line decals keep their collision: they lie on (or are) the walkable ground. Mixed meshes (a solid
//    neon tube + its beam) keep theirs too (per-section collision needs the importer).
//
// 2) The go-kart race tracks (scenes iMGRaceA/B/C) sit at their scene-local coordinates, stacked over the campus; their
//    collision blocks the school grounds (one stands on the 1_04 tree's line of fire). Hidden + no collision. sm-visual's
//    UBullySceneFixups does the same (plus lighting work); both are idempotent, so this half can go once that lands.
//
// -BRNoCollisionFixups disables both (A/B evidence).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BullyCollisionFixups.generated.h"

UCLASS()
class UBullyCollisionFixups : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** True if every material on the component is an additive light-effect card. */
	static bool IsLightCard(const class UStaticMeshComponent* C);
};
