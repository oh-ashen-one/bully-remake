// OWNER: sm-cutscenes
// Original (baked) cutscene playback from the importer's per-cutscene manifest
// (`python3 -m tools.importers cutscene build` -> imported/cutscenes/<id>/cutscene.json + <id>.wav; nothing derived from
// the game is in the repo). Two tiers, chosen per play from what is actually imported:
//   Full  -- every character actor's SkeletalMesh + AnimSequence is imported (/Game/Imported/Characters/<ACTOR>/...,
//            tools/ue/run_import_cutscene_actors.sh): the decoded camera track drives an owned camera, the actors are
//            spawned at their world origins and scrubbed to the cutscene clock, the player and the mission's pawns are
//            hidden, input is off; original audio + subtitles + letterbox.
//   Audio -- manifest + WAV but no actor assets: original audio + subtitles + letterbox over whatever camera the
//            mission has staged (the mission action runs its fallback's staging actions minus their text).
// Enter / gamepad right face button skips (after a short grace period). The done callback fires exactly once.
// Console: Cutscene.Play <id> [audio], Cutscene.Skip, Cutscene.Status.
// Offscreen evidence: -BRCutsceneShots=<id> -BRShotDir=<dir> [-BRCutsceneShotTimes=5,12.5,30] plays the cutscene once
// shaders are idle and grabs a HighResShot at each cutscene time, then quits (tools/ue/run_cutscene_shots.sh).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BullyCutscene.generated.h"

class FBullyHUD;
class ACameraActor;
class ASkeletalMeshActor;
class UAudioComponent;
class USoundWaveProcedural;
class UAnimSequence;
class USkeletalMesh;
class APawn;

/** One decoded cutscene (the importer's manifest, plus resolved assets). */
struct FBullyCutsceneData
{
	FString Id;
	FString Dir;                       // folder holding cutscene.json / <id>.wav
	float Duration = 0.f;
	FString WavPath;
	struct FSub { float Start = 0.f, End = 0.f; FString Text; };
	TArray<FSub> Subs;
	// camera baked at Hz: Pos/Target in UE cm, Fov degrees (4:3 horizontal), Roll degrees; Cut[i] = segment i->i+1 is a cut
	float CamHz = 30.f;
	TArray<FVector> CamPos, CamTarget;
	TArray<float> CamFov, CamRoll;
	TSet<int32> CamCuts;
	struct FActor
	{
		FString Name, Kind, MeshPath, AnimPath;
		FVector Offset = FVector::ZeroVector;   // UE cm
		bool bCharacter = true;
	};
	TArray<FActor> Actors;
	bool HasCamera() const { return CamPos.Num() > 1 && CamPos.Num() == CamTarget.Num(); }
};

UCLASS()
class UBullyCutsceneSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	enum class ETier : uint8 { None, Audio, Full };

	static UBullyCutsceneSubsystem* Get(UWorld* World);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** What can be played for this id right now (manifest + audio/subtitles = Audio; + every character asset = Full). */
	ETier Probe(const FString& Id);
	/** Starts the cutscene at the best available tier (bAllowFull=false forces Audio). Returns the tier played (None =
	 *  content absent: nothing started, OnDone NOT called). `Hud` may be null (a private overlay is made then). */
	ETier Play(const FString& Id, TSharedPtr<FBullyHUD> Hud, TFunction<void(bool /*bSkipped*/)> OnDone, bool bAllowFull = true);
	/** Ends the current cutscene now and calls its done callback (bSkipped = true). */
	void Skip();
	/** Tears down without calling the done callback (mission stop / retry). */
	void Stop();
	bool IsPlaying() const { return bPlaying; }
	float GetElapsed() const { return Elapsed; }
	const FString& GetCurrentId() const { return Cur.Id; }
	ETier GetCurrentTier() const { return CurTier; }

private:
	bool LoadData(const FString& Id, FBullyCutsceneData& Out) const;
	bool StartAudio();
	bool SpawnActors();
	void ApplyCamera(float T);
	void ApplyActors(float T);
	void ApplySubtitle(float T);
	void Finish(bool bSkipped, bool bCallDone);
	void HidePawns();
	void TickShots(float Dt);

	FBullyCutsceneData Cur;
	ETier CurTier = ETier::None;
	bool bPlaying = false;
	float Elapsed = 0.f;
	int32 SubIndex = -1;
	TSharedPtr<FBullyHUD> Hud;
	bool bOwnHud = false;
	TFunction<void(bool)> DoneFn;

	UPROPERTY() TObjectPtr<USoundWaveProcedural> Wave;
	UPROPERTY() TObjectPtr<UAudioComponent> AudioComp;
	UPROPERTY() TObjectPtr<ACameraActor> Cam;
	UPROPERTY() TArray<TObjectPtr<ASkeletalMeshActor>> ActorsSpawned;
	UPROPERTY() TArray<TObjectPtr<UAnimSequence>> ActorAnims;
	TWeakObjectPtr<AActor> PrevViewTarget;
	TArray<TWeakObjectPtr<AActor>> Hidden;
	bool bInputIgnored = false;

	// offscreen evidence harness (-BRCutsceneShots=<id>)
	struct FShots
	{
		bool bEnabled = false;
		FString Id, Dir;
		int32 ResX = 1920, ResY = 1080;
		TArray<float> Times;
		int32 Next = 0;
		int32 Hold = 0;                // frames rendered while held at the current shot time
		bool bStarted = false, bQuitting = false;
		double IdleSince = -1.0, ReadyAt = -1.0, QuitAt = -1.0, LastLog = -100.0;
		float Warmup = 3.f;
	} Shots;
};
