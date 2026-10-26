// OWNER: builder-B4 (ambient)
#include "AmbientWalker.h"

#include "BullyCombatAI.h"
#include "BullyCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogAmbient, Log, All);

AAmbientWalker::AAmbientWalker()
{
	// No controller: the movement component must still run.
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	// Solid for the player: a normal Pawn capsule (Pawn blocks Pawn). Walker <-> walker passing is allowed per pair in
	// BeginPlay (IgnoreActorWhenMoving), NOT through a custom channel: the level's meshes answer some channels
	// differently, and a walker of its own object type was stopped at corners that Pawn capsules walk through.
	bSnapToGroundOnBeginPlay = true;
	bCameraAutoFollow = false;
}

void AAmbientWalker::BeginPlay()
{
	Super::BeginPlay();
	// ai-npc "fight anybody": students are valid combat targets (see StartBrawl in the header)
	if (UBullyCombatComponent* C = GetCombat())
	{
		C->Team = EBullyCombatTeam::Enemy;
	}
	// walkers pass through each other (both directions, every pair): see the class comment
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<AAmbientWalker> It(W); It; ++It)
		{
			if (*It != this)
			{
				GetCapsuleComponent()->IgnoreActorWhenMoving(*It, true);
				It->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);
			}
		}
	}
	SetScriptedGait(EBullyGait::Walk);   // WalkSpeed (set by the spawner) is the loop speed
	HomeWaypoints = Waypoints;           // sm-campus: CampusResume returns here
	NextIndex = Waypoints.Num() ? StartIndex % Waypoints.Num() : 0;
	LastPos = ProgressRefPos = GetActorLocation();
	ProgressRefTime = GetWorld()->GetTimeSeconds();
	UE_LOG(LogAmbient, Display, TEXT("BR_AMBIENT walker %s (%s) loop %s start %s -> wp %d of %d, speed %.0f cm/s"),
		*GetName(), *StudentId, *LoopId, *GetActorLocation().ToString(), NextIndex, Waypoints.Num(), WalkSpeed);
}

void AAmbientWalker::Advance()
{
	if (Waypoints.Num() == 0) { return; }
	NextIndex = (NextIndex + 1) % Waypoints.Num();
	if (NextIndex == StartIndex % Waypoints.Num()) { ++LoopsCompleted; }
}

void AAmbientWalker::StartBrawl()
{
	bBrawling = true;
	bChatHold = false;
	if (!Cast<ABullyCombatAIController>(GetController()))
	{
		if (UWorld* W = GetWorld())
		{
			if (ABullyCombatAIController* AI = W->SpawnActor<ABullyCombatAIController>())
			{
				AI->Possess(this);
				AI->AttackSoon(0.8f);   // a beat of outrage before swinging back
			}
		}
	}
	UE_LOG(LogAmbient, Display, TEXT("BR_AMBIENT BRAWL %s (%s): dropped its routine, fighting the player"), *GetName(), *StudentId);
}

void AAmbientWalker::ResumeLoop(const TArray<FVector>& Loop, int32 Start)
{
	Waypoints = Loop;
	HomeWaypoints = Loop;   // sm-campus
	StartIndex = Waypoints.Num() ? Start % Waypoints.Num() : 0;
	NextIndex = StartIndex;
	bIdling = false;
	YieldPhase = 0;
	LastPos = ProgressRefPos = GetActorLocation();
	ProgressRefTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	SetScriptedGait(EBullyGait::Walk);
	UE_LOG(LogAmbient, Display, TEXT("BR_AMBIENT walker %s resumed a loop of %d waypoints from wp %d (scripted -> ambient)"), *GetName(), Waypoints.Num(), NextIndex);
}

