// OWNER: slice-weapons
// The player's arsenal. Loads Config/Weapons.json, tracks ammo per weapon, cycles the equipped weapon (Fists = none)
// and fires ABullyProjectiles with third-person aim:
//   ranged: toward whatever the camera's centre ray hits (or the lock-on target);
//   thrown: a ballistic arc onto the lock-on target / the camera-ray point, else a fixed-pitch lob.
// Every weapon starts unlocked with its starting ammo (console BRGiveWeapons refills). -BRWeaponSmoke runs a headless
// check that fires each weapon at a spawned target and logs PASS/FAIL per weapon.
// Look + feel: the equipped weapon's held model sits in Jimmy's right hand ('held_mesh' on bone Root-R-Hand); firing
// plays its 'fire_clip' on the upper body and lets the shot go 'release_s' into the clip; 'sfx_fire' plays at release.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BullyProjectile.h"
#include "BullyWeaponComponent.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class ABullyCombatPawn;

struct FBullyWeaponDef
{
	FName Id;
	FString Name;
	bool bThrown = false;
	FBullyProjectileSpec Spec;
	int32 AmmoStart = 10;
	int32 AmmoMax = 20;
	float FireInterval = 0.5f;     // s between shots
	float ThrowPitchDeg = 18.f;    // thrown, no target: lob this far above the camera ray
	FString MeshPath;              // optional local projectile mesh; empty -> engine sphere
	FString HeldMeshPath;          // model in the right hand while equipped (thrown: the item itself)
	FVector HeldOffset = FVector::ZeroVector;   // relative to the hand bone (cm)
	FRotator HeldRot = FRotator::ZeroRotator;
	float HeldScale = 1.f;
	FName FireClip;                // Jimmy clip, upper body
	FName HoldClip;                // upper-body loop while equipped (launchers carried at the ready)
	float FireRate = 1.f;
	float ReleaseSeconds = 0.f;    // into the clip (at rate 1)
	FName SfxFire;
};

class UBullyStorySubsystem;

UCLASS(ClassGroup = (Bully), meta = (BlueprintSpawnableComponent))
class UBullyWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UBullyStorySubsystem;   // story-mode save/load reads and restores the ammo table

public:
	UBullyWeaponComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool HasEquipped() const { return Defs.IsValidIndex(Equipped); }
	const FBullyWeaponDef* GetEquipped() const { return HasEquipped() ? &Defs[Equipped] : nullptr; }
	int32 GetAmmo(FName Id) const { const int32* A = Ammo.Find(Id); return A ? *A : 0; }
	const TArray<FBullyWeaponDef>& GetDefs() const { return Defs; }

	/** Step +1/-1 through Fists and every weapon that has ammo. */
	void Cycle(int32 Dir);
	/** Equip by id (NAME_None = Fists). False if unknown or out of ammo. */
	bool Equip(FName Id);
	/** Fire the equipped weapon along the current aim. False if nothing was fired (holstered, cooldown, busy, empty). */
	bool Fire();
	/** Fire the equipped weapon at a world point (smoke tests, scripted throws). */
	bool FireAt(const FVector& Target);
	/** Fire at an actor's chest; the shot re-aims at where it is on the release frame. */
	bool FireAtActor(AActor* Target);
	void RefillAll();
	/** sm-humandrive: empty every weapon (fists only) -- the human-mode smoke starts a mission with only what the campaign
	 *  would have granted by then. */
	void EmptyAll();
	/** Campaign grant (on_pass / give_weapon): v1 every weapon starts unlocked, so this tops ammo up to the
	 *  weapon's starting loadout and logs. False if the id is not in the arsenal. */
	bool GiveWeapon(FName Id);
	/** Campaign grant (give_ammo): top the weapon's ammo up to N. False if the id is not in the arsenal. */
	bool GiveAmmo(FName Id, int32 N);
	/** Tests / captures: drop the cooldown and any shot waiting for its release frame. */
	void ResetCooldown() { NextFireAt = 0.0; bPending = false; }

	/** "WEAPON  Slingshot  30/40" for the local player's HUD (empty if there is no local armed player). */
	static FString HudLine();

private:
	bool LoadDefs();
	bool CanFireNow() const;
	bool LaunchEquipped(const FVector& Target, bool bHasTarget);
	bool LaunchNow(const FVector& Target, bool bHasTarget);
	void UpdateHeld();
	UStaticMesh* LoadMesh(const FString& Path);
	FVector MuzzleLocation() const;
	bool AimPoint(FVector& Out) const;
	UStaticMesh* VisualFor(const FBullyWeaponDef& D, bool& bIsFallback);
	void TickSmoke();

	TArray<FBullyWeaponDef> Defs;
	TMap<FName, int32> Ammo;
	int32 Equipped = -1;           // index into Defs, -1 = Fists
	double NextFireAt = 0.0;

	UPROPERTY(Transient) TMap<FName, TObjectPtr<UStaticMesh>> MeshCache;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<UStaticMesh>> PathCache;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> FallbackSphere;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Held;

	// a shot waiting for its release frame
	bool bPending = false;
	double PendingAt = 0.0;
	FVector PendingTarget = FVector::ZeroVector;
	bool bPendingHasTarget = false;
	TWeakObjectPtr<AActor> PendingActor;   // locked-on / given target: re-aimed at release
	int32 HeldShownFor = -2;       // Equipped index the held model shows (-2 = not set up yet)
	double HideHeldUntil = 0.0;    // thrown: the hand is empty for a moment after the throw

	struct FSmoke
	{
		bool bEnabled = false;
		int32 Step = 0;
		double StepStart = 0.0;
		int32 Weapon = 0;
		int32 Passes = 0;
		TArray<FString> Failures;
		int32 HitsBefore = 0;
		int32 DetonationsBefore = 0;
		float HealthBefore = 0.f;
		TWeakObjectPtr<ABullyCombatPawn> Target;
	} Smoke;

	static TWeakObjectPtr<UBullyWeaponComponent> LocalArmed;
};
