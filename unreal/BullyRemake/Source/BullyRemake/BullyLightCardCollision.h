// OWNER: sm-humandrive
// Light cards are see-through: no collision on the imported level's glow/beam/bloom geometry.
//
// The importer makes every render triangle collision (complex-as-simple; the game's own Coll/ data is not parsed). That
// includes the original's additive light effects -- stadium floodlight shine (DL_SC1d_N, one 349 x 93 m mesh over the
// football field), lamp beams, window coronas, bloom cards. In the original they are pure visuals; here they were solid
// walls floating in the air: every slingshot pellet fired from the 1_04 tree at the jocks stopped on the floodlight shine
// 4 m out, pawns snapped onto it, and it fed bogus "floors" to ground traces.
//
// At world begin play every static mesh component whose materials are ALL additive light cards (additive blend AND a base
// texture from the light-effect set below) gets NoCollision. Additive grass/water/court-line decals keep their collision:
// they lie on (or are) the walkable ground. Mixed meshes (a solid neon tube + its beam) keep theirs too (per-section
// collision needs the importer; see HANDOFF). -BRNoLightCardFix disables it (A/B evidence).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BullyLightCardCollision.generated.h"

UCLASS()
class UBullyLightCardCollision : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** True if every material on the component is an additive light-effect card. */
	static bool IsLightCard(const class UStaticMeshComponent* C);
};