bool AAmbientWalker::TickYield(double Now, const FVector& Pos)
{
	static const bool bDisabled = FParse::Param(FCommandLine::Get(), TEXT("BRNoYield"));   // A/B switch for the logic runs
	if (bDisabled) { return false; }
	UWorld* W = GetWorld();
	const APawn* Player = W ? UGameplayStatics::GetPlayerPawn(W, 0) : nullptr;
	if (!Player || Player == this)
	{
		YieldPhase = 0;
		return false;
	}
	FVector ToPlayer = Player->GetActorLocation() - Pos;
	ToPlayer.Z = 0.f;
	const float Dist = ToPlayer.Size();
	MinPlayerDist = FMath::Min(MinPlayerDist, Dist);
	const FVector Away = Dist > 1.f ? -ToPlayer / Dist : FVector::ForwardVector;
	if (YieldPhase == 0)
	{
		if (Now < YieldCooldownUntil || Dist > YieldRadius)
		{
			return false;
		}
		FVector PV = Player->GetVelocity();
		PV.Z = 0.f;
		const float Closing = FVector::DotProduct(PV, Away);                  // player speed toward the walker
		FVector MyV = GetVelocity();
		MyV.Z = 0.f;
		const float Approach = FVector::DotProduct(MyV, -Away);              // walker speed toward the player
		const float Touch = 2.f * GetCapsuleComponent()->GetScaledCapsuleRadius() + 25.f;
		if (Dist > Touch && Closing < 60.f && Approach < 60.f)
		{
			return false;   // neither is closing in: leave it alone (a player standing next to a walker's path is fine)
		}
		// choose the side-step: perpendicular to the line to the player, toward the freer side
		const FVector Perp(-Away.Y, Away.X, 0.f);
		const float R = GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float HH = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		FVector Best = FVector::ZeroVector;
		for (const float Sign : { 1.f, -1.f })
		{
			const FVector D = Perp * Sign;
			FHitResult Hit;
			FCollisionQueryParams P(SCENE_QUERY_STAT(BullyWalkerYield), false, this);
			P.AddIgnoredActor(Player);
			const FVector From = Pos + FVector(0.f, 0.f, 45.f);   // lifted by the step height: kerbs are not walls
			const bool bBlocked = W->SweepSingleByChannel(Hit, From, From + D * 130.f, FQuat::Identity, ECC_Visibility,
				FCollisionShape::MakeCapsule(R, FMath::Max(R, HH - 45.f)), P);
			if (!bBlocked) { Best = D; break; }
		}
		YieldDir = Best;   // zero vector = nowhere to go: pause only
		YieldPhase = 1;
		YieldPhaseEnd = Now + YieldPauseSeconds;
		++YieldCount;
		UE_LOG(LogAmbient, Display, TEXT("BR_AMBIENT YIELD #%d %s at %s: player %.0f cm away (closing %.0f, approach %.0f) -> %s"),
			YieldCount, *GetName(), *Pos.ToString(), Dist, Closing, Approach, YieldDir.IsNearlyZero() ? TEXT("pause only") : *YieldDir.ToString());
	}
	if (YieldPhase == 1)
	{
		if (Now >= YieldPhaseEnd)
		{
			YieldPhase = YieldDir.IsNearlyZero() ? 0 : 2;
			YieldPhaseEnd = Now + YieldStepSeconds;
			if (YieldPhase == 0) { YieldCooldownUntil = Now + YieldCooldownSeconds; }
		}
		if (YieldPhase == 1)
		{
			return true;   // paused: no input
		}
	}
	if (YieldPhase == 2)
	{
		if (Now >= YieldPhaseEnd)
		{
			YieldPhase = 0;
			YieldCooldownUntil = Now + YieldCooldownSeconds;
			return false;
		}
		AddMovementInput(YieldDir, 1.f);
		return true;
	}
	return false;
}

