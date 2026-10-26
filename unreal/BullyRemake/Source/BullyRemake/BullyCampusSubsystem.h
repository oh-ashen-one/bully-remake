// OWNER: sm-campus
// Campus life at Bullworth Academy (Chapter 1): the school day, the trouble meter + prefects, faction attitudes and
// ambient speech, on top of the existing ambient walkers (UAmbientSubsystem / AAmbientWalker), the game clock
// (UBullyClockSubsystem) and the campaign's faction respect (UBullyCampaignSubsystem). Config: Config/Campus.json;
// install-derived data (triggers, cast voices, speech takes, the bell) from `python3 -m tools.importers campus`
// (imported/campus/, git-ignored). Behaviour sources are cited per block in Config/Campus.json and in
// docs/re-notes/campus-life.md.
//
// Two layers:
//   * Trouble core (every game world): the punishment-point meter. Missions drive it through `heat_set` / `hide`
//     (MissionSubsystem forwards them here, replacing the old "punishment-lite" stub); the 1_02B bin hide keeps its
//     contract (points held until hidden, drain while hidden, heat:zero at 0).
//   * Campus life (story play, or -BRCampusLife / -BRCampusSmoke in harness runs; -BRNoCampusLife turns it off): the
//     schedule (bells, students docking into the school at 8:45/12:45 and streaming out at 11:30/15:30, truancy,
//     attending class), prefects (vision cone from pedstats, warn / chase / bust), bust consequences (main.lua
//     F_PlayerBusted: sent to class, principal's office, or just put back), faction attitudes toward Jimmy
//     (SFaction.lua rows shifted by campaign respect), and contextual ambient barks + two-person conversations.
//
// Missions: while one runs the campus rules stand down (no truancy, no new trouble, no prefect chases, no
// fight-picking), every walker becomes a bystander (pass-through, not a combat target) and walkers whose model the
// mission spawns are hidden (the original's unique-model rule). Barks keep playing.
//
// Console: Campus.Status | Campus.Trouble <points> | Campus.Bell
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Dom/JsonObject.h"
#include "BullyCampusSubsystem.generated.h"

class AAmbientWalker;
class UAudioComponent;
class USoundAttenuation;
class USoundWaveProcedural;
class USoundBase;

UCLASS()
class BULLYREMAKE_API UBullyCampusSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBullyCampusSubsystem* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bBegun; }

	// --- trouble core (MissionSubsystem heat_set / hide) ----------------------------------------------------
	/** Mission-forced trouble (1_02B: 200 points, min 199 until the player hides): held until hidden, then drains at
	 *  DecayHidden per second. */
	void MissionHeatSet(float Points, float DecayHidden);
	void SetPlayerHidden(bool bHidden);
	float GetTroublePoints() const { return Points; }
	void AddTrouble(float Delta, const TCHAR* Why);
	void SetTrouble(float NewPoints, const TCHAR* Why);

	// --- queries ---------------------------------------------------------------------------------------------
	bool IsLifeActive() const { return bLife; }
	bool IsClassTime() const;
	/** Before 1_02C the school day is held at MORNING (see the .cpp). */
	bool ScheduleHeld() const;
	bool IsTruant() const { return bTruant; }
	int32 GetBustCount() const { return BustCount; }
	/** Jimmy's standing with a faction (0 Abhor .. 4 Adore). */
	int32 AttitudeToPlayer(const FString& Faction) const;
	void RingBell(const TCHAR* Why);
	void LogStatus() const;

