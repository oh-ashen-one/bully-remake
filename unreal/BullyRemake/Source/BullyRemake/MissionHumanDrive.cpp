// OWNER: sm-humandrive
// HUMAN-MODE mission smoke driver (-BRMissionSmokeHuman, tools/ue/run_mission_smoke.sh --human).
//
// The normal smoke driver (UMissionSubsystem::TickSmoke) proves the mission scripts advance: it teleports onto every goal,
// lands hits straight through ApplyWeaponHit, hold-drives minigames and skips speech. This driver instead plays like a
// person with a pad: it only ever
//   - pushes the move stick (Enhanced Input IA_Move, camera-relative, plus IA_Sprint / IA_Jump) along a navmesh path toward
//     what the HUD is showing: the blip, the npc being chased / followed, the prompt's target, the fight;
//   - fights through the real bindings (IA_LockOn / IA_Block held, IA_Attack / IA_Grapple pressed, IA_WeaponNext to pull a
//     ranged weapon, camera aim for free-aimed shots);
//   - presses real keys through the player controller: E only while the interact prompt is on screen (and once to start a
//     minigame, then holds still in range), J/K/L for a dialogue choice, Enter on speech only with -BRMissionSmokeHumanSkipSpeech;
//   - otherwise waits speech, cutscene and timer beats out at game speed.
// It never teleports the player. The mission's own scripted teleports/fades still run, and a mission with no "start" block
// begins at its campaign star (where a player walking up to it would trigger it) -- that one placement happens before the
// mission starts and is logged.
//
// Navigation: the imported level has no navmesh, so before each mission starts the driver spawns NavMeshBoundsVolumes around
// clusters of the mission's points (Recast RuntimeGeneration=Dynamic, Config/DefaultEngine.ini) and waits for the build
// (that wait is excluded from the reported human minutes). Goals in another connected region (interior pockets) are reached
// through the ABullyDoorLink pair that connects the regions, if one does.
//
// Failure: no ground gained toward the current goal for 30 s (after replans, jumps and sidesteps), or 60 s with nothing a
// player could act on, logs `BR_MISSION_SMOKE human STUCK ...` with position/goal and the run FAILS. Real mission fails at
// human pace are counted (fail -> the mission's own retry); more than 4 on one stage FAILS the run.
#include "MissionSubsystem.h"
#include "BullyCampaignSubsystem.h"
#include "BullyCharacter.h"
#include "BullyCombatComponent.h"
#include "BullyCombatPawn.h"
#include "BullyDoorLink.h"
#include "BullyHUD.h"
#include "BullyProgressSubsystem.h"
#include "BullyScriptedNPC.h"
#include "BullyWeaponComponent.h"
#include "MissionTrigger.h"
#include "Components/BrushComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputKeyEventArgs.h"
#include "Camera/PlayerCameraManager.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "PhysicsEngine/BodySetup.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyHuman, Log, All);

namespace
{
	// combat-tutorial / fight events a stage can wait on (day_one's lesson chain, waves, KOs)
	bool IsFightEvent(const FString& E)
	{
		return E.StartsWith(TEXT("wave:")) || E == TEXT("block_success") || E == TEXT("combo_done") || E.StartsWith(TEXT("grapple_"))
			|| E == TEXT("ground_kick") || E == TEXT("enemy_beaten") || E == TEXT("enemy_knocked_out") || E == TEXT("humiliation_done");
	}
	void CollectEvents(const FMissionCondition& C, TArray<FString>& Out)
	{
		if (C.Type == FMissionCondition::EType::Event) { Out.Add(C.Name.ToString()); }
		for (const FMissionCondition& K : C.Children) { CollectEvents(K, Out); }
	}
	bool HasType(const FMissionCondition& C, FMissionCondition::EType T)
	{
		if (C.Type == T) { return true; }
		for (const FMissionCondition& K : C.Children) { if (HasType(K, T)) { return true; } }
		return false;
	}
	const FMissionCondition* FindType(const FMissionCondition& C, FMissionCondition::EType T)
	{
		if (C.Type == T) { return &C; }
		for (const FMissionCondition& K : C.Children) { if (const FMissionCondition* F = FindType(K, T)) { return F; } }
		return nullptr;
	}
	FVector Flat(const FVector& V) { return FVector(V.X, V.Y, 0.0); }
}

// ---------------------------------------------------------------------------------------------------------------
// input: everything goes through the player's real bindings

void UMissionSubsystem::HumanInject(FName Action, const FInputActionValue& V) const
{
	ABullyCharacter* P = Cast<ABullyCharacter>(GetPlayerPawn());
	APlayerController* PC = P ? Cast<APlayerController>(P->GetController()) : nullptr;
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Sub = LP ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP) : nullptr;
	const UInputAction* A = P ? P->GetCombatAction(Action) : nullptr;
	if (Sub && A) { Sub->InjectInputForAction(A, V, {}, {}); }
}

void UMissionSubsystem::HumanPressKey(const FKey& Key)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC) { return; }
	const FInputDeviceId Dev = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(PC->GetPlatformUserId());
	PC->InputKey(FInputKeyEventArgs(nullptr, Dev, Key, IE_Pressed, 1.f, false, FPlatformTime::Cycles64()));
	Human.Release.AddUnique(Key);
	Human.LastKeyAt = Smoke.Elapsed;
	UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human presses %s (stage %d)"), *Key.ToString(), StageIndex + 1);
}

// ---------------------------------------------------------------------------------------------------------------
// navigation: runtime navmesh around the mission's points