void AAmbientWalker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);   // keeps the scripted gait applied
	UWorld* W = GetWorld();
	if (!W || Waypoints.Num() == 0) { return; }
	const double Now = W->GetTimeSeconds();
	const FVector Pos = GetActorLocation();
	PathWalked += FVector::Dist2D(Pos, LastPos);
	LastPos = Pos;

	// fight anybody: first health loss abandons the loop for the combat AI; while combat owns the body, no walking
	if (UBullyCombatComponent* C = GetCombat())
	{
		if (BrawlRefHealth < 0.f) { BrawlRefHealth = C->Health; }
		if (!bBrawling && C->Health < BrawlRefHealth)
		{
			// sm-campus: non-fighters run for it (and may be hit again); everybody else fights back
			const bool bFlee = bFleeWhenHit && !C->IsDown();
			if (bFlee) { BrawlRefHealth = C->Health; CampusFlee(W->GetFirstPlayerController() && W->GetFirstPlayerController()->GetPawn() ? W->GetFirstPlayerController()->GetPawn()->GetActorLocation() : Pos, 5.f); }
			else { StartBrawl(); }
			OnFirstHit.Broadcast(this, bFlee);
		}
		if (bBrawling || C->GetState() != EBullyCombatState::Idle)
		{
			ProgressRefPos = Pos;
			ProgressRefTime = Now;
			return;
		}
	}

	if (bChatHold)
	{
		// ai-npc conversation: stand still, turn to the player; held time is not a stuck candidate
		if (const APlayerController* PC = W->GetFirstPlayerController())
		{
			if (const APawn* P = PC->GetPawn())
			{
				const FVector To = P->GetActorLocation() - Pos;
				if (To.SizeSquared2D() > 1.f)
				{
					const FRotator Want(0.f, To.Rotation().Yaw, 0.f);
					SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, DeltaSeconds, 6.f));
				}
			}
		}
		ProgressRefPos = Pos;
		ProgressRefTime = Now;
		return;
	}

	// sm-campus: schedule errands, prefect chases, fleeing, conversation holds own the body while active
	if (TickCampus(DeltaSeconds, Now, Pos))
	{
		ProgressRefPos = Pos;
		ProgressRefTime = Now;
		return;
	}

	if (TickYield(Now, Pos))
	{
		// yielding: not a stuck candidate, idle timers keep running
		ProgressRefPos = Pos;
		ProgressRefTime = Now;
	}
	else if (bIdling)
	{
		if (Now >= IdleUntil)
		{
			bIdling = false;
			ProgressRefPos = Pos;
			ProgressRefTime = Now;
		}
	}
	else
	{
		const FVector Target = Waypoints[NextIndex];
		const FVector To = FVector(Target.X - Pos.X, Target.Y - Pos.Y, 0.f);
		if (To.Size() <= ArriveRadius)
		{
			++WaypointsReached;
			bIdling = true;
			IdleUntil = Now + FMath::FRandRange(IdleSecondsRange.X, IdleSecondsRange.Y);
			UE_LOG(LogAmbient, Display, TEXT("BR_AMBIENT %s reached wp %d (loops %d) t=%.1f"), *GetName(), NextIndex, LoopsCompleted, Now);
			Advance();
		}
		else
		{
			AddMovementInput(To.GetSafeNormal(), 1.f);
			// stuck detection: expected progress over StuckSeconds is far above this
			if (Now - ProgressRefTime >= StuckSeconds)
			{
				const float Moved = FVector::Dist2D(Pos, ProgressRefPos);
				if (Moved < 0.25f * WalkSpeed * StuckSeconds)
				{
					++StuckEvents;
					UE_LOG(LogAmbient, Warning, TEXT("BR_AMBIENT STUCK %s at %s moved %.0f cm in %.1f s heading to wp %d %s: skipping it"),
						*GetName(), *Pos.ToString(), Moved, StuckSeconds, NextIndex, *Target.ToString());
					Advance();
				}
				ProgressRefPos = Pos;
				ProgressRefTime = Now;
			}
		}
	}
	if (Now >= NextLog)
	{
		NextLog = Now + 5.0;
		UE_LOG(LogAmbient, Display, TEXT("BR_AMBIENT t=%.1f %s pos=(%.0f,%.0f,%.0f) speed=%.0f %s next=%d reached=%d loops=%d stuck=%d walked=%.1f m"),
			Now, *GetName(), Pos.X, Pos.Y, Pos.Z, GetHorizontalSpeed(), bIdling ? TEXT("idle") : TEXT("walk"), NextIndex, WaypointsReached, LoopsCompleted, StuckEvents, PathWalked / 100.f);
	}
}

