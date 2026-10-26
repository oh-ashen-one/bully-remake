// OWNER: sm-minigames
// The original's interactive pieces of the Chapter 1 opening, driven by mission data (UMissionSubsystem actions):
//
//   - Chemistry class (scripts/secnd/ClassChem.lua, C_Chem_1): a scrolling button-prompt sequence. The class script queues
//     ClassChemAddAction(seq, act, waitTime, window) entries (act 0..3 = cross/circle/square/triangle on class 1, 34 = the
//     end marker), then ClassChemStartSeq; one missed or wrong prompt ends the run (MinigameEnd) and the class restarts the
//     sequence -- three runs in all ("Get Ready" / "Try Again" / "Last Chance"), the third failure blows the experiment up
//     and fails the class. A clean run passes: grade screen, then the class-1 unlock. Sequence data:
//     Config/Minigames/ClassChem.json. The scroll/hit-window rules live in Bully.exe (DRM-encrypted, not read): ours are
//     written from the script's parameters (window seconds per prompt, waitTime gaps) and documented there.
//   - Locker lockpick (scripts/secnd/MGLockPicking.lua, 1_02B): a three-number combination dial, numbers 0..39
//     (MGLockSetCombo(math.random(0,39) x3)); the original tutorial (TUT_PICK1): "slowly rotate in the direction of the
//     arrow". A failed / abandoned attempt within 5 m of the same locker keeps the combination (gLastLockPickCombo).
//   - Climb spots (1_04's tree, /Global/TreeClimb): press the action key at the trunk to climb to the perch (TREE_ON_BOT,
//     TREE_UP_CYCLE, BRNCH_UP2), sit there (TREE_IDLE) with the weapon usable, press it again to climb down.
//   - Player clips (1_03's PlayerPickupSlingshot = WEAPON\PICKUP_SLINGSHOT): a full-body one-shot on Jimmy.
//
// The overlay is pure C++ Slate like FBullyHUD. Original button glyphs and HUD strings are read at runtime from
// imported/minigames/ (python3 -m tools.importers minigame); without them it draws its own keys and our own wording.
// Clips come from Content/Imported (tools/ue/run_import_character.sh --anims-only); a missing clip is skipped.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "InputCoreTypes.h"
#include "Styling/SlateBrush.h"
#include "BullyMinigames.generated.h"

class SWidget;
class UTexture2D;
class APlayerController;
class ABullyCharacter;
class UInputMappingContext;
class UInputAction;

UCLASS()
class UBullyMinigameSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	enum class EKind : uint8 { None, Chem, Lock };
	/** Chem: Intro -> Ready ("Get Ready"/"Try Again"/"Last Chance") -> Begin -> Play -> (Failed -> Ready)* -> Grades -> Unlock.
	 *  Lock: Play -> Open. */
	enum class EPhase : uint8 { Idle, Intro, Ready, Begin, Play, Failed, Grades, Unlock, Open };

	static UBullyMinigameSubsystem* Get(const UWorld* World);
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	struct FStart
	{
		EKind Kind = EKind::None;
		FName Id;                    // minigame id (mission data)
		FString Config;              // chem: class config name in Config/Minigames/ClassChem.json ("1")
		FName SuccessEvent, FailEvent;
		FVector At = FVector::ZeroVector;   // lock: the locker (combo memory is per spot)
	};
	/** Starts a minigame (input locked, overlay up). The result fires SuccessEvent / FailEvent on UMissionSubsystem. */
	bool Begin(const FStart& S);
	/** Mission teardown / stage change: stop without firing anything. */
	void Abort(const TCHAR* Why);
	bool IsRunning() const { return Kind != EKind::None; }
	EKind GetKind() const { return Kind; }
	/** Result of the last finished minigame (true = success). */
	bool LastSucceeded() const { return bLastOk; }

	// --- harness ------------------------------------------------------------------------------------------------
	/** -BRMissionSmoke: 1 = play through the same input handlers a key press reaches (chem: botch the first run once to
	 *  exercise the original's retry, then play every prompt; lock: walk away once after the first click -- the combo
	 *  must survive -- then turn the dial the arrow's way). */
	int32 SmokeAssist = 0;
	/** What a player sees, for the human driver: the key for the prompt inside its window right now (none if no prompt is
	 *  answerable or it was answered). bWrong = a key that does NOT match (to botch on purpose). */
	FKey HumanChemKey(bool bWrong = false) const;
	/** The lock's arrow: +1 clockwise / -1 counter-clockwise / 0 = not turning now; Key = the key that turns that way. */
	int32 HumanLockDir(FKey& Key) const;
	int32 GetChemRun() const { return Chem.Run; }

	// --- climb spots (mission-scoped) ---------------------------------------------------------------------------
	struct FClimbSpot
	{
		FName Id;
		FVector Base = FVector::ZeroVector;   // trunk foot (feet height)
		FVector Trunk = FVector::ZeroVector;  // top of the trunk climb (feet height), above Base
		FVector Top = FVector::ZeroVector;    // the perch (feet height)
		float TopYaw = 0.f;
		float Radius = 170.f;                 // how close to Base the climb prompt shows
		FString UpPrompt = TEXT("Climb"), DownPrompt = TEXT("Climb down");
	};
	void ArmClimb(const FClimbSpot& Spot);
	void DisarmClimbs();
	/** Start a climb now (up = from the base, down = from the perch). Harness + the action key both land here. */
	bool RequestClimb(FName Id, bool bUp);
	bool IsClimbArmed(FName Id) const;
	bool IsPerched(FName Id) const { return ClimbState == EClimb::Perched && ClimbId == Id; }
	bool IsClimbing() const { return ClimbState == EClimb::Up || ClimbState == EClimb::Down; }
	/** The climb prompt the player sees right now (empty = none). */
	FString ClimbPrompt() const;
	const FClimbSpot* FindClimb(FName Id) const;
	/** "climb:<id>_top" / "climb:<id>_bottom" -> id + direction (drivers: which climb a stage waits on). */
	static bool ParseClimbEvent(FName Event, FName& OutId, bool& bOutUp)
	{
		FString S = Event.ToString();
		if (!S.StartsWith(TEXT("climb:"))) { return false; }
		S.RightChopInline(6);
		if (S.EndsWith(TEXT("_top"))) { bOutUp = true; S.LeftChopInline(4); }
		else if (S.EndsWith(TEXT("_bottom"))) { bOutUp = false; S.LeftChopInline(7); }
		else { return false; }
		OutId = FName(*S);
		return true;
	}

	// --- player clips -------------------------------------------------------------------------------------------
	/** Full-body one-shot on Jimmy (first candidate that exists), movement locked while it plays. Seconds <= 0 = the clip's
	 *  length. Returns false if none of the clips is imported (nothing happens, mission flow continues). */
	bool PlayPlayerClip(const TArray<FString>& Candidates, float Seconds, bool bLockMove);

	// --- overlay (read by the Slate widget) ---------------------------------------------------------------------
	struct FChemPrompt { int32 Act = 0; double DueAt = 0.0; float Window = 0.6f; int32 Result = 0; bool bEnd = false; FName Clip; };
	struct FChemState
	{
		TArray<FChemPrompt> Prompts;   // the current run (acts fixed after the first run, like the script)
		int32 Next = 0;                // first unanswered prompt
		int32 Run = 0;                 // 0-based run index (0..2)
		int32 Runs = 3;
		double PhaseAt = 0.0;          // world time the phase began
		FString ClassTitle;            // "Chemistry 1"
		int32 ClassNum = 1;
		bool bPassed = false;
		FName PromptClipPending;       // clip to play after a hit
	};
	struct FLockState
	{
		float Dial = 0.f;               // 0..40 (wraps)
		int32 Combo[3] = { 0, 0, 0 };
		int32 Step = 0;                 // 0..2 = next number, 3 = open
		double ClickAt = -10.0;         // last click (feedback flash)
		float Speed = 0.f;              // numbers per second (signed, + = clockwise)
		bool bTurned = false;
	};
	EPhase GetPhase() const { return Phase; }
	double Now() const;
	const FChemState& GetChem() const { return Chem; }
	const FLockState& GetLock() const { return Lock; }
	bool UsingGamepad() const { return bGamepad; }
	FString Text(const TCHAR* Key) const;
	const FSlateBrush* Glyph(const TCHAR* Name) const;
	/** Lock direction for step i: +1 clockwise, -1 counter-clockwise (cw, ccw, cw like a real combination lock). */
	static int32 LockDirFor(int32 Step) { return (Step % 2 == 0) ? 1 : -1; }