void UMissionSubsystem::HumanPrepareNav(const FMissionDef& D, const FVector& PlayerAt)
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	TArray<FVector> Pts;
	Pts.Add(PlayerAt);
	if (D.bHasStart) { Pts.Add(D.StartLocation); }
	for (const auto& Kv : D.Points) { Pts.Add(Kv.Value); }
	for (const FMissionTriggerDef& T : D.Triggers) { Pts.Add(T.Center); }
	for (const auto& Kv : D.Paths) { for (const FVector& W : Kv.Value) { Pts.Add(W); } }
	for (TActorIterator<ABullyDoorLink> It(World); It; ++It)   // door links near the mission's ground join its clusters below
	{
		for (const FVector& P : TArray<FVector>(Pts))
		{
			if (FVector::Dist2D(P, It->GetActorLocation()) < 15000.f) { Pts.Add(It->GetActorLocation()); Pts.Add(It->ExitLocation); break; }
		}
	}
	// greedy clustering: points within 120 m of a cluster's box join it (a route's points chain into one box)
	TArray<FBox> Boxes;
	for (const FVector& P : Pts)
	{
		if (P.ContainsNaN()) { continue; }
		bool bJoined = false;
		for (FBox& B : Boxes)
		{
			if (B.ExpandBy(FVector(12000.f, 12000.f, 3000.f)).IsInside(P)) { B += P; bJoined = true; break; }
		}
		if (!bJoined) { Boxes.Add(FBox(P, P)); }
	}
	// merge overlapping clusters until stable
	for (bool bMerged = true; bMerged;)
	{
		bMerged = false;
		for (int32 i = 0; i < Boxes.Num() && !bMerged; ++i)
		{
			for (int32 j = i + 1; j < Boxes.Num(); ++j)
			{
				if (Boxes[i].ExpandBy(FVector(12000.f, 12000.f, 3000.f)).Intersect(Boxes[j])) { Boxes[i] += Boxes[j]; Boxes.RemoveAt(j); bMerged = true; break; }
			}
		}
	}
	for (FBox& B : Boxes) { B = B.ExpandBy(FVector(4000.f, 4000.f, 1500.f)); }
	// skip clusters already covered by a previous mission's volumes
	int32 Spawned = 0;
	for (const FBox& B : Boxes)
	{
		bool bCovered = false;
		for (const FBox& Have : Human.NavBoxes) { if (Have.IsInside(B)) { bCovered = true; break; } }
		if (bCovered) { continue; }
		const FVector C = B.GetCenter();
		FActorSpawnParameters SP;
		SP.bDeferConstruction = true;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ANavMeshBoundsVolume* V = World->SpawnActor<ANavMeshBoundsVolume>(ANavMeshBoundsVolume::StaticClass(), FTransform(C), SP);
		if (!V) { continue; }
		UBrushComponent* BC = V->GetBrushComponent();
		UBodySetup* BS = NewObject<UBodySetup>(BC);
		const FVector Ext = B.GetExtent();
		BS->AggGeom.BoxElems.Add(FKBoxElem(Ext.X * 2.0, Ext.Y * 2.0, Ext.Z * 2.0));
		BC->BrushBodySetup = BS;
		V->FinishSpawning(FTransform(C));
		// the brush bounds are only right once the body setup is in place: refresh them and re-announce the volume
		BC->UpdateBounds();
		if (UNavigationSystemV1* NS = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)) { NS->OnNavigationBoundsUpdated(V); }
		Human.NavVolumes.Add(V);
		Human.NavBoxes.Add(B);
		++Spawned;
		UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human nav bounds %s size %.0fx%.0fx%.0f m (volume bounds %s)"), *C.ToCompactString(),
			Ext.X * 0.02, Ext.Y * 0.02, Ext.Z * 0.02, *V->GetComponentsBoundingBox(true).ToString());
	}
	Human.bNavWaiting = true;
	Human.NavWaitStart = Smoke.Elapsed;
	UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human nav: %d cluster(s), %d new volume(s) for '%s' -- waiting for the navmesh build"), Boxes.Num(), Spawned, *D.Id);
}

bool UMissionSubsystem::HumanNavReady()
{
	if (!Human.bNavWaiting) { return true; }
	UNavigationSystemV1* NS = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const double Waited = Smoke.Elapsed - Human.NavWaitStart;
	const bool bBuilding = NS && NS->IsNavigationBuildInProgress();
	const bool bHaveData = NS && NS->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) != nullptr;
	if (Waited < 2.0 || (bBuilding && Waited < 400.0) || (!bHaveData && Waited < 30.0))
	{
		static double LastNavLog = -100.0;
		if (Smoke.Elapsed - LastNavLog >= 10.0) { LastNavLog = Smoke.Elapsed; UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human nav: building (%.0f s, data %d, building %d)"), Waited, bHaveData ? 1 : 0, bBuilding ? 1 : 0); }
		return false;
	}
	Human.bNavWaiting = false;
	Human.NavWaitTotal += Waited;
	Human.MissionNavWait += Waited;
	UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human nav: ready after %.1f s (navdata %s, still building %d)"), Waited, bHaveData ? TEXT("yes") : TEXT("NO"), bBuilding ? 1 : 0);
	return true;
}