void AAmbientWalker::EndPlay(const EEndPlayReason::Type Reason)
{
	UE_LOG(LogAmbient, Display, TEXT("BR_AMBIENT SUMMARY %s (%s) loop %s: waypoints reached %d, loops completed %d, stuck %d, yields %d, closest player %.0f cm, walked %.1f m"),
		*GetName(), *StudentId, *LoopId, WaypointsReached, LoopsCompleted, StuckEvents, YieldCount, MinPlayerDist, PathWalked / 100.f);
	Super::EndPlay(Reason);
}

// ---------------------------------------------------------------------------------------------------------------
// sm-campus: schedule / authority hooks (UBullyCampusSubsystem)

void AAmbientWalker::SetHiddenAndInert(bool bHide)
{
	SetActorHiddenInGame(bHide);
	SetActorEnableCollision(!bHide);
	SetActorTickEnabled(!bHide);
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->SetComponentTickEnabled(!bHide);
		if (bHide) { GetCharacterMovement()->StopMovementImmediately(); }
	}
	if (GetMesh()) { GetMesh()->SetComponentTickEnabled(!bHide); }
	if (UBullyCombatComponent* C = GetCombat())
	{
		C->SetComponentTickEnabled(!bHide);
		if (bHide || bBystander) { UBullyCombatComponent::Registry().Remove(C); }
		else { UBullyCombatComponent::Registry().AddUnique(C); }
	}
}

namespace { bool WantsHidden(bool bInside, bool bSupp, bool bDorm) { return bInside || bSupp || bDorm; } }

void AAmbientWalker::SetCampusSuppressed(bool bOn)
{
	if (bSuppressed == bOn) { return; }
	bSuppressed = bOn;
	SetHiddenAndInert(WantsHidden(IsInside(), bSuppressed, bDormant));
	UE_LOG(LogAmbient, Display, TEXT("BR_CAMPUS walker %s (%s) %s (mission uses this model)"), *GetName(), *StudentId, bOn ? TEXT("suppressed") : TEXT("restored"));
}

void AAmbientWalker::SetDormant(bool bOn)
{
	if (bDormant == bOn || bBrawling) { return; }
	bDormant = bOn;
	SetHiddenAndInert(WantsHidden(IsInside(), bSuppressed, bDormant));
}

void AAmbientWalker::SetBystander(bool bOn)
{
	if (bBystander == bOn) { return; }
	bBystander = bOn;
	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Cap->SetCollisionResponseToChannel(ECC_Pawn, bOn ? ECR_Overlap : ECR_Block);
	}
	if (UBullyCombatComponent* C = GetCombat())
	{
		if (bOn || WantsHidden(IsInside(), bSuppressed, bDormant)) { UBullyCombatComponent::Registry().Remove(C); }
		else { UBullyCombatComponent::Registry().AddUnique(C); }
	}
}

