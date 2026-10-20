// OWNER: slice-town
// Where to put a pawn that is being teleported: real, open, walkable GROUND near a spot — never a roof, an awning, an
// upper storey or a gap between walls. Used by every teleport (G menu, number row, BRGo) and the ride rescue.
//
// At each candidate XY (the spot first, then rings outward) every walkable surface in the vertical band is listed by
// tracing down repeatedly. A surface counts as ground only when there is no other walkable surface under it (a roof,
// a bridge deck or an upper floor always has one), it is not water, a standing capsule fits on it, and an open patch
// around it is clear of walls. Interior pockets work the same way: their ground floor has nothing under it.
#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;

struct FBullyLanding
{
	/** Ground point (on the surface, not the capsule centre) nearest to Spot, searched out to Radius cm. False if none. */
	static bool Find(const UWorld* World, const FVector& Spot, float Radius, FVector& OutGround, const AActor* Ignore = nullptr,
		float ClearHalf = 120.f, float BandUp = 3000.f, float BandDown = 3000.f);
};
