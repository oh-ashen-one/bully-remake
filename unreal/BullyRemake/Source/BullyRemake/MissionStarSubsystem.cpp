// OWNER: campaign-spine
#include "MissionStarSubsystem.h"

#include "BullyCampaignSubsystem.h"
#include "BullyClockSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "MissionStar.h"
#include "MissionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyStar, Log, All);

UMissionStarSubsystem* UMissionStarSubsystem::Get(UWorld* World)
{
	return World ? World->GetSubsystem<UMissionStarSubsystem>() : nullptr;
}

bool UMissionStarSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer)) { return false; }
	const UWorld* W = Cast<UWorld>(Outer);
	return W && W->IsGameWorld() && W->GetNetMode() != NM_DedicatedServer && !FParse::Param(FCommandLine::Get(), TEXT("BRNoMission"));
}

TStatId UMissionStarSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMissionStarSubsystem, STATGROUP_Tickables);
}

void UMissionStarSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Harness suppression: any -BR* switch means a scripted test run whose determinism the stars would disturb --
	// except -BRCampaignSmoke, which is the star system's own test (its run also carries harmless flags like
	// -BRNoAmbient, so the exemption is on the smoke flag, not on the switch list).
	Smoke.bEnabled = FParse::Param(FCommandLine::Get(), TEXT("BRCampaignSmoke"));
	if (!Smoke.bEnabled)
	{
		TArray<FString> Tokens, Switches;
		FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);
		for (const FString& S : Switches)
		{
			if (S.StartsWith(TEXT("BR"), ESearchCase::IgnoreCase)) { bSuppressed = true; break; }
		}
	}
	if (UMissionSubsystem* M = UMissionSubsystem::Get(&InWorld))
	{
		M->OnMissionFinished.AddUObject(this, &UMissionStarSubsystem::OnMissionDone);
	}
	if (UBullyClockSubsystem* Clock = UBullyClockSubsystem::Get(&InWorld))
	{
		Clock->OnHourChanged.AddLambda([this](int32) { Refresh(); });
	}
	if (UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(&InWorld))
	{
		Camp->OnChanged.AddLambda([this]() { Refresh(); });
	}
	bSubscribed = true;
	UE_LOG(LogBullyStar, Display, TEXT("BR_STAR subsystem armed (suppressed=%d, campaign smoke=%d)"), bSuppressed ? 1 : 0, Smoke.bEnabled ? 1 : 0);
}

void UMissionStarSubsystem::Deinitialize()
{
	DespawnAll(TEXT("world teardown"));
	if (bSubscribed)
	{
		if (UMissionSubsystem* M = UMissionSubsystem::Get(GetWorld())) { M->OnMissionFinished.RemoveAll(this); }
		if (UBullyClockSubsystem* Clock = UBullyClockSubsystem::Get(GetWorld())) { Clock->OnHourChanged.RemoveAll(this); }
		if (UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(GetWorld())) { Camp->OnChanged.RemoveAll(this); }
	}
	Super::Deinitialize();
}

void UMissionStarSubsystem::OnMissionDone(const FMissionDef& M, bool bSuccess)
{
	Smoke.bHaveFinish = true;
	Smoke.FinishedId = M.Id;
	Smoke.bFinishedOk = bSuccess;
	Refresh();   // a mission ended: re-evaluate (pass may have unlocked new rows)
}

void UMissionStarSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	if (Smoke.bEnabled) { TickSmoke(DeltaTime); }
	if (World->GetTimeSeconds() >= NextEvalAt)
	{
		NextEvalAt = World->GetTimeSeconds() + 1.0;   // cadence covers mission-start despawns and sub-hour window edges
		Refresh();
	}
}