void AAmbientWalker::CampusGoTo(const FVector& Dest, bool bRun, bool bEnterOnArrive, const TArray<FVector>& Route)
{
	if (bBrawling || IsInside()) { return; }
	CampusMode = ECampusMode::Errand;
	ErrandDest = Dest;
	ErrandRoute = Route;
	bErrandRun = bRun;
	bErrandEnter = bEnterOnArrive;
	bErrandArrived = false;
	ErrandStucks = 0;
	bIdling = false;
	YieldPhase = 0;
	if (bRun)
	{
		if (SavedRunSpeed < 0.f) { SavedRunSpeed = RunSpeed; }
		RunSpeed = FMath::Min(CampusRunSpeed, 480.f);   // hurrying students jog, prefects chase faster (CampusChase)
	}
	SetScriptedGait(bRun ? EBullyGait::Run : EBullyGait::Walk);
	ErrandRefPos = GetActorLocation();
	ErrandRefTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void AAmbientWalker::CampusEnter()
{
	if (bBrawling) { return; }
	CampusMode = ECampusMode::Inside;
	bErrandArrived = true;
	SetHiddenAndInert(true);
	UE_LOG(LogAmbient, Display, TEXT("BR_CAMPUS walker %s (%s) went inside at %s"), *GetName(), *StudentId, *GetActorLocation().ToString());
}

void AAmbientWalker::CampusEmerge(const FVector& At)
{
	if (!IsInside()) { return; }
	CampusMode = ECampusMode::Loop;
	SetActorLocation(At + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
	SetHiddenAndInert(WantsHidden(false, bSuppressed, bDormant));
	CampusResume();
	UE_LOG(LogAmbient, Display, TEXT("BR_CAMPUS walker %s (%s) came out at %s"), *GetName(), *StudentId, *At.ToString());
}

void AAmbientWalker::CampusChase(AActor* Target)
{
	if (bBrawling || IsInside() || !Target) { return; }
	CampusMode = ECampusMode::Chase;
	ChaseTarget = Target;
	bIdling = false;
	if (SavedRunSpeed < 0.f) { SavedRunSpeed = RunSpeed; }
	RunSpeed = CampusRunSpeed;
	SetScriptedGait(EBullyGait::Run);
}

void AAmbientWalker::CampusFlee(const FVector& From, float Seconds)
{
	if (IsInside()) { return; }
	CampusMode = ECampusMode::Flee;
	FleeFrom = From;
	ModeUntil = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + Seconds;
	bIdling = false;
	if (SavedRunSpeed < 0.f) { SavedRunSpeed = RunSpeed; }
	RunSpeed = FMath::Min(CampusRunSpeed, 500.f);
	SetScriptedGait(EBullyGait::Run);
}

void AAmbientWalker::CampusHold(float Seconds, const AActor* FaceTo)
{
	if (bBrawling || IsInside() || CampusMode == ECampusMode::Chase || CampusMode == ECampusMode::Flee) { return; }
	if (CampusMode != ECampusMode::Hold) { ModeBeforeHold = CampusMode; }
	CampusMode = ECampusMode::Hold;
	HoldFace = FaceTo;
	ModeUntil = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + Seconds;
}

void AAmbientWalker::CampusResume()
{
	if (IsInside()) { return; }
	CampusMode = ECampusMode::Loop;
	ChaseTarget.Reset();
	if (SavedRunSpeed >= 0.f) { RunSpeed = SavedRunSpeed; SavedRunSpeed = -1.f; }
	if (HomeWaypoints.Num() > 0)
	{
		Waypoints = HomeWaypoints;
		const FVector P = GetActorLocation();
		int32 Best = 0;
		for (int32 i = 1; i < Waypoints.Num(); ++i)
		{
			if (FVector::DistSquared2D(Waypoints[i], P) < FVector::DistSquared2D(Waypoints[Best], P)) { Best = i; }
		}
		NextIndex = Best;
	}
	bIdling = false;
	YieldPhase = 0;
	SetScriptedGait(EBullyGait::Walk);
	LastPos = ProgressRefPos = GetActorLocation();
	ProgressRefTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

bool AAmbientWalker::TickCampus(float Dt, double Now, const FVector& Pos)
{
	switch (CampusMode)
	{
	case ECampusMode::Loop:
		return false;
	case ECampusMode::Inside:
		return true;
	case ECampusMode::Hold:
	{
		if (const AActor* F = HoldFace.Get())
		{
			const FVector To = F->GetActorLocation() - Pos;
			if (To.SizeSquared2D() > 1.f) { SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, To.Rotation().Yaw, 0.f), Dt, 6.f)); }
		}
		if (Now >= ModeUntil)
		{
			CampusMode = ModeBeforeHold == ECampusMode::Hold ? ECampusMode::Loop : ModeBeforeHold;
			if (CampusMode == ECampusMode::Errand) { ErrandRefPos = Pos; ErrandRefTime = Now; }
		}
		return true;
	}
	case ECampusMode::Flee:
	{
		FVector Away = Pos - FleeFrom;
		Away.Z = 0.f;
		AddMovementInput(Away.IsNearlyZero() ? GetActorForwardVector() : Away.GetSafeNormal(), 1.f);
		if (Now >= ModeUntil) { CampusResume(); }
		return true;
	}
	case ECampusMode::Chase:
	{
		AActor* T = ChaseTarget.Get();
		if (!T) { CampusResume(); return false; }
		FVector To = T->GetActorLocation() - Pos;
		To.Z = 0.f;
		if (To.Size() > 60.f) { AddMovementInput(To.GetSafeNormal(), 1.f); }
		return true;
	}
	case ECampusMode::Errand:
	{
		while (ErrandRoute.Num() > 0 && FVector::Dist2D(ErrandRoute[0], Pos) <= ArriveRadius + 60.f) { ErrandRoute.RemoveAt(0); ErrandStucks = 0; }
		FVector To = (ErrandRoute.Num() > 0 ? ErrandRoute[0] : ErrandDest) - Pos;
		To.Z = 0.f;
		if (ErrandRoute.Num() == 0 && To.Size() <= ArriveRadius + 20.f)
		{
			bErrandArrived = true;
			if (bErrandEnter) { CampusEnter(); }
			else { CampusResume(); }
			return true;
		}
		FVector Dir = To.GetSafeNormal();
		// Jimmy standing in the way: veer round him instead of pushing into him
		const UWorld* Wd = GetWorld();
		const APawn* Pl = Wd && Wd->GetFirstPlayerController() ? Wd->GetFirstPlayerController()->GetPawn() : nullptr;
		bool bPlayerNear = false;
		if (Pl && Pl != this)
		{
			FVector ToP = Pl->GetActorLocation() - Pos;
			ToP.Z = 0.f;
			bPlayerNear = ToP.Size() < 1500.f;
			if (ToP.Size() < 220.f && FVector::DotProduct(Dir, ToP.GetSafeNormal()) > 0.4f)
			{
				const float Side = FVector::CrossProduct(Dir, ToP).Z > 0.f ? -1.f : 1.f;
				Dir = Dir.RotateAngleAxis(70.f * Side, FVector::UpVector);
			}
		}
		AddMovementInput(Dir, 1.f);
		if (Now - ErrandRefTime >= StuckSeconds)
		{
			const float Moved = FVector::Dist2D(Pos, ErrandRefPos);
			const float Speed = bErrandRun ? RunSpeed : WalkSpeed;
			if (Moved < 0.25f * Speed * StuckSeconds)
			{
				++ErrandStucks;
				UE_LOG(LogAmbient, Display, TEXT("BR_CAMPUS errand STUCK %s at %s -> %s (%d)"), *GetName(), *Pos.ToString(), *ErrandDest.ToString(), ErrandStucks);
				if (ErrandStucks >= 2 && bPlayerNear && bErrandEnter && ErrandStucks < 4)
				{
					// in view of Jimmy: no vanishing on the spot, try once more from a step to the side
					AddMovementInput(Dir.RotateAngleAxis(90.f, FVector::UpVector), 1.f);
				}
				else if (ErrandStucks >= 2)
				{
					// a wall between the walker and its door: it goes in anyway (out of sight, or the student slips
					// through a side door); a non-entering errand just picks the home loop back up
					bErrandArrived = true;
					if (bErrandEnter) { CampusEnter(); } else { CampusResume(); }
					return true;
				}
			}
			ErrandRefPos = Pos;
			ErrandRefTime = Now;
		}
		return true;
	}
	}
	return false;
}
