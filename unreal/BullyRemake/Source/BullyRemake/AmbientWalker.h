// OWNER: builder-B4 (ambient)
// An imported student walking a fixed waypoint loop (milestone 4 "signs of life"; no schedules, no AI).
#pragma once

#include "CoreMinimal.h"
#include "BullyCharacter.h"
#include "AmbientWalker.generated.h"

/**
 * A BullyCharacter (same capsule, movement and C++ locomotion anim instance as Jimmy) that follows `Waypoints` in a
 * loop at its own walk speed and idles a random time at each waypoint. Spawned by UAmbientSubsystem from
 * Config/AmbientLoops.json with the student's skeletal mesh (BullyCharacter::MeshPath) and clips (resolved from the
 * mesh's package folder by UBullyAnimInstance). Collision with the world is real: a walker that stops making progress
 * for StuckSeconds skips to its next waypoint and logs BR_AMBIENT STUCK, so a bad path shows up in the log.
 *
 * Collision between pawns (polish1): the walker is a normal Pawn capsule, so the PLAYER is blocked by it (a walker is a
 * solid body, no more walking through students). Walker <-> walker stays pass-through (each pair adds the other to its
 * capsule's IgnoreActorWhenMoving in BeginPlay): a blocking walker pair on a shared loop (two students meeting head-on
 * in a corridor, loops that cross) can deadlock, and the stuck logic would then skip waypoints and desynchronise the
 * loops. (First try: a custom "Walker" object type. Rejected: the level's meshes answer a new channel differently from
 * Pawn, which froze walkers at corners their Pawn-type predecessors walked through: 3 STUCK events per walker in 100 s.)
 * To keep the player from being trapped (a walker idling in a
 * doorway, a walker walking into the player), each walker yields: when the player is within YieldRadius and closing
 * in (or touching), the walker pauses YieldPauseSeconds and then side-steps off the player's line for YieldStepSeconds
 * toward whichever side is free; yield time is excluded from the stuck check. Logged as BR_AMBIENT YIELD.
 */
UCLASS()
class AAmbientWalker : public ABullyCharacter
{
	GENERATED_BODY()

public:
	AAmbientWalker();

	FString LoopId;
	FString StudentId;
	TArray<FVector> Waypoints;
	FVector2D IdleSecondsRange = FVector2D(1.5, 4.0);
	float ArriveRadius = 70.f;
	float StuckSeconds = 3.f;
	int32 StartIndex = 0;
	float YieldRadius = 190.f;          // player closer than this (centre to centre) and closing in -> yield
	float YieldPauseSeconds = 0.35f;    // freeze first (a head-on player usually just walks past a pausing student)
	float YieldStepSeconds = 0.9f;      // then side-step this long
	float YieldCooldownSeconds = 2.5f;  // minimum gap between two yields (prevents dancing in front of a stationary player)

	int32 GetLoopsCompleted() const { return LoopsCompleted; }
	int32 GetWaypointsReached() const { return WaypointsReached; }
	int32 GetStuckEvents() const { return StuckEvents; }
	float GetPathLengthWalked() const { return PathWalked; }
	int32 GetNextIndex() const { return NextIndex; }
	FVector GetNextWaypoint() const { return Waypoints.IsValidIndex(NextIndex) ? Waypoints[NextIndex] : GetActorLocation(); }
	int32 GetYieldCount() const { return YieldCount; }
	float GetMinPlayerDistance() const { return MinPlayerDist; }

	/** Scripted-NPC hand-back (UScriptedNPCSubsystem "make ambient"): replaces the loop and starts walking it from `Start`. */
	void ResumeLoop(const TArray<FVector>& Loop, int32 Start = 0);

	/** ai-npc (UNPCChatSubsystem): mid-conversation hold — stop walking and face the player until released. */
	bool bChatHold = false;

