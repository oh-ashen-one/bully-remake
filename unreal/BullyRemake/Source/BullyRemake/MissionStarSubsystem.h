// OWNER: campaign-spine
// Owns the overworld star markers: re-evaluates the campaign table (UBullyCampaignSubsystem) on world begin play,
// clock hour change, campaign change (pass/chapter/respect) and mission end, spawning an AMissionStar for every
// available row whose star_location is in the loaded area (v1: area 0 outdoors only — interior rows are skipped
// with a one-time log) and despawning the rest. Stars never spawn while a mission is active, nor in any -BR*
// harness run except -BRCampaignSmoke (which is the star system's own headless test).
//
// -BRCampaignSmoke drives the design doc's verification: boot chain (day_one via the mission smoke driver, no
// quit) -> expected star set -> walk into the 1_02B star -> the 1_02B -> 1_02C chain plays out -> clock window
// checks (23:00 despawns day-windowed stars, paused clock keeps availability stable, 8:00 brings them back) ->
// pass-side effects (force-pass 1_09 pauses the clock; force-pass 1_B sets chapter 2, force-completes the
// Halloween optional + level-1 classes, sets respect). Each check logs BR_CAMPAIGN <name> PASS/FAIL and the run
// ends with a BR_CAMPAIGN SMOKE summary + self-quit, like the other harnesses.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MissionStarSubsystem.generated.h"

class AMissionStar;

UCLASS()
class UMissionStarSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UMissionStarSubsystem* Get(UWorld* World);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Re-evaluate the table against the current campaign/clock/mission state and reconcile the spawned stars. */
	void Refresh();
	AMissionStar* FindStar(FName MissionId) const;

private:
	void DespawnAll(const TCHAR* Why);
	void TeleportPlayer(const FVector& Loc);

	// -BRCampaignSmoke phases
	void TickSmoke(float Dt);
	void SmokeCheck(FName Name, bool bOk, const FString& Detail);
	void SmokeDone();

	TMap<FName, TWeakObjectPtr<AMissionStar>> Stars;
	TSet<FName> LoggedSkips;         // one-time logs (interior rows)
	bool bSuppressed = false;        // a -BR* harness run (other than -BRCampaignSmoke)
	double NextEvalAt = 0.0;         // slow cadence re-check (windows finer than one hour)
	bool bSubscribed = false;

	struct FSmoke
	{
		bool bEnabled = false;
		int32 Phase = 0;
		double Elapsed = 0.0;
		double PhaseAt = 0.0;          // when the current phase started (per-phase timeouts)
		double NextDriveAt = 0.0;      // generic mission driver cadence
		int32 Passes = 0, Fails = 0;
		bool bDone = false;
		// last mission-finish observed through OnMissionFinished
		bool bHaveFinish = false;
		FString FinishedId;
		bool bFinishedOk = false;
		bool bStar103AtEntry = false;  // check 3 precondition: the 1_03 star was up before the clock jump
		bool bStarted102B = false;     // phase 3: the star-started mission is running
		bool bClockGone = false;       // phase 7: 1_03 unavailable at 23:00
		bool bClockStable = false;     // phase 8: availability did not age under a paused clock
	} Smoke;

	void OnMissionDone(const struct FMissionDef& M, bool bSuccess);
	void OnClockOrCampaign();
};