void UMissionStarSubsystem::Refresh()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(World);
	if (!Camp || !Camp->IsLoaded()) { return; }
	const UMissionSubsystem* M = UMissionSubsystem::Get(World);
	if (bSuppressed || (M && M->IsActive()) || !UGameplayStatics::GetPlayerPawn(World, 0))
	{
		if (M && M->IsActive()) { DespawnAll(TEXT("mission active")); }
		return;
	}

	for (const auto& Pair : Camp->GetMissions())
	{
		const FCampaignMission& Row = Pair.Value;
		if (!Row.bHasStarLocation) { continue; }
		const FName Id(*Row.Id);
		if (Row.StarArea != 0)
		{
			if (!LoggedSkips.Contains(Id))
			{
				LoggedSkips.Add(Id);
				UE_LOG(LogBullyStar, Display, TEXT("BR_STAR skip '%s': star is in interior area %d (v1 spawns area 0 only)"), *Row.Id, Row.StarArea);
			}
			continue;
		}
		const bool bWant = Camp->IsAvailable(Row.Id);
		const TWeakObjectPtr<AMissionStar>* Existing = Stars.Find(Id);
		if (bWant && (!Existing || !Existing->IsValid()))
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags |= RF_Transient;
			AMissionStar* S = World->SpawnActorDeferred<AMissionStar>(AMissionStar::StaticClass(), FTransform(Row.StarLocation));
			if (!S) { continue; }
			S->MissionId = Id;
			S->bGold = Row.Star != FName(TEXT("red"));
			S->FinishSpawning(FTransform(Row.StarLocation));
			Stars.Add(Id, S);
		}
		else if (!bWant && Existing && Existing->IsValid())
		{
			UE_LOG(LogBullyStar, Display, TEXT("BR_STAR despawn '%s' (no longer available)"), *Row.Id);
			(*Existing)->Destroy();
			Stars.Remove(Id);
		}
	}
}

void UMissionStarSubsystem::DespawnAll(const TCHAR* Why)
{
	if (Stars.Num() == 0) { return; }
	UE_LOG(LogBullyStar, Display, TEXT("BR_STAR despawn all (%s)"), Why);
	for (auto& Pair : Stars)
	{
		if (Pair.Value.IsValid()) { Pair.Value->Destroy(); }
	}
	Stars.Reset();
}

AMissionStar* UMissionStarSubsystem::FindStar(FName MissionId) const
{
	const TWeakObjectPtr<AMissionStar>* S = Stars.Find(MissionId);
	return (S && S->IsValid()) ? S->Get() : nullptr;
}

void UMissionStarSubsystem::TeleportPlayer(const FVector& Loc)
{
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Pawn) { return; }
	Pawn->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
	if (ACharacter* C = Cast<ACharacter>(Pawn))
	{
		if (C->GetCharacterMovement()) { C->GetCharacterMovement()->StopMovementImmediately(); }
	}
}

// ---------------------------------------------------------------------------------------------------------------
// -BRCampaignSmoke: the campaign spine's headless verification (design doc "Verification").

void UMissionStarSubsystem::SmokeCheck(FName Name, bool bOk, const FString& Detail)
{
	++(bOk ? Smoke.Passes : Smoke.Fails);
	UE_LOG(LogBullyStar, Display, TEXT("BR_CAMPAIGN %s %s (%s)"), *Name.ToString(), bOk ? TEXT("PASS") : TEXT("FAIL"), *Detail);
}

void UMissionStarSubsystem::SmokeDone()
{
	Smoke.bDone = true;
	const bool bAll = Smoke.Fails == 0;
	UE_LOG(LogBullyStar, Display, TEXT("BR_CAMPAIGN SMOKE %s %d/%d (t=%.1fs)"), bAll ? TEXT("PASS") : TEXT("FAIL"), Smoke.Passes, Smoke.Passes + Smoke.Fails, Smoke.Elapsed);
	if (UWorld* World = GetWorld()) { GEngine->Exec(World, TEXT("quit")); }
}