bool UMissionSubsystem::HumanFindPath(const FVector& From, const FVector& To, TArray<FVector>& Out, bool& bPartial) const
{
	Out.Reset();
	bPartial = true;
	UWorld* World = GetWorld();
	UNavigationSystemV1* NS = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NS) { return false; }
	FNavLocation A, B;
	if (!NS->ProjectPointToNavigation(From, A, FVector(200.f, 200.f, 300.f))) { return false; }
	// the goal: prefer the floor right under it (a point floating inside a prop / above a stairwell must not snap to the
	// storey above), then a wide projection
	bool bGoal = false;
	{
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BullyHumanGoalFloor), true, GetPlayerPawn());
		if (World->LineTraceSingleByObjectType(H, To + FVector(0, 0, 120.f), To - FVector(0, 0, 600.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
		{
			bGoal = NS->ProjectPointToNavigation(H.ImpactPoint, B, FVector(250.f, 250.f, 160.f));
		}
	}
	if (!bGoal && !NS->ProjectPointToNavigation(To, B, FVector(300.f, 300.f, 600.f))) { return false; }
	UNavigationPath* P = NS->FindPathToLocationSynchronously(World, A.Location, B.Location, GetPlayerPawn());
	if (!P || !P->IsValid() || P->PathPoints.Num() < 1) { return false; }
	Out = P->PathPoints;
	bPartial = P->IsPartial();
	return true;
}

float UMissionSubsystem::HumanSteer(const FVector& Goal, float ArriveCm, bool bSprint, bool bMovingGoal)
{
	ABullyCharacter* P = Cast<ABullyCharacter>(GetPlayerPawn());
	if (!P) { return 0.f; }
	const FVector Me = P->GetActorLocation();
	const float Dist = FVector::Dist2D(Me, Goal);
	// progress bookkeeping (stuck detection runs off this)
	if (FVector::Dist2D(Goal, Human.ProgGoal) > (bMovingGoal ? 800.f : 150.f))
	{
		Human.ProgGoal = Goal;
		Human.BestDist = Dist;
		Human.ProgressAt = Smoke.Elapsed;
		Human.PathAt = -100.0;   // new goal: replan now
	}
	else if (Dist < Human.BestDist - 100.f) { Human.BestDist = Dist; Human.ProgressAt = Smoke.Elapsed; }
	if (bMovingGoal) { Human.ProgGoal = Goal; }
	Human.IdleSince = -1.0;
	if (Dist <= ArriveCm && FMath::Abs(Me.Z - Goal.Z) < 400.f) { Human.ProgressAt = Smoke.Elapsed; return Dist; }

	// (re)plan: new goal, a moving goal every second, stale path every 4 s, or off the path
	const double Replan = bMovingGoal ? 1.0 : 4.0;
	if (Smoke.Elapsed - Human.PathAt > Replan || FVector::Dist2D(Goal, Human.PathGoal) > 200.f)
	{
		Human.PathAt = Smoke.Elapsed;
		Human.PathGoal = Goal;
		TArray<FVector> Pts;
		bool bPartial = true;
		Human.bPathOk = HumanFindPath(Me, Goal, Pts, bPartial);
		Human.bPathPartial = bPartial;
		// a goal in another region (interior pocket): head for the door link whose far side reaches it
		if (Human.bPathOk && bPartial && Pts.Num() > 0 && FVector::Dist2D(Pts.Last(), Goal) > 600.f)
		{
			for (TActorIterator<ABullyDoorLink> It(GetWorld()); It; ++It)
			{
				ABullyDoorLink* Partner = It->GetPartner();
				if (!Partner) { continue; }
				TArray<FVector> ToDoor, FromDoor;
				bool bP1 = true, bP2 = true;
				if (HumanFindPath(Me, It->GetActorLocation(), ToDoor, bP1) && !bP1 && HumanFindPath(Partner->ExitLocation, Goal, FromDoor, bP2) && !bP2)
				{
					Pts = ToDoor;
					Pts.Add(It->GetActorLocation());   // walk into the doorway trigger itself
					UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human route via door '%s' -> '%s' (goal %s in the other region)"), *It->LinkId.ToString(), *Partner->LinkId.ToString(), *Goal.ToCompactString());
					break;
				}
			}
		}
		Human.Path = Pts;
		Human.PathIdx = Pts.Num() > 1 ? 1 : 0;
		if (!Human.bPathOk) { Human.Path.Reset(); }
	}
	// pick the steering point
	FVector Aim = Goal;
	while (Human.Path.IsValidIndex(Human.PathIdx))
	{
		const FVector& Wp = Human.Path[Human.PathIdx];
		const bool bLast = Human.PathIdx == Human.Path.Num() - 1;
		if (FVector::Dist2D(Me, Wp) < (bLast ? 40.f : 90.f) && FMath::Abs(Me.Z - Wp.Z) < 250.f) { ++Human.PathIdx; continue; }
		Aim = Wp;
		break;
	}
	if (!Human.Path.IsValidIndex(Human.PathIdx) && Human.Path.Num() > 0 && Human.bPathPartial)
	{
		Aim = Human.Path.Last();   // partial path end reached: push on toward the goal (stuck detection judges it)
		if (FVector::Dist2D(Me, Aim) < 100.f) { Aim = Goal; }
	}

	// stuck recovery: sidestep for a moment after 6 s without progress, again after 12 s / 18 s (alternate sides)
	const double NoProg = Smoke.Elapsed - Human.ProgressAt;
	FVector Dir = Flat(Aim - Me).GetSafeNormal();
	if (Smoke.Elapsed < Human.RecoverUntil) { Dir = Human.RecoverDir; }
	else if (NoProg > 6.0 * (Human.Recoveries + 1) && Human.Recoveries < 4)
	{
		++Human.Recoveries;
		const float Side = (Human.Recoveries % 2) ? 1.f : -1.f;
		Human.RecoverDir = (FVector::CrossProduct(FVector::UpVector, Dir) * Side - Dir * 0.5f).GetSafeNormal();
		Human.RecoverUntil = Smoke.Elapsed + 1.2;
		Human.PathAt = -100.0;
		UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human no progress for %.0f s toward %s (%.0f cm): sidestep + replan (recovery %d)"), NoProg, *Goal.ToCompactString(), Dist, Human.Recoveries);
	}
	if (NoProg < 1.0) { Human.Recoveries = 0; }
	if (Dir.IsNearlyZero()) { return Dist; }

	// blocked (pushing but not moving): jump, like a player hopping a kerb / low wall
	const float Speed = P->GetHorizontalSpeed();
	if (Speed < 60.f && P->GetCharacterMovement() && P->GetCharacterMovement()->IsMovingOnGround())
	{
		if (Human.BlockedSince < 0.0) { Human.BlockedSince = Smoke.Elapsed; }
		if (Smoke.Elapsed - Human.BlockedSince > 0.6 && Smoke.Elapsed - Human.LastJump > 1.5)
		{
			Human.LastJump = Smoke.Elapsed;
			HumanInject(TEXT("jump"), FInputActionValue(true));
		}
	}
	else { Human.BlockedSince = -1.0; }

	// stick input, relative to the camera's yaw (what the player's thumb does)
	const AController* Ctl = P->GetController();
	const float CamYaw = Ctl ? Ctl->GetControlRotation().Yaw : 0.f;
	const FVector Fwd = FRotator(0.f, CamYaw, 0.f).Vector();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
	const FVector2D Stick(FVector::DotProduct(Dir, Right), FVector::DotProduct(Dir, Fwd));
	HumanInject(TEXT("move"), FInputActionValue(Stick.GetSafeNormal()));
	if (bSprint && Dist > 900.f) { HumanInject(TEXT("sprint"), FInputActionValue(true)); }
	return Dist;
}

// ---------------------------------------------------------------------------------------------------------------
// fighting through the real combat input

bool UMissionSubsystem::HumanFight(const FMissionStage& St)
{
	ABullyCharacter* P = Cast<ABullyCharacter>(GetPlayerPawn());
	UBullyCombatComponent* PC = P ? P->GetCombat() : nullptr;
	if (!P || !PC) { return false; }
	const FVector Me = P->GetActorLocation();
	TArray<FString> Evs;
	CollectEvents(St.Complete, Evs);
	bool bFightStage = false;
	for (const FString& E : Evs) { if (IsFightEvent(E)) { bFightStage = true; break; } }

	// candidate targets: this stage's wave members first, then any opposing fighter close by (spawn_enemy partners, ambushers)
	UBullyCombatComponent* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	bool bBestStationary = false;
	for (const FWaveGroup& G : Waves)
	{
		if (G.bFired) { continue; }
		for (const TWeakObjectPtr<ABullyCombatPawn>& M : G.Members)
		{
			ABullyCombatPawn* E = M.Get();
			UBullyCombatComponent* EC = E ? E->GetCombat() : nullptr;
			if (!EC || EC->IsOut() || EC->GetState() == EBullyCombatState::Beaten) { continue; }
			const float D = FVector::Dist(E->GetActorLocation(), Me);
			if (D < BestD) { BestD = D; Best = EC; bBestStationary = E->bStationary; }
		}
	}
	if (!Best)
	{
		for (const TWeakObjectPtr<UBullyCombatComponent>& W : UBullyCombatComponent::Registry())
		{
			UBullyCombatComponent* EC = W.Get();
			if (!EC || EC == PC || EC->GetWorld() != GetWorld() || EC->Team == PC->Team || EC->IsOut() || !EC->GetOwner()) { continue; }
			const float D = FVector::Dist(EC->GetOwner()->GetActorLocation(), Me);
			const bool bBeaten = EC->GetState() == EBullyCombatState::Beaten;
			if (bBeaten && !Evs.Contains(TEXT("humiliation_done"))) { continue; }
			// outside a fight stage: only defend against someone right on top of us who is swinging
			if (!bFightStage && (D > 300.f || EC->GetState() != EBullyCombatState::Attacking)) { continue; }
			if (D < BestD && D < (bFightStage ? 4000.f : 300.f)) { BestD = D; Best = EC; const ABullyCombatPawn* CP = Cast<ABullyCombatPawn>(EC->GetOwner()); bBestStationary = CP && CP->bStationary; }
		}
	}
	if (!Best)
	{
		HumanInject(TEXT("lockon"), FInputActionValue(false));
		return false;
	}
	AActor* TA = Best->GetOwner();
	const FVector TL = TA->GetActorLocation();
	const float D2 = FVector::Dist2D(TL, Me);
	const float Dz = TL.Z - Me.Z;
	UBullyWeaponComponent* Wpn = P->GetWeapons();

	// a stationary target we can't walk up to (up a wall, across a gap) or a stage that pins us in a zone: use a ranged weapon
	const FMissionCondition* Zone = FindType(St.Fail, FMissionCondition::EType::ZoneTimer);
	FName Ranged = NAME_None;
	if (Wpn)
	{
		for (const FBullyWeaponDef& Def : Wpn->GetDefs()) { if (!Def.bThrown && Wpn->GetAmmo(Def.Id) > 0) { Ranged = Def.Id; break; } }
	}
	bool bShoot = bBestStationary && !Ranged.IsNone() && (FMath::Abs(Dz) > 170.f || Zone != nullptr || D2 > 1500.f);
	if (bBestStationary && !Ranged.IsNone() && !bShoot && D2 > 400.f)
	{
		TArray<FVector> Tmp; bool bPartial = true;
		if (!HumanFindPath(Me, TL, Tmp, bPartial) || bPartial) { bShoot = true; }   // no walking route to it
	}
	if (bShoot)
	{
		// pull the weapon out (IA_WeaponNext until it is in hand)
		if (!Wpn->HasEquipped() || Wpn->GetEquipped()->Id != Ranged)
		{
			if (Smoke.Elapsed - Human.LastWeaponCycle > 0.4) { Human.LastWeaponCycle = Smoke.Elapsed; HumanInject(TEXT("weapon_next"), FInputActionValue(true)); }
			return true;
		}
		// aim: lock on when the target is inside lock range, else free-aim the camera's centre ray onto it (the mouse)
		const bool bLock = D2 < PC->LockOnRange * 0.95f && FMath::Abs(Dz) < 190.f;
		if (bLock) { HumanInject(TEXT("lockon"), FInputActionValue(true)); }
		APlayerController* Ctl = Cast<APlayerController>(P->GetController());
		bool bOnTarget = bLock && PC->IsLockedOn() && PC->GetLockTarget() == Best;
		if (!bLock && Ctl && Ctl->PlayerCameraManager)
		{
			const FVector Cam = Ctl->PlayerCameraManager->GetCameraLocation();
			const FVector Want = TL + FVector(0, 0, 40.f);
			const FRotator Rot = (Want - Cam).Rotation();
			const FRotator Cur = Ctl->GetControlRotation();
			const FRotator Delta = (Rot - Cur).GetNormalized();
			const float Step = 240.f * GetWorld()->GetDeltaSeconds();   // mouse turn rate
			Ctl->SetControlRotation(FRotator(Cur.Pitch + FMath::Clamp(Delta.Pitch, -Step, Step), Cur.Yaw + FMath::Clamp(Delta.Yaw, -Step, Step), 0.f));
			P->NoteLookInput();
			const FVector CamDir = Ctl->PlayerCameraManager->GetCameraRotation().Vector();
			bOnTarget = FVector::DotProduct(CamDir, (Want - Cam).GetSafeNormal()) > FMath::Cos(FMath::DegreesToRadians(1.5f));
		}
		if (bOnTarget && Smoke.Elapsed - Human.LastShot > 0.7)
		{
			Human.LastShot = Smoke.Elapsed;
			UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human fires %s at %s (%.0f cm, dz %.0f, %s)"), *Ranged.ToString(), *TA->GetName(), D2, Dz, bLock ? TEXT("locked on") : TEXT("free aim"));
			HumanInject(TEXT("attack"), FInputActionValue(true));
		}
		if (Zone)   // stay in the perch
		{
			const TWeakObjectPtr<AMissionTrigger>* T = Triggers.Find(Zone->Name);
			if (T && T->IsValid() && !(*T)->IsPlayerInside()) { HumanSteer((*T)->GetActorLocation(), 60.f, false, false); }
		}
		return true;
	}
	// melee: fists out
	if (Wpn && Wpn->HasEquipped())
	{
		if (Smoke.Elapsed - Human.LastWeaponCycle > 0.4) { Human.LastWeaponCycle = Smoke.Elapsed; HumanInject(TEXT("weapon_next"), FInputActionValue(true)); }
		return true;
	}
	// approach
	if (D2 > 400.f) { HumanSteer(TL, 110.f, D2 > 1200.f, true); return true; }
	if (D2 < 900.f) { HumanInject(TEXT("lockon"), FInputActionValue(true)); }
	Human.IdleSince = -1.0;
	Human.ProgressAt = Smoke.Elapsed;   // in the fight: progress is judged by the fight, not distance
	auto Close = [&](float Want)
	{
		if (D2 > Want && PC->CanMove())
		{
			const AController* Ctl = P->GetController();
			const float CamYaw = Ctl ? Ctl->GetControlRotation().Yaw : 0.f;
			const FVector Fwd = FRotator(0.f, CamYaw, 0.f).Vector();
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
			const FVector Dir = Flat(TL - Me).GetSafeNormal();
			HumanInject(TEXT("move"), FInputActionValue(FVector2D(FVector::DotProduct(Dir, Right), FVector::DotProduct(Dir, Fwd)).GetSafeNormal()));
			return false;
		}
		return true;
	};
	const double Since = Smoke.Elapsed - Human.LastAtk;
	auto Press = [&](FName A) { Human.LastAtk = Smoke.Elapsed; HumanInject(A, FInputActionValue(true)); };
	const EBullyCombatState S = PC->GetState();
	const EBullyCombatState ES = Best->GetState();
	const bool bIdle = S == EBullyCombatState::Idle || S == EBullyCombatState::Blocking;

	// day_one's taught moves: do what the on-screen lesson asks
	if (Evs.Contains(TEXT("block_success")))
	{
		Close(160.f);
		HumanInject(TEXT("block"), FInputActionValue(true));
		return true;
	}
	if (Evs.Contains(TEXT("combo_done")))
	{
		if (Human.AtkPhase == 0)
		{
			if (!bIdle || ES != EBullyCombatState::Idle) { return true; }
			if (!Close(110.f)) { return true; }
			if (Since > 0.5) { Press(TEXT("attack")); Human.AtkPhase = 1; }
		}
		else if (Human.AtkPhase == 1 && Since >= 0.2) { Press(TEXT("attack")); Human.AtkPhase = 2; }
		else if (Human.AtkPhase == 2 && Since >= 0.25) { Press(TEXT("attack")); Human.AtkPhase = 3; }
		else if (Human.AtkPhase == 3 && Since >= 1.6) { Human.AtkPhase = 0; }
		return true;
	}
	const bool bWantStrikes = Evs.Contains(TEXT("grapple_strikes_done"));
	const bool bWantPush = Evs.Contains(TEXT("grapple_done")) || (Evs.Contains(TEXT("ground_kick")) && ES != EBullyCombatState::Downed);
	if (Evs.Contains(TEXT("grapple_start")) || bWantStrikes || bWantPush)
	{
		if (S == EBullyCombatState::Grappling)
		{
			if (bWantStrikes && Since >= 0.3) { Press(TEXT("attack")); }
			else if (bWantPush && Since >= 0.4) { Press(TEXT("grapple")); }
			return true;
		}
		if (bIdle && ES == EBullyCombatState::Idle && Since > 0.5 && Close(100.f)) { Press(TEXT("grapple")); }
		return true;
	}
	if (Evs.Contains(TEXT("ground_kick")))
	{
		if (ES == EBullyCombatState::Downed && bIdle && Since > 0.4 && Close(140.f)) { Press(TEXT("attack")); }
		return true;
	}
	// free fight (combat test's player script): block telegraphed swings, close in, light chains, ground kicks, humiliate
	const FBullyCombatMove* EMv = Best->GetCurrentMove();
	if (EMv && EMv->bAutoChain && EMv->Name != Human.LastEnemyMove) { ++Human.EnemySwings; }
	Human.LastEnemyMove = EMv ? EMv->Name : NAME_None;
	const bool bThreat = EMv && D2 < 230.f && (EMv->bAutoChain || (EMv->ActiveStart >= 0.f && Best->GetStateTime() <= EMv->ActiveEnd));
	if (bThreat && bIdle) { HumanInject(TEXT("block"), FInputActionValue(true)); Human.AtkPhase = 1; return true; }
	if (S == EBullyCombatState::Attacking)
	{
		if (Human.AtkPhase == 2 && Since >= 0.2) { Press(TEXT("attack")); Human.AtkPhase = 3; }
		else if (Human.AtkPhase == 3 && Since >= 0.25) { Press(TEXT("attack")); Human.AtkPhase = 1; }
		return true;
	}
	if (!bIdle) { return true; }
	if (ES == EBullyCombatState::Beaten && Evs.Contains(TEXT("humiliation_done")))
	{
		if (Close(100.f) && Since > 0.5) { Press(TEXT("grapple")); }
		return true;
	}
	if (ES == EBullyCombatState::Downed)
	{
		if (Close(140.f) && Since > 0.4) { Press(TEXT("attack")); }
		return true;
	}
	if (!Close(115.f)) { return true; }
	if (Since > 0.35 && ES != EBullyCombatState::GettingUp) { Press(TEXT("attack")); Human.AtkPhase = 2; }
	return true;
}

FString UMissionSubsystem::HumanStuckReport(const TCHAR* Why) const
{
	const APawn* P = GetPlayerPawn();
	// forensics: the walkable floors stacked at the goal's XY and where the goal lands on the navmesh
	FString Floors;
	if (UWorld* W = GetWorld())
	{
		TArray<FHitResult> Hits;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BullyHumanFloors), true, P);
		FVector From = Human.ProgGoal + FVector(0, 0, 1500.f);
		for (int32 i = 0; i < 8; ++i)
		{
			FHitResult H;
			if (!W->LineTraceSingleByObjectType(H, From, Human.ProgGoal - FVector(0, 0, 1500.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q)) { break; }
			Floors += FString::Printf(TEXT("%s%.0f(%s)"), Floors.IsEmpty() ? TEXT("") : TEXT(","), H.ImpactPoint.Z, *GetNameSafe(H.GetActor()).Left(28));
			From = H.ImpactPoint - FVector(0, 0, 5.f);
		}
		if (UNavigationSystemV1* NS = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W))
		{
			FNavLocation NL;
			Floors += NS->ProjectPointToNavigation(Human.ProgGoal, NL, FVector(300.f, 300.f, 600.f)) ? FString::Printf(TEXT(" | navmesh at goal: %s"), *NL.Location.ToCompactString()) : FString(TEXT(" | goal NOT on navmesh"));
		}
	}
	FVector Blip;
	const bool bBlip = StageTarget(Blip);
	const FMissionStage* St = Mission.IsValid() && Mission->Stages.IsValidIndex(StageIndex) ? &Mission->Stages[StageIndex] : nullptr;
	return FString::Printf(TEXT("%s: mission '%s' stage %d '%s' (%s) pos=%s goal=%s (%s) blip=%s dist=%.0f cm, path %s%s, no progress %.0f s"),
		Why, Mission.IsValid() ? *Mission->Id : TEXT("-"), StageIndex + 1, St ? *St->Id.ToString() : TEXT("-"), St ? *St->Objective : TEXT(""),
		P ? *P->GetActorLocation().ToCompactString() : TEXT("none"), *Human.ProgGoal.ToCompactString(), *Human.GoalWhy,
		bBlip ? *Blip.ToCompactString() : TEXT("none"), P ? FVector::Dist2D(P->GetActorLocation(), Human.ProgGoal) : -1.f,
		Human.bPathOk ? (Human.bPathPartial ? TEXT("PARTIAL") : TEXT("ok")) : TEXT("NONE"), *FString::Printf(TEXT(" (%d pts)"), Human.Path.Num()),
		Smoke.Elapsed - Human.ProgressAt) + TEXT(" | floors at goal (z top->down): ") + Floors;
}