private:
	void TickChem(float Dt, APlayerController* PC);
	void TickLock(float Dt, APlayerController* PC);
	void TickClimb(float Dt, APlayerController* PC);
	void TickPlayerClip(float Dt);
	void ChemPress(int32 Act);              // an action button (0..3) went down
	void ChemFailRun(const TCHAR* Why);
	void ChemStartRun();
	void LockTurn(float Numbers);           // rotate the dial (+ = clockwise)
	void Finish(bool bOk);
	bool LoadChemConfig(const FString& Name, TArray<FChemPrompt>& OutSteps, float& OutFirstExtra, int32& OutAllowed, int32& OutRuns, FString& OutTitle) const;
	void LockInput(bool bOn, bool bFullBlock);
	void ShowOverlay(bool bOn);
	void LoadAssets();
	ABullyCharacter* Player() const;
	void SetPlayerClip(FName Clip, float Seconds);   // ride-layer one-shot; NAME_None clears
	void Rumble(float Intensity, float Seconds);
	void Fire(FName Event);

	EKind Kind = EKind::None;
	EPhase Phase = EPhase::Idle;
	FStart Args;
	bool bLastOk = false;
	bool bGamepad = false;
	FChemState Chem;
	FLockState Lock;
	TArray<int32> ChemActs;              // acts picked on the first run (kept for the retries)
	float ChemFirstExtra = 2.f;
	int32 ChemAllowed = 4;
	double LockAbortAt = -1.0;           // smoke: walked away once
	bool bSmokeBotched = false;
	bool bSmokeWalkedAway = false;
	// per-spot combo memory (the original's shared.gLastLockPickCombo / gLastLockPickLocation, 5 m)
	bool bHaveLastCombo = false;
	FVector LastComboAt = FVector::ZeroVector;
	int32 LastCombo[3] = { 0, 0, 0 };

	// climb
	enum class EClimb : uint8 { None, Up, Perched, Down };
	TArray<FClimbSpot> Climbs;
	EClimb ClimbState = EClimb::None;
	FName ClimbId;
	double ClimbAt = 0.0;
	FVector ClimbFrom = FVector::ZeroVector;
	int32 ClimbLeg = 0;
	uint8 SavedMoveMode = 1;

	// player clip
	double ClipEndsAt = -1.0;
	bool bClipLocksMove = false;

	// input lock
	bool bInputLocked = false;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> BlockContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BlockAction;

	// overlay + imported assets
	TSharedPtr<SWidget> Overlay;
	bool bAssetsLoaded = false;
	TMap<FString, FString> Texts;
	TMap<FName, FSlateBrush> Brushes;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> GlyphTextures;
};
