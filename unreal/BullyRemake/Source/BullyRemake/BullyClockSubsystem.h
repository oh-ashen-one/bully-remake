// OWNER: campaign-spine
// The in-game world clock: hours/minutes/day, ticking at a configurable minutes-per-second rate (defaults come from
// the "clock" block of Config/Campaign.json — start 8:30, 1 game minute per real second). Missions drive it through
// the mission framework's `clock` action (set "h:mm" / pause / unpause) and the campaign table's on_pass effects;
// the HUD clock widget is a later wave. A paused clock simply stops advancing: availability windows are evaluated
// against the current (frozen) time, so pausing never expires a window — the Halloween night chain relies on that.
//
// Console (testing): Clock.Status | Clock.Set <h> <m> | Clock.Pause | Clock.Unpause.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "BullyClockSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnClockHourChanged, int32 /*NewHour*/);

class UBullyStorySubsystem;

UCLASS()
class UBullyClockSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	friend class UBullyStorySubsystem;   // story-mode save/load restores the private clock state

public:
	static UBullyClockSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !bPaused; }

	void PauseClock();
	void UnpauseClock();
	void SetClock(int32 Hour, int32 Minute);

	bool IsPaused() const { return bPaused; }
	int32 GetHour() const { return Hour; }
	int32 GetMinute() const { return Minute; }
	int32 GetDay() const { return Day; }
	/** Fractional hour (8:30 -> 8.5) — the value availability windows are tested against. */
	float GetHourFloat() const { return Hour + Minute / 60.f; }
	/** [StartH, EndH) on the fractional hour; StartH > EndH wraps midnight (20 -> 1 means 20:00 through 01:00). */
	bool IsInWindow(float StartH, float EndH) const;

	/** Fires when the hour rolls over (tick or SetClock); the star subsystem/HUD refresh off this. */
	FOnClockHourChanged OnHourChanged;

private:
	int32 Hour = 8;
	int32 Minute = 30;
	int32 Day = 1;
	float MinutesPerSecond = 1.f;
	bool bPaused = false;
	float MinuteCarry = 0.f;   // sub-minute remainder, game minutes
};