void UMissionStarSubsystem::TickSmoke(float Dt)
{
	if (Smoke.bDone) { return; }
	Smoke.Elapsed += Dt;
	UWorld* World = GetWorld();
	UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(World);
	UBullyClockSubsystem* Clock = UBullyClockSubsystem::Get(World);
	UMissionSubsystem* M = UMissionSubsystem::Get(World);
	if (!World || !Camp || !Clock || !M) { return; }
	if (Smoke.Elapsed > 420.0) { SmokeCheck(TEXT("timeout"), false, TEXT("overall time limit")); SmokeDone(); return; }
	const double InPhase = Smoke.Elapsed - Smoke.PhaseAt;
	const UMissionSubsystem::EState MState = M->GetState();

	// the star-started missions are driven by the real mission smoke driver in drive-current mode (DriveSmoke(""))

	switch (Smoke.Phase)
	{
	case 0:   // init: subsystems up, then boot the day_one chain through the mission smoke driver (no quit on done)
		if (Smoke.Elapsed > 2.0 && UGameplayStatics::GetPlayerPawn(World, 0))
		{
			UE_LOG(LogBullyStar, Display, TEXT("BR_CAMPAIGN smoke init: %d table rows, clock day %d %02d:%02d"),
				Camp->GetMissions().Num(), Clock->GetDay(), Clock->GetHour(), Clock->GetMinute());
			M->DriveSmoke(TEXT("day_one"), /*bQuitOnDone*/ false);
			Smoke.Phase = 1; Smoke.PhaseAt = Smoke.Elapsed;
		}
		return;
	case 1:   // wait for the boot chain to complete
		if (Smoke.FinishedId == TEXT("day_one")) { Smoke.Phase = 2; Smoke.PhaseAt = Smoke.Elapsed; }
		else if (InPhase > 240.0) { SmokeCheck(TEXT("1_boot_stars"), false, TEXT("day_one did not complete in 240 s")); Smoke.Phase = 6; Smoke.PhaseAt = Smoke.Elapsed; }
		return;
	case 2:   // check 1: the expected star set after the boot chain (1_02B available, 1_03 not yet)
		if (FindStar(TEXT("1_02B")) || InPhase > 5.0)
		{
			const bool bBootOk = Smoke.bFinishedOk;
			AMissionStar* Star = FindStar(TEXT("1_02B"));
			const bool bAvail103 = Camp->IsAvailable(TEXT("1_03"));
			SmokeCheck(TEXT("1_boot_stars"), bBootOk && Star && !bAvail103,
				FString::Printf(TEXT("day_one completed=%d, star 1_02B %s, 1_03 available=%d (want 0)"), bBootOk ? 1 : 0, Star ? TEXT("up") : TEXT("MISSING"), bAvail103 ? 1 : 0));
			if (Star)
			{
				TeleportPlayer(Star->GetActorLocation());
				Smoke.Phase = 3; Smoke.PhaseAt = Smoke.Elapsed;
			}
			else { SmokeCheck(TEXT("2_star_start_chain"), false, TEXT("no 1_02B star to walk into")); Smoke.Phase = 6; Smoke.PhaseAt = Smoke.Elapsed; }
		}
		return;
	case 3:   // check 2a: the 1_02B star starts its mission; the mission smoke driver (drive-current) runs it
	{
		if (!Smoke.bStarted102B)
		{
			if (MState == UMissionSubsystem::EState::Running && M->GetMission() && M->GetMission()->Id == TEXT("1_02B"))
			{
				Smoke.bStarted102B = true;
				M->DriveSmoke(TEXT(""), /*bQuitOnDone*/ false);   // drive whatever is running, across the chain
			}
			else if (InPhase > 15.0)
			{
				SmokeCheck(TEXT("2_star_start_chain"), false, TEXT("walking into the 1_02B star did not start the mission"));
				Smoke.Phase = 6; Smoke.PhaseAt = Smoke.Elapsed;
			}
			return;
		}
		if (Smoke.FinishedId == TEXT("1_02B"))
		{
			const bool bOk = Smoke.bFinishedOk;
			const bool bAvail = Camp->IsAvailable(TEXT("1_02C"));
			UE_LOG(LogBullyStar, Display, TEXT("BR_CAMPAIGN 1_02B finished ok=%d; 1_02C available=%d (chain next)"), bOk ? 1 : 0, bAvail ? 1 : 0);
			Smoke.Phase = 4; Smoke.PhaseAt = Smoke.Elapsed;
			Smoke.bStarted102B = false;
		}
		else if (InPhase > 150.0)
		{
			SmokeCheck(TEXT("2_star_start_chain"), false, TEXT("1_02B did not complete in 150 s"));
			Smoke.Phase = 6; Smoke.PhaseAt = Smoke.Elapsed;
		}
		return;
	}
	case 4:   // the original's 1_02B -> 1_02C handoff: wait for the chained mission (it can start AND finish within
	{         // a frame or two -- the driver left the player at the class door, 1_02C's own target)
		const bool bChainRunning = MState == UMissionSubsystem::EState::Running && M->GetMission() && M->GetMission()->Id == TEXT("1_02C");
		if (Smoke.FinishedId == TEXT("1_02C") || bChainRunning)
		{
			Smoke.Phase = 5; Smoke.PhaseAt = Smoke.Elapsed;
		}
		else if (InPhase > 15.0)
		{
			SmokeCheck(TEXT("2_star_start_chain"), false, TEXT("1_02B pass did not chain into 1_02C"));
			Smoke.Phase = 6; Smoke.PhaseAt = Smoke.Elapsed;
		}
		return;
	}
	case 5:   // 1_02C runs under the same driver (the chain kept it armed); its clock actions are part of check 2
		if (Smoke.FinishedId == TEXT("1_02C") || InPhase > 90.0)
		{
			const bool bChainOk = Smoke.FinishedId == TEXT("1_02C") && Smoke.bFinishedOk;
			const bool bClockOk = !Clock->IsPaused() && Clock->GetHour() == 9;
			SmokeCheck(TEXT("2_star_start_chain"), bChainOk && bClockOk,
				FString::Printf(TEXT("1_02B->1_02C chain completed=%d, clock %02d:%02d %s (want 09:xx running)"),
					bChainOk ? 1 : 0, Clock->GetHour(), Clock->GetMinute(), Clock->IsPaused() ? TEXT("paused") : TEXT("running")));
			Smoke.Phase = 6; Smoke.PhaseAt = Smoke.Elapsed;
		}
		return;
	case 6:   // check 3 setup: jump the clock to 23:00 (day-windowed stars must despawn)
		Smoke.bStar103AtEntry = FindStar(TEXT("1_03")) != nullptr;
		Clock->SetClock(23, 0);
		Smoke.Phase = 7; Smoke.PhaseAt = Smoke.Elapsed;
		return;
	case 7:   // after the jump: 1_03 gone; then pause the clock and confirm availability is stable under it
		if (InPhase > 0.6)
		{
			Smoke.bClockGone = !FindStar(TEXT("1_03")) && !Camp->IsAvailable(TEXT("1_03"));
			UE_LOG(LogBullyStar, Display, TEXT("BR_CAMPAIGN clock 23:00: 1_03 star %s, available=%d (want gone/0)"), FindStar(TEXT("1_03")) ? TEXT("STILL UP") : TEXT("despawned"), Camp->IsAvailable(TEXT("1_03")) ? 1 : 0);
			Clock->PauseClock();
			Smoke.Phase = 8; Smoke.PhaseAt = Smoke.Elapsed;
		}
		return;
	case 8:   // paused for 2 s: availability must not age
		if (InPhase > 2.0)
		{
			Smoke.bClockStable = !Camp->IsAvailable(TEXT("1_03")) && !FindStar(TEXT("1_03"));
			UE_LOG(LogBullyStar, Display, TEXT("BR_CAMPAIGN clock paused 2 s: 1_03 availability stable=%d"), Smoke.bClockStable ? 1 : 0);
			Clock->SetClock(8, 0);
			Clock->UnpauseClock();
			Smoke.Phase = 9; Smoke.PhaseAt = Smoke.Elapsed;
		}
		return;
	case 9:   // back to 8:00: the day-windowed star returns
		if (InPhase > 0.6)
		{
			const bool bBack = FindStar(TEXT("1_03")) && Camp->IsAvailable(TEXT("1_03"));
			SmokeCheck(TEXT("3_clock_windows"), Smoke.bStar103AtEntry && Smoke.bClockGone && Smoke.bClockStable && bBack,
				FString::Printf(TEXT("1_03 star: up before=%d, gone at 23:00=%d, stable paused=%d, back at 08:00=%d"),
					Smoke.bStar103AtEntry ? 1 : 0, Smoke.bClockGone ? 1 : 0, Smoke.bClockStable ? 1 : 0, bBack ? 1 : 0));
			Smoke.Phase = 10; Smoke.PhaseAt = Smoke.Elapsed;
		}
		return;
	case 10:  // check 4: pass side effects (force-complete path)
	{
		Camp->NotifyPassed(TEXT("1_09"));
		const bool bClockPaused = Clock->IsPaused();
		Camp->NotifyPassed(TEXT("1_B"));
		const bool bChapter = Camp->GetChapter() == 2;
		const bool bForced = Camp->IsCompleted(TEXT("1_11x2")) && Camp->IsCompleted(TEXT("C_Chem_1")) && Camp->IsCompleted(TEXT("C_Art_1"))
			&& Camp->IsCompleted(TEXT("C_Wrestling_1")) && Camp->IsCompleted(TEXT("C_English_1"));
		const bool bRespect = Camp->GetRespect(TEXT("bullies")) == 100 && Camp->GetRespect(TEXT("nerds")) == 85 && Camp->GetRespect(TEXT("jocks")) == 45;
		SmokeCheck(TEXT("4_pass_side"), bClockPaused && bChapter && bForced && bRespect,
			FString::Printf(TEXT("1_09 pass -> clock paused=%d; 1_B pass -> chapter=%d (want 2), force-completes=%d, respect bullies=%d nerds=%d jocks=%d"),
				bClockPaused ? 1 : 0, Camp->GetChapter(), bForced ? 1 : 0,
				Camp->GetRespect(TEXT("bullies")), Camp->GetRespect(TEXT("nerds")), Camp->GetRespect(TEXT("jocks"))));
		// check 5: the chapter-2 boss arc — 2_B's pass takes the campaign to chapter 3 (force-completing the
		// level-2 classes, preppies 100), and the Chapt2Trans handoff seeds the chapter-3 faction table
		Camp->NotifyPassed(TEXT("2_09"));
		Camp->NotifyPassed(TEXT("2_B"));
		const bool bCh3 = Camp->GetChapter() == 3;
		const bool bForced2 = Camp->IsCompleted(TEXT("C_Chem_2")) && Camp->IsCompleted(TEXT("C_Art_2")) && Camp->IsCompleted(TEXT("C_Wrestling_2"))
			&& Camp->IsCompleted(TEXT("C_English_2")) && Camp->IsCompleted(TEXT("C_Photography_1")) && Camp->IsCompleted(TEXT("C_Photography_2"));
		const bool bPrep100 = Camp->GetRespect(TEXT("preppies")) == 100;
		Camp->NotifyPassed(TEXT("Chapt2Trans"));
		const bool bFactions = Camp->GetRespect(TEXT("bullies")) == 100 && Camp->GetRespect(TEXT("nerds")) == 85 && Camp->GetRespect(TEXT("preppies")) == 100
			&& Camp->GetRespect(TEXT("greasers")) == 50 && Camp->GetRespect(TEXT("jocks")) == 45 && Camp->GetRespect(TEXT("townies")) == 0;
		SmokeCheck(TEXT("5_ch2_arc"), bCh3 && bForced2 && bPrep100 && bFactions,
			FString::Printf(TEXT("2_B pass -> chapter=%d (want 3), lvl-2 force-completes=%d, preppies=%d; Chapt2Trans -> faction table=%d"),
				Camp->GetChapter(), bForced2 ? 1 : 0, Camp->GetRespect(TEXT("preppies")), bFactions ? 1 : 0));
		Smoke.Phase = 11; Smoke.PhaseAt = Smoke.Elapsed;
		return;
	}
	case 11:
		SmokeDone();
		return;
	default:
		return;
	}
}