	// --- sm-campus: schedule / authority hooks, driven by UBullyCampusSubsystem ---------------------------------
	// Loop = the home loop above (default). Errand = walk/run straight to a point (a class door at the bell, a hangout
	// after class), optionally vanishing into it on arrival (Inside: hidden, no collision, no tick until Emerge).
	// Chase = a prefect running at the player. Flee = a non-fighter running away after being hit. Hold = stand and
	// face someone (a bark or a two-person conversation).
	enum class ECampusMode : uint8 { Loop, Errand, Inside, Chase, Flee, Hold };
	/** Route: optional waypoints walked first (the campus approach lane round the front terrace). */
	void CampusGoTo(const FVector& Dest, bool bRun, bool bEnterOnArrive, const TArray<FVector>& Route = TArray<FVector>());
	void CampusEnter();
	void CampusEmerge(const FVector& At);
	void CampusChase(AActor* Target);
	void CampusFlee(const FVector& From, float Seconds);
	void CampusHold(float Seconds, const AActor* FaceTo);
	/** Back to the home loop (the waypoints the walker was spawned with), from the nearest home waypoint. */
	void CampusResume();
	/** Missions: pass-through to every pawn and out of the combat registry (not a lock-on / swing target). */
	void SetBystander(bool bOn);
	bool IsBystander() const { return bBystander; }
	/** Mission uniqueness (F_SetCharacterModelsUnique): the running mission uses this model, so the ambient double is
	 *  hidden (no collision, no tick) until the mission ends. */
	void SetCampusSuppressed(bool bOn);
	bool IsCampusSuppressed() const { return bSuppressed; }
	/** Far from the player: frozen and hidden (performance); woken again when the player comes back in range. */
	void SetDormant(bool bOn);
	bool IsDormant() const { return bDormant; }
	ECampusMode GetCampusMode() const { return CampusMode; }
	bool IsInside() const { return CampusMode == ECampusMode::Inside; }
	bool IsBrawling() const { return bBrawling; }
	bool IsIdling() const { return bIdling && CampusMode == ECampusMode::Loop; }
	bool HasErrandArrived() const { return bErrandArrived; }
	FVector GetErrandDest() const { return ErrandDest; }
	/** A non-fighter (pedstats aggression ~0: generic students, girls) runs from the player instead of fighting back. */
	bool bFleeWhenHit = false;
	/** Run speed of an errand (late for class) or a chase (prefects run at the ped.dat run speed, 6 m/s). */
	float CampusRunSpeed = 560.f;
	/** Fired once when the walker's first health loss turns it into a brawler (or a fleer). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWalkerHit, AAmbientWalker* /*Walker*/, bool /*bFled*/);
	FOnWalkerHit OnFirstHit;
	/** The walker joins a fight against the player (faction ally, or an Abhor-attitude student picking one). */
	void JoinFightAgainstPlayer() { if (!bBrawling) { StartBrawl(); } }

protected:
	/**
	 * ai-npc "fight anybody": every walker's combat component joins the Enemy team (BeginPlay), so the player's
	 * swings, grapples and lock-on connect with ordinary students. The first health loss converts the walker into
	 * a brawler: the loop is abandoned and an ABullyCombatAIController possesses it (same AI as the sparring
	 * partner, no tutorial gates/tether). A KO'd walker stays down. Scripted NPCs inherit this: hitting a
	 * mission bully drops its script and starts the fight.
	 */
	void StartBrawl();
	bool bBrawling = false;
	float BrawlRefHealth = -1.f;

public:

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void Advance();
	/** Player-yield state machine; returns true while it owns the walker's movement this tick (waypoint walking is skipped). */
	bool TickYield(double Now, const FVector& Pos);

	int32 NextIndex = 0;
	bool bIdling = false;
	double IdleUntil = 0.0;
	int32 LoopsCompleted = 0;
	int32 WaypointsReached = 0;
	int32 StuckEvents = 0;
	float PathWalked = 0.f;
	FVector LastPos = FVector::ZeroVector;
	FVector ProgressRefPos = FVector::ZeroVector;
	double ProgressRefTime = 0.0;
	double NextLog = 0.0;
	// yield: 0 none, 1 pause, 2 side-step
	int32 YieldPhase = 0;
	double YieldPhaseEnd = 0.0;
	double YieldCooldownUntil = 0.0;
	FVector YieldDir = FVector::ZeroVector;
	int32 YieldCount = 0;
	float MinPlayerDist = 1e9f;

	// sm-campus state
	bool TickCampus(float Dt, double Now, const FVector& Pos);
	void SetHiddenAndInert(bool bHide);
	ECampusMode CampusMode = ECampusMode::Loop;
	TArray<FVector> HomeWaypoints;
	FVector ErrandDest = FVector::ZeroVector;
	TArray<FVector> ErrandRoute;
	bool bErrandRun = false, bErrandEnter = false, bErrandArrived = false;
	int32 ErrandStucks = 0;
	FVector ErrandRefPos = FVector::ZeroVector;
	double ErrandRefTime = 0.0;
	TWeakObjectPtr<AActor> ChaseTarget;
	FVector FleeFrom = FVector::ZeroVector;
	double ModeUntil = 0.0;
	TWeakObjectPtr<const AActor> HoldFace;
	ECampusMode ModeBeforeHold = ECampusMode::Loop;
	double HoldResumeErrandUntil = 0.0;
	bool bBystander = false, bSuppressed = false, bDormant = false;
	float SavedRunSpeed = -1.f;
};