// ---------------------------------------------------------------------------------------------------------------
// the driver

void UMissionSubsystem::TickSmokeHuman(float Dt)
{
	UWorld* World = GetWorld();
	if (Smoke.bDone || !World) { return; }
	Smoke.Elapsed += Dt;
	// release keys pressed on an earlier tick (a press is one frame down)
	if (Human.Release.Num() > 0 && Smoke.Elapsed - Human.LastKeyAt > 0.05)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			const FInputDeviceId Dev = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(PC->GetPlatformUserId());
			for (const FKey& K : Human.Release) { PC->InputKey(FInputKeyEventArgs(nullptr, Dev, K, IE_Released, 0.f, false, FPlatformTime::Cycles64())); }
		}
		Human.Release.Reset();
	}
	auto MissionMinutes = [&]() { return (Smoke.Elapsed - Human.MissionStartAt - Human.MissionNavWait) / 60.0; };
	auto Quit = [&](bool bPass, const FString& Why)
	{
		Smoke.bDone = true;
		for (const FString& L : Human.Summary) { UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human summary: %s"), *L); }
		UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE %s: human: %s (stages seen %d/%d, fails %d, game time %.1f min excl. %.0f s navmesh build, t=%.1fs)"),
			bPass ? TEXT("PASS") : TEXT("FAIL"), *Why, Smoke.StagesSeen.Num(), Mission.IsValid() ? Mission->Stages.Num() : 0, Human.MissionFails,
			(Smoke.Elapsed - Human.NavWaitTotal) / 60.0, Human.NavWaitTotal, Smoke.Elapsed);
		GEngine->Exec(World, TEXT("quit"));
	};

	if (!Smoke.bStarted)
	{
		APawn* Pawn = GetPlayerPawn();
		if (Smoke.Elapsed <= 3.0 || !Pawn) { return; }
		if (!Human.bPreStartDone)
		{
			Human.bPreStartDone = true;
			FMissionDef D;
			FString Err;
			if (!LoadMission(Smoke.MissionId, D, Err)) { Quit(false, FString::Printf(TEXT("mission failed to load: %s"), *Err)); return; }
			// a mission without a "start" block begins where the player walks into its star
			FVector At = Pawn->GetActorLocation();
			if (!D.bHasStart)
			{
				const UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(World);
				const FCampaignMission* Row = Camp ? Camp->FindMission(D.Id) : nullptr;
				if (Row && Row->bHasStarLocation)
				{
					At = Row->StarLocation;
					TeleportPlayer(At + FVector(0, 0, 100.f), 0.f, false);
					UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human pre-start: player placed at the campaign star of '%s' %s (where walking into the star starts it)"), *D.Id, *At.ToCompactString());
				}
				else
				{
					UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human pre-start: '%s' has no start block and no star; the player begins at %s"), *D.Id, *At.ToCompactString());
				}
			}
			HumanPrepareNav(D, At);
			return;
		}
		if (!HumanNavReady()) { return; }
		Smoke.bStarted = true;
		Human.MissionNavWait = 0.0;   // the pre-start build is not part of the mission's minutes
		if (!StartMission(Smoke.MissionId, 0, /*bDevOverride*/ true)) { Quit(false, TEXT("mission failed to load")); return; }
		Smoke.bWantFailTest = false;
		return;
	}
	if (Smoke.Elapsed - Human.NavWaitTotal > Human.Limit) { Quit(false, FString::Printf(TEXT("time limit %.0f s"), Human.Limit)); return; }

	// mission bookkeeping (a chain swaps missions under us)
	if (Mission.IsValid() && Mission->Id != Human.LastMissionId && (State == EState::Running))
	{
		Human.LastMissionId = Mission->Id;
		Human.MissionStartAt = Smoke.Elapsed;
		Human.MissionNavWait = 0.0;
		Human.MissionFails = 0;
		Human.LastStage = -1;
		Smoke.StagesSeen.Reset();
		UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human mission '%s' begins (t=%.1fs)"), *Mission->Id, Smoke.Elapsed);
		if (APawn* Pw = GetPlayerPawn())
		{
			bool bCovered = false;
			for (const FBox& B : Human.NavBoxes) { if (B.IsInside(Pw->GetActorLocation())) { bCovered = true; break; } }
			bool bNeedNav = !bCovered;
			for (const auto& Kv : Mission->Points)
			{
				bool bIn = false;
				for (const FBox& B : Human.NavBoxes) { if (B.IsInside(Kv.Value)) { bIn = true; break; } }
				if (!bIn) { bNeedNav = true; break; }
			}
			if (bNeedNav) { HumanPrepareNav(*Mission, Pw->GetActorLocation()); }
		}
	}
	if (!HumanNavReady()) { return; }

	if (State == EState::Complete)
	{
		const FString Id = Mission.IsValid() ? Mission->Id : FString();
		if (Human.LastMissionId == Id && !Id.IsEmpty() && (Human.Summary.Num() == 0 || !Human.Summary.Last().StartsWith(Id + TEXT(" "))))
		{
			Human.Summary.Add(FString::Printf(TEXT("%s PASS %.2f min (stages %d/%d, fails %d, nav wait %.0f s excluded)"), *Id, MissionMinutes(), Smoke.StagesSeen.Num(), Mission->Stages.Num(), Human.MissionFails, Human.MissionNavWait));
			UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human mission '%s' PASS in %.2f min of game time (%d fails)"), *Id, MissionMinutes(), Human.MissionFails);
		}
		if (Human.bChain && !PendingAutoStart.IsEmpty()) { return; }   // chained hand-off pending: keep driving the next mission
		const bool bAll = Mission.IsValid() && Smoke.StagesSeen.Num() == Mission->Stages.Num();
		Quit(bAll, bAll ? TEXT("mission completed") : TEXT("completed without visiting every stage"));
		return;
	}
	if (State == EState::Failed)
	{
		if (!Human.bFailNoted)
		{
			Human.bFailNoted = true;
			++Human.MissionFails;
			++Human.StageFails;
			UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human FAILED stage %d '%s' at human pace (fail %d on this stage) at %s"), StageIndex + 1,
				Mission.IsValid() ? *Mission->Stages[StageIndex].Id.ToString() : TEXT("-"), Human.StageFails, GetPlayerPawn() ? *GetPlayerPawn()->GetActorLocation().ToCompactString() : TEXT("-"));
			if (Human.StageFails > 4) { Quit(false, HumanStuckReport(TEXT("stage failed 5 times at human pace"))); }
		}
		return;
	}
	Human.bFailNoted = false;
	if (State != EState::Running || !Mission.IsValid()) { return; }

	const FMissionStage& St = Mission->Stages[StageIndex];
	if (StageIndex != Human.LastStage || Human.LastStage < 0)
	{
		if (Human.LastStage >= 0 && Human.LastStage != StageIndex)
		{
			UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human stage %d done in %.1f s"), Human.LastStage + 1, Smoke.Elapsed - Human.StageStartAt);
		}
		if (Human.LastStage != StageIndex) { Human.StageFails = 0; }
		Human.LastStage = StageIndex;
		Human.StageStartAt = Smoke.Elapsed;
		Smoke.StagesSeen.AddUnique(StageIndex);
		Human.ProgressAt = Smoke.Elapsed;
		Human.BestDist = TNumericLimits<float>::Max();
		Human.ProgGoal = FVector(1e9, 1e9, 0);
		Human.IdleSince = -1.0;
		Human.AtkPhase = 0;
		Human.PathAt = -100.0;
		UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human stage %d/%d '%s' begins: \"%s\" (t=%.1fs)"), StageIndex + 1, Mission->Stages.Num(), *St.Id.ToString(), *St.Objective, Smoke.Elapsed);
	}
	ABullyCharacter* P = Cast<ABullyCharacter>(GetPlayerPawn());
	if (!P) { return; }
	const FVector Me = P->GetActorLocation();
	bool bActed = false;      // something a player does this tick
	bool bWaiting = false;    // a beat a player waits out (speech, timer, an npc walking, a fade)
	FString Why;

	// 1) dialogue choice on screen: press the key of the first option whose gate passes (greyed options are visible)
	if (Choice.bActive)
	{
		bActed = true; Why = TEXT("dialogue choice");
		if (Smoke.Elapsed - Human.LastKeyAt > 1.5)
		{
			UBullyProgressSubsystem* Prog = UBullyProgressSubsystem::Get(World);
			static const FKey Keys[3] = { EKeys::J, EKeys::K, EKeys::L };
			for (int32 i = 0; i < Choice.Options.Num() && i < 3; ++i)
			{
				const FChoiceOption& O = Choice.Options[i];
				if (O.RequiresItem.IsNone() || (Prog && Prog->GetItemCount(O.RequiresItem) > 0)) { HumanPressKey(Keys[i]); break; }
			}
		}
	}
	// 2) a fight lesson: perform the asked move with the attack button
	else if (Lesson.bActive)
	{
		bActed = true; Why = TEXT("lesson");
		if (Smoke.Elapsed - Human.LastAtk > 0.8 && P->GetCombat() && !P->GetCombat()->IsBusy())
		{
			Human.LastAtk = Smoke.Elapsed;
			if (Lesson.Move != P->GetCombat()->LightChainStart)
			{
				UE_LOG(LogBullyHuman, Warning, TEXT("BR_MISSION_SMOKE human lesson move %s is not what the attack button starts (%s)"), *Lesson.Move.ToString(), *P->GetCombat()->LightChainStart.ToString());
			}
			HumanInject(TEXT("attack"), FInputActionValue(true));
		}
	}
	// 3) a minigame station: walk into range, press E once, then hold still until the bar fills
	else if (Minigame.bArmed)
	{
		bActed = true; Why = TEXT("minigame");
		Human.GoalWhy = TEXT("minigame station");
		const float D = FVector::Dist2D(Me, Minigame.At);
		if (D > Minigame.Radius * 0.55f || FMath::Abs(Me.Z - Minigame.At.Z) > 350.f) { HumanSteer(Minigame.At, Minigame.Radius * 0.4f, false, false); }
		else
		{
			Human.ProgressAt = Smoke.Elapsed;
			if (!Minigame.bRunning && Smoke.Elapsed - Human.LastKeyAt > 1.0) { HumanPressKey(EKeys::E); }
		}
	}

	// 4) fights (wave stages, day_one's lessons, anyone swinging at us)
	if (!bActed && HumanFight(St)) { bActed = true; Why = TEXT("fight"); Human.GoalWhy = TEXT("fight"); }

	// 5) interact: press E when the prompt is up, else walk to its target
	if (!bActed)
	{
		TArray<const FMissionCondition*> Interacts;
		CollectInteracts(St.Complete, Interacts);
		for (const FMissionCondition* C : Interacts)
		{
			if (StageEvents.Contains(FName(*FString::Printf(TEXT("interact:%s"), *C->Name.ToString())))) { continue; }
			bActed = true; Why = TEXT("interact");
			const bool bPrompt = Hud.IsValid() && !Hud->GetState().PromptText.IsEmpty();
			if (bPrompt)
			{
				Human.ProgressAt = Smoke.Elapsed;
				if (Smoke.Elapsed - Human.LastKeyAt > 1.2)
				{
					UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human sees prompt '%s'"), *Hud->GetState().PromptText);
					HumanPressKey(EKeys::E);
				}
			}
			else
			{
				FVector T;
				if (ConditionTarget(*C, T))
				{
					Human.GoalWhy = FString::Printf(TEXT("interact %s"), *C->Name.ToString());
					HumanSteer(T, C->bActorTag ? FMath::Min(C->Radius * 0.6f, 150.f) : 50.f, true, C->bActorTag);
				}
				else { bActed = false; }
			}
			break;
		}
	}

	// 6) a chase (leash on the fail side): run after the npc until he's home, then the stage's own goal
	if (!bActed && St.Fail.IsValid())
	{
		if (const FMissionCondition* L = FindType(St.Fail, FMissionCondition::EType::Leash))
		{
			const FName ArrivedEv(*FString::Printf(TEXT("npc_arrived:%s"), *L->Name.ToString()));
			ABullyScriptedNPC* N = NpcById(L->Name);
			if (N && !StageEvents.Contains(ArrivedEv) && !MissionEvents.Contains(ArrivedEv))
			{
				bActed = true; Why = TEXT("chase");
				Human.GoalWhy = FString::Printf(TEXT("chasing %s"), *L->Name.ToString());
				HumanSteer(N->GetActorLocation(), 250.f, true, true);
			}
		}
	}

	// 7) the HUD blip (or the stage's positional goal)
	FVector Blip;
	if (!bActed && StageTarget(Blip))
	{
		const FMissionCondition* Reach = FindType(St.Complete, FMissionCondition::EType::Reach);
		const bool bReachedAlready = Evaluate(St.Complete);
		float Arrive = 80.f;
		if (Reach && FVector::Dist2D(Reach->Location, Blip) < 10.f) { Arrive = FMath::Max(60.f, Reach->Radius * 0.5f); }
		const float D = FVector::Dist2D(Me, Blip);
		if (!bReachedAlready && (D > Arrive || FMath::Abs(Me.Z - Blip.Z) > 400.f))
		{
			bActed = true; Why = TEXT("blip");
			Human.GoalWhy = TEXT("HUD blip");
			HumanSteer(Blip, Arrive, true, false);
		}
		else { bWaiting = true; Why = TEXT("at the blip, waiting"); }
	}
	// zone constraint (stay inside a perch) while waiting
	if (!bActed)
	{
		if (const FMissionCondition* Z = FindType(St.Fail, FMissionCondition::EType::ZoneTimer))
		{
			const TWeakObjectPtr<AMissionTrigger>* T = Triggers.Find(Z->Name);
			if (T && T->IsValid() && !(*T)->IsPlayerInside()) { bActed = true; Why = TEXT("back into the zone"); HumanSteer((*T)->GetActorLocation(), 60.f, false, false); }
		}
	}

	// 8) nothing to do: wait out speech / timers / npcs / fades -- or be stuck with no goal
	if (!bActed)
	{
		if (Speech.bActive)
		{
			bWaiting = true; Why = TEXT("speech");
			if (Human.bSkipSpeech && Smoke.Elapsed - Human.LastKeyAt > 1.5) { HumanPressKey(EKeys::Enter); }
		}
		else if (HasType(St.Complete, FMissionCondition::EType::Timer)) { bWaiting = true; Why = TEXT("timed beat"); }
		else if (Leads.Num() > 0 || Follows.Num() > 0 || PendingArrive.Num() > 0) { bWaiting = true; Why = TEXT("npc walking"); }
		if (Human.IdleSince < 0.0) { Human.IdleSince = Smoke.Elapsed; Human.IdleWhy = Why; }
		const double Idle = Smoke.Elapsed - Human.IdleSince;
		if (Idle > (bWaiting ? 90.0 : 60.0))
		{
			UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human STUCK %s"), *HumanStuckReport(bWaiting ? TEXT("waited 90 s with nothing happening") : TEXT("no actionable goal for 60 s (no blip / prompt / fight / speech)")));
			Quit(false, HumanStuckReport(TEXT("STUCK idle")));
			return;
		}
		Human.ProgressAt = Smoke.Elapsed;
	}
	else if (Smoke.Elapsed - Human.ProgressAt > Human.StuckSeconds)
	{
		UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human STUCK %s"), *HumanStuckReport(TEXT("no progress toward the goal")));
		Quit(false, HumanStuckReport(TEXT("STUCK")));
		return;
	}
	if (Smoke.Elapsed - Human.LastBeat > 10.0)
	{
		Human.LastBeat = Smoke.Elapsed;
		UE_LOG(LogBullyHuman, Display, TEXT("BR_MISSION_SMOKE human beat: stage %d '%s' doing [%s] pos=%s goal=%s (%s) d=%.0f speed=%.0f path=%s/%d t=%.0f"), StageIndex + 1, *St.Id.ToString(), *Why,
			*Me.ToCompactString(), *Human.ProgGoal.ToCompactString(), *Human.GoalWhy, FVector::Dist2D(Me, Human.ProgGoal), P->GetHorizontalSpeed(),
			Human.bPathOk ? (Human.bPathPartial ? TEXT("partial") : TEXT("ok")) : TEXT("none"), Human.Path.Num(), Smoke.Elapsed);
	}
}
