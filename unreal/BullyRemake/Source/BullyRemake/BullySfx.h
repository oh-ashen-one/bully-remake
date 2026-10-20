// OWNER: slice-weapons
// Gameplay sound events -> imported original-game waves. Config/Sfx.json maps an event id (weapon_fire_slingshot,
// splat_egg, punch_heavy, gokart_engine_loop, ...) to one or more waves (bank + wave name); the waves are decoded from
// the player's own install and imported to /Game/Imported/Audio/<bank>/<wave> by tools/ue/run_import.sh --sfx.
// A missing event or asset is silent (logged once), so machines without the import still run.
#pragma once

#include "CoreMinimal.h"

class AActor;
class UAudioComponent;
class USceneComponent;
class USoundBase;
class UObject;

struct FBullySfx
{
	/** One-shot at a world location (random variant, pitch jitter). */
	static void Play(const UObject* WorldContext, FName Event, const FVector& At, float VolumeScale = 1.f);
	/** Looping sound attached to a component (engines, rolling wheels, a gas cloud's hiss). Null if the event is
	 *  missing. The caller owns the component: set its volume / pitch, Stop() or destroy it with the owner. */
	static UAudioComponent* Loop(AActor* Owner, USceneComponent* AttachTo, FName Event, float VolumeScale = 1.f);
	/** Has the event got at least one imported wave? */
	static bool Has(FName Event);
};
