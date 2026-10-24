// OWNER: sm-playloop
// Story mode: the "just launch it and play" loop around the campaign spine (docs/campaign-spine.md).
//
//   -BRStory                 player story mode. A title screen (New Story / Continue / Quit; keyboard, mouse and
//                            gamepad) opens over the world with the game paused. New Story runs arrival -> day_one and
//                            hands off to campaign free roam (mission stars); Continue restores the save and respawns
//                            the player in free roam. Ignored (logged) when any mission harness flag is present
//                            (-BRMission=, -BRMissionSmoke, -BRCampaignSmoke, -BRMissionShots), so those behave as before.
//   -BRStorySlot=<name>      save slot override (default "BullyStory"; the smoke uses "BullyStorySmoke").
//   -BRStorySmoke=shots      rendered evidence (tools/ue/run_story_shots.sh, offscreen, needs a smoke save): PNGs of the
//                            title screen, then the free-roam star + guidance HUD from 18 m and 5 m (UI included).
//   -BRStorySmoke=new|continue   headless verification (tools/ue/run_story_smoke.sh):
//                            new       title -> New Story -> the smoke driver plays arrival+day_one -> autosave written ->
//                                      free-roam guidance + the 1_02B star -> state mutated and written like Save & Quit.
//                            continue  title offers Continue -> restore -> chapter / completions / wallet / items / clock /
//                                      ammo / respect / flags / respawn spot restored, 1_02B star back, guidance shown.
//
// Save (BullySaveGame.h): autosave after every mission pass and chapter change (deferred until the mission machine is
// idle and no start_mission chain is pending, so a save is never taken between arrival and day_one or inside
// 1_02B -> 1_02C), and "Save & Quit" in the Esc menu. Mission-machine state is never saved.
//
// Free roam: with no mission running the mission HUD shows the next story objective ("Go to the yellow star: <title>") and
// points the existing waypoint at the nearest available gold star. A passed mission gets a "MISSION PASSED" banner with the
// reward line (money / respect / weapon unlocks).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BullyStorySubsystem.generated.h"

class UBullySaveGame;
class SWidget;
struct FMissionDef;
class UBullyCampaignSubsystem;

UCLASS()
class UBullyStorySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBullyStorySubsystem* Get(UWorld* World);
	/** The story flags are on the command line and no mission harness flag overrides them. */
	static bool IsStoryRunRequested();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	bool IsStoryMode() const { return bStory; }
	bool IsTitleOpen() const { return TitleWidget.IsValid(); }

	// --- title screen ---
	struct FTitleItem
	{
		enum class EKind : uint8 { Continue, NewStory, Quit };
		EKind Kind = EKind::NewStory;
		FString Label;
	};
	void OpenTitle();
	void CloseTitle();
	void TitleMove(int32 Dir);
	void TitleActivate();
	void TitleSelect(int32 Index);
	const TArray<FTitleItem>& GetTitleItems() const { return TitleItems; }
	int32 GetTitleSelected() const { return TitleSelected; }
	/** Second line under the menu (the save summary while Continue is highlighted). */
	FString GetTitleDetail() const;

	// --- flow ---
	void BeginNewStory();
	bool ContinueStory();

	// --- save ---
	FString SlotName() const;
	bool SaveExists() const;
	UBullySaveGame* LoadSave() const;
	/** Capture the current free-roam state (campaign, clock, wallet, arsenal, player spot) and write the slot. */
	bool SaveNow(const FString& Reason);
	/** The Esc-menu action. Mid-mission it keeps the last autosave (no mid-mission state is ever written). */
	bool SaveAndQuit(bool bQuit = true);
	int32 GetAutosaveCount() const { return AutosaveCount; }

private:
	UBullySaveGame* Capture(const FString& Reason, bool bWithPlayer) const;
	bool Apply(const UBullySaveGame* Save);
	bool WriteSlot(UBullySaveGame* Save);
	void RequestAutosave(const FString& Reason);
	void TickAutosave();
	void TickGuidance(float Dt);
	bool PickGuidance(FString& OutText, FVector& OutLoc, bool& bOutBlip) const;
	void OnMissionDone(const FMissionDef& M, bool bSuccess);
	void OnStageBegan(const FMissionDef& M, int32 Index);
	void QuitGame();

	// -BRStorySmoke
	void TickSmoke(float Dt);
	void SmokeCheck(const TCHAR* Name, bool bOk, const FString& Detail);
	void SmokeDone();

	bool bStory = false;
	bool bTitleDone = false;
	bool bSubscribed = false;
	double Elapsed = 0.0;
	TSharedPtr<SWidget> TitleWidget;
	bool bTitleOnViewport = false;   // false = logic-only title (no Slate / viewport)
	FString LastPassed;              // last mission passed (saved as LastMission)
	TArray<FTitleItem> TitleItems;
	int32 TitleSelected = 0;
	FString Slot;

	UPROPERTY(Transient) TObjectPtr<UBullySaveGame> LastSafe;   // last captured/loaded safe state (Save & Quit mid-mission keeps it)
	TSet<FName> StoryFlags;                                     // persistent "world_*" mission events
	bool bAutosaveDirty = false;
	FString AutosaveReason;
	int32 AutosaveCount = 0;
	int32 LastChapterSeen = 1;

	// free-roam guidance
	bool bGuiding = false;
	double NextGuidanceAt = 0.0;
	FString GuidanceText;
	FVector GuidanceLoc = FVector::ZeroVector;
	bool bGuidanceBlip = false;
	double AutosaveToastAt = -1.0;

	// per-mission reward snapshot (for the MISSION PASSED line)
	FString SnapMissionId;
	int32 SnapMoney = 0;
	int32 SnapWalletRespect = 0;
	TMap<FName, int32> SnapRespect;
	TSet<FName> SnapGranted;

	struct FSmoke
	{
		enum class EMode : uint8 { Off, New, Continue, Shots };
		EMode Mode = EMode::Off;
		int32 Phase = 0;
		double Elapsed = 0.0;
		double PhaseAt = 0.0;
		int32 Passes = 0, Fails = 0;
		bool bDone = false;
		bool bFinished = false;          // day_one passed (New)
		FString FinishedId;
		bool bFinishedOk = false;
		FVector ExpectLoc = FVector::ZeroVector;   // respawn spot the Continue run must restore
		int32 SavedMoney = 0;
		FString ShotDir;                 // shots mode: where the PNGs go
		double CompileIdleSince = -1.0;  // shots mode: shader/asset compilation drained at this time
		int32 ShotStep = 0;
		FVector ShotStar = FVector::ZeroVector;
	} Smoke;
};
