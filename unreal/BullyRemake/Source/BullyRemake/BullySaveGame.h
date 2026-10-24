// OWNER: sm-playloop
// Story-mode save game: everything the campaign needs to resume in free roam, and nothing that is mid-mission.
// Written by UBullyStorySubsystem (autosave after every mission pass / chapter change, "Save & Quit" in the Esc menu)
// to <Project>/Saved/SaveGames/<slot>.sav through the engine's USaveGame path (slot "BullyStory"; the headless story
// smoke uses "BullyStorySmoke" so it never touches a player's save).
//
// Captured: campaign completions + attempts, current chapter, per-faction respect, collectibles, granted weapons and
// persistent story flags; the game clock (day, time, paused); the wallet (money, respect) + item inventory; the
// player's weapon ammo table; and a respawn point (the spot the last mission was passed from, or the free-roam spot of
// a manual save) plus the id of the last passed mission. Mission-machine state (stage, flags, spawned npcs) is never
// saved: Continue always lands in free roam and the unlocked mission stars are re-spawned from the campaign table.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BullySaveGame.generated.h"

UCLASS()
class UBullySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;
	static const TCHAR* DefaultSlot() { return TEXT("BullyStory"); }

	UPROPERTY() int32 Version = CurrentVersion;
	UPROPERTY() FString SavedAtUtc;
	UPROPERTY() FString Reason;          // "autosave:<mission>" / "save_and_quit" / "smoke"
	UPROPERTY() FString LastMission;     // last mission passed

	// campaign
	UPROPERTY() int32 Chapter = 1;
	UPROPERTY() TMap<FString, int32> Completion;
	UPROPERTY() TMap<FString, int32> Attempts;
	UPROPERTY() TMap<FName, int32> Respect;
	UPROPERTY() TArray<FName> Collectibles;
	UPROPERTY() TArray<FName> GrantedWeapons;
	UPROPERTY() TArray<FName> StoryFlags;

	// clock
	UPROPERTY() int32 ClockDay = 1;
	UPROPERTY() int32 ClockHour = 8;
	UPROPERTY() int32 ClockMinute = 30;
	UPROPERTY() bool bClockPaused = false;

	// wallet + inventory + arsenal
	UPROPERTY() int32 Money = 0;
	UPROPERTY() int32 WalletRespect = 0;
	UPROPERTY() TMap<FName, int32> Items;
	UPROPERTY() TMap<FName, int32> WeaponAmmo;

	// respawn point (free roam)
	UPROPERTY() bool bHasRespawn = false;
	UPROPERTY() FVector RespawnLocation = FVector::ZeroVector;
	UPROPERTY() float RespawnYaw = 0.f;

	/** One line for the title screen's Continue entry. */
	FString Summary() const;
};
