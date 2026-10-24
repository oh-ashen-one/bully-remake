// OWNER: mission-fw
// Persistent player progress: the reward loop's wallet. Money and respect accumulate across missions and persist for
// the whole session (a GameInstance subsystem outlives individual worlds / level loads). Missions pay out through the
// mission framework's `reward` action; the mission HUD reads these to show a running total. The wallet also carries a
// light session item inventory (AddItem/TakeItem/GetItemCount) for the campaign-spine fetch/social beats (1_02B's
// chocolates, 1_G1's diary) — ids are free-form names, not the original's numeric item table. Nothing game-specific
// is hardcoded here beyond the two counters; it is the minimal economy primitive the objective->reward loop needs.
//
// Console (testing): Progress.Status  (log money/respect) | Progress.Money <n> (add) | Progress.Respect <n> (add) |
//                    Progress.Reset   (zero both, clear items).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BullyProgressSubsystem.generated.h"

class UBullyStorySubsystem;

UCLASS()
class UBullyProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	friend class UBullyStorySubsystem;   // story-mode save/load restores the private wallet/inventory

public:
	static UBullyProgressSubsystem* Get(const UObject* WorldContext);

	void AddMoney(int32 Delta);
	/** Top the wallet up to Minimum if it is below it (the original's money floor before a social beat). */
	void EnsureMoney(int32 Minimum);
	/** Take Amount from the wallet; false (and nothing taken) if it cannot cover it. */
	bool TakeMoney(int32 Amount);
	void AddRespect(int32 Delta);
	/** Session item inventory (mission fetch/social stubs; free-form ids). */
	void AddItem(FName Id, int32 N = 1);
	bool TakeItem(FName Id, int32 N = 1);
	int32 GetItemCount(FName Id) const { return Items.FindRef(Id); }
	const TMap<FName, int32>& GetItems() const { return Items; }
	void ResetProgress();

	int32 GetMoney() const { return Money; }
	int32 GetRespect() const { return Respect; }

	/** Fires whenever Money or Respect changes (the HUD listens to refresh its wallet readout). */
	DECLARE_MULTICAST_DELEGATE(FProgressChanged);
	FProgressChanged OnChanged;

private:
	int32 Money = 0;
	int32 Respect = 0;
	TMap<FName, int32> Items;
};