private:
	// ---- config / data
	struct FPeriod { FString Id; float Start = 0.f, End = 0.f; bool bClass = false; };
	struct FCastRow { FString Model, Faction, Voice; bool bFemale = false; float Aggression = 0.f, Chicken = 0.f; bool bPrefect = false; };
	struct FMember
	{
		TWeakObjectPtr<AAmbientWalker> Walker;
		FString Model, Faction, Voice, Zone;   // zone: grounds | halls
		bool bPrefect = false;
		float Aggression = 0.f;
		double NextBark = 0.0;
		double NextNotice = 0.0;
		bool bWasBrawling = false, bWasDown = false;
		bool bScheduledOut = true;   // false = docked for class / the night
		FVector HomeStart = FVector::ZeroVector;
	};
	struct FVolume { FString Name; TArray<FVector2D> Poly; FVector Min = FVector::ZeroVector, Max = FVector::ZeroVector; bool bPoly = false; };

	bool LoadConfig();
	void LoadImported();
	bool LoadTriggerFile(const FString& Stem);
	bool FindPoint(const FString& Ref, FVector& Out) const;
	bool FindPath(const FString& Ref, TArray<FVector>& Out) const;
	bool FindVolume(const FString& Ref, FVolume& Out) const;
	const FCastRow* FindCast(const FString& Model) const;
	TArray<FVector> LaneRoute(const FVector& From, const FVector& To, const FString& Zone) const;
	void SetupCast();
	void AdoptWalker(AAmbientWalker* W, const FString& Zone);

	// ---- per-tick pieces
	void TickSchedule();
	void TickMissions();
	void TickTrouble(float Dt);
	void TickPrefects(float Dt);
	void TickReactions();
	void TickConversations();
	void TickDormancy();
	void TickHud();
	void TickAttend(float Dt);
	void SeedRespect();
	void BeginClassDocking(bool bRun);
	void StartClass();
	void EndClass(bool bAttended);
	void StreamOut();
	void AttendClass(const TCHAR* Why);
	void Bust(AAmbientWalker* By);
	void FinishBust();
	void PlacePlayer(const FVector& At, float Yaw);
	void SetFade(float Alpha);
	void OnWalkerFirstHit(AAmbientWalker* W, bool bFled);

	// ---- helpers
	APawn* Player() const;
	bool MissionActive() const;
	bool PlayerOnGrounds() const;
	bool PlayerInSchool() const;
	bool PlayerTrespassing(FString* Where = nullptr) const;
	bool CanSee(const AAmbientWalker* Prefect, const APawn* Target) const;
	FMember* MemberOf(const AAmbientWalker* W);
	float ClockHour() const;
	bool InWindow(float H, float A, float B) const { return A <= B ? (H >= A && H < B) : (H >= A || H < B); }
	const FPeriod* CurrentPeriod() const;

	// ---- speech
	struct FTake { FString Wav; float Seconds = 0.f; FString Text; };
	void LoadSpeech();
	/** Plays one take of <Voice>_<one of Codes> at the walker; returns its length (0 = nothing played). */
	float Bark(FMember& M, const TCHAR* Context, bool bForce = false);
	float PlayTake(const FTake& T, AActor* At, float Volume, const FString& Label);
	const TArray<FTake>* Takes(const FString& Voice, const FString& Code) const;
	TArray<FString> CodesFor(const TCHAR* Context) const;

	// ---- state
	bool bBegun = false, bLife = false, bSmoke = false;
	TSharedPtr<FJsonObject> Cfg;
	TArray<FPeriod> Periods;
	TMap<FString, FCastRow> CastRows;
	TMap<FString, TSharedPtr<FJsonObject>> Triggers;   // stem -> trigger file
	TArray<FMember> Members;
	TArray<FVector> GroundsDoors, HallsDoors, DormDoors, GroundsLane;
	FVolume Grounds;
	TArray<FVolume> SchoolVolumes, TrespassVolumes;
	FVector ClassExitPoint = FVector::ZeroVector, PrincipalSpawn = FVector::ZeroVector, PrincipalOutside = FVector::ZeroVector, SchoolFront = FVector::ZeroVector;
	FVector BellPoint = FVector::ZeroVector;

	// trouble
	float Points = 0.f;
	float MaxPoints = 400.f, ChaseThreshold = 100.f, DecayUnseen = 2.5f, DecayHidden = 25.f, TruancyFloor = 100.f, TrespassPerSecond = 10.f;
	float HitStudentPoints = 20.f, HitPrefectPoints = 100.f;
	bool bMissionHold = false, bHidden = false;
	double LastSeenByAuthority = -100.0;

	// schedule
	int32 LastMinuteKey = -1;
	bool bDocking = false, bInClass = false, bTruant = false, bAttendedThis = false, bLateShown = false, bNight = false;
	FString ClassId;
	float HeadToClassLead = 15.f / 60.f, LateAfter = 2.f / 60.f, StreamOutMinutes = 30.f;
	float CurfewStart = 23.f, CurfewEnd = 7.f, DormStart = 22.5f, SchoolClosedStart = 18.75f;
	FString TruancyAfterMission = TEXT("1_02C");
	TArray<bool> SkippedClasses;
	TArray<FIntPoint> SkippingThresholds;   // (skipped classes, points)

	// prefects / bust
	TWeakObjectPtr<AAmbientWalker> Chaser;
	double ChaseLostSince = -1.0, NextChaseBark = 0.0;
	float VisionHalfCos = 0.643f, VisionRange = 1200.f, BustRadius = 120.f, GiveUpSeconds = 6.f, GiveUpRange = 3000.f;
	int32 BustCount = 0, BustsAtLastPrincipal = 0;
	int32 BustPhase = 0;   // 0 none, 1 fading out, 2 consequence (lecture), 3 fading in
	double BustPhaseAt = 0.0;
	FString BustOutcome;
	TArray<FTake> LectureQueue;
	double LectureNextAt = 0.0;
	FVector BustReturnAt = FVector::ZeroVector;
	float AttendDwell = 0.f;
	bool bRespectSeeded = false;

	// missions
	bool bMissionWasActive = false;
	FString MissionModelsFor;
	TSet<FString> MissionModels;

	// reactions / conversations
	double NextConversation = 0.0, NextFightPick = 0.0, LastBarkAt = -100.0;
	int32 ActiveBarks = 0;
	TMap<FString, TMap<FString, TArray<FTake>>> Speech;   // voice -> code -> takes
	TMap<FString, TArray<FTake>> Lecture;                 // PRINCIPAL_LECTURING id -> takes
	TMap<FString, TArray<FString>> BarkCodes;             // context -> codes
	TArray<TPair<FString, FString>> ConversationPairs;
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> LiveAudio;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> Attenuation;
	TArray<uint8> BellPcm;
	int32 BellRate = 0, BellChannels = 0;
	int32 BarksPlayed = 0, ConversationsPlayed = 0, BellsRung = 0;

	// tick throttles
	double SetupTried = -1.0, NextSlow = 0.0, NextDormancy = 0.0;

	// HUD
	double NextHud = 0.0;
	float LastHudPoints = -1.f;

	// smoke harness (-BRCampusSmoke)
	void TickSmoke(float Dt);
	void SmokeCheck(const TCHAR* Name, bool bOk, const FString& Detail);
	int32 SmokeStep = 0;
	double SmokeStepAt = 0.0, SmokeStart = 0.0;
	int32 SmokePass = 0, SmokeFail = 0;
	TWeakObjectPtr<AAmbientWalker> SmokeTarget;
	int32 SmokeBellsAtStart = 0;
	float SmokePointsBefore = 0.f;
	bool bSmokeChaseSeen = false;
};
