// OWNER: campaign-spine
// Campaign state + the Chapter 1 mission table (Config/Campaign.json). One row per mission id: title, chapter,
// star marker (gold = story, red = side; star_location optional — chained/auto missions have none), availability
// window on the game clock, prereq chain, and the on_pass block (the SMissPass.lua equivalent, in data):
//
//   { "_format": "bully-remake.campaign/1",
//     "clock": { "start_hour": 8, "start_minute": 30, "minutes_per_second": 1 },
//     "factions": ["bullies", "nerds", ...],
//     "missions": {
//       "1_03": { "title": "The Setup", "chapter": 1, "star": "gold",
//                 "star_location": { "area": 0, "location_cm": [x, y, z] },
//                 "window": [8, 19],
//                 "prereq": ["1_02C"],
//                 "on_pass": { "give_weapon": 303, "respect": { "bullies": 20 } } } } }
//
// Supported on_pass keys: give_weapon (int game id, or a Weapons.json string id), give_ammo ({id, n} or a list —
// tops the weapon's ammo up to n), respect ({faction: value} — v1 SET semantics, matching SFaction), force_complete
// ([ids], recurses through NotifyPassed), chapter_set (int), unlock_collectible (name -> flag), money (cents -> the
// persistent wallet, which counts whole dollars), clock ("pause" | "unpause" | "h:mm"). Unknown keys log a warning
// and never fail the mission.
//
// Availability (IsAvailable): the row's chapter matches the current chapter (chapter 0 = chapterless), every prereq
// is completed, the clock is inside the window, and the id is not already completed (v1: story missions are
// one-shot). Ids with no table row are outside the campaign and always available (sandbox missions, smoke missions).
//
// Console (testing): Campaign.Status | Campaign.Pass <id> | Campaign.Chapter <n> | Campaign.Respect <faction> <n>.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "BullyCampaignSubsystem.generated.h"

struct FCampaignMission
{
	FString Id;
	FString Title;
	int32 Chapter = 0;                    // 1-based externally; 0 = chapterless (any chapter)
	FName Star;                           // "gold" / "red" (NAME_None = no star marker)
	bool bHasStarLocation = false;
	int32 StarArea = 0;                   // original area code; v1 spawns area 0 (outdoors) only
	FVector StarLocation = FVector::ZeroVector;
	bool bHasWindow = false;
	float WindowStart = 0.f, WindowEnd = 0.f;   // fractional hours, [start, end)
	TArray<FString> Prereq;
	TSharedPtr<FJsonObject> OnPass;       // kept raw; applied key-by-key in NotifyPassed
};

UCLASS()
class UBullyCampaignSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UBullyCampaignSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool IsAvailable(const FString& Id) const;
	bool IsCompleted(const FString& Id) const { return Completion.Contains(Id); }
	int32 GetCompletionCount(const FString& Id) const { return Completion.FindRef(Id); }
	const FCampaignMission* FindMission(const FString& Id) const;
	int32 GetChapter() const { return Chapter; }
	int32 GetRespect(FName Faction) const { return Respect.FindRef(Faction); }
	int32 GetAttempts(const FString& Id) const { return Attempts.FindRef(Id); }
	bool IsLoaded() const { return bLoaded; }
	const TMap<FString, FCampaignMission>& GetMissions() const { return Missions; }

	/** The pass side (SMissPass equivalent): applies the row's on_pass block (force_complete recurses through this
	 *  same path), then bumps the completion count and broadcasts OnChanged so stars/HUD refresh. */
	void NotifyPassed(const FString& Id);
	/** Attempt counting per mission id (StartMission calls this; retry-skips-intro reads it once world-started). */
	void NoteAttempt(const FString& Id) { Attempts.FindOrAdd(Id) += 1; }
	void SetChapter(int32 NewChapter);
	void SetRespect(FName Faction, int32 Value);

	/** Weapon grants, shared by on_pass and the give_weapon/give_ammo mission actions. Numeric ids are the original
	 *  game ids, mapped to Weapons.json names (303 = slingshot; 306 super slingshot -> slingshot for now; 437
	 *  skateboard and 328 camera are logged only — a ride and an unbuilt system). give_ammo tops up to N. */
	void GiveWeaponByGameId(int32 GameId);
	void GiveWeaponByName(FName WeaponId);
	void GiveAmmoByGameId(int32 GameId, int32 N);
	void GiveAmmoByName(FName WeaponId, int32 N);

	/** Fires whenever completion/chapter/respect changes (the star subsystem refreshes off this). */
	DECLARE_MULTICAST_DELEGATE(FOnCampaignChanged);
	FOnCampaignChanged OnChanged;

private:
	void ApplyOnPass(const FCampaignMission& Row);
	void ApplyWeaponGrant(const TSharedPtr<FJsonValue>& V);
	void ApplyAmmoGrant(const TSharedPtr<FJsonValue>& V);

	TMap<FString, FCampaignMission> Missions;
	TMap<FString, int32> Completion;      // id -> pass count
	TMap<FString, int32> Attempts;        // id -> StartMission count
	TMap<FName, int32> Respect;           // faction -> value (set semantics)
	TSet<FName> Collectibles;             // unlock_collectible flags
	int32 Chapter = 1;
	TSet<FString> Passing;                // force_complete recursion guard
	bool bLoaded = false;
};
