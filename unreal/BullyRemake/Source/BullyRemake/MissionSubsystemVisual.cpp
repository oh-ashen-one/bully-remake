// OWNER: sm-visual
// UMissionSubsystem, visual half (kept in its own translation unit so the stage machine file stays focused):
//   * camera "frame" mode -- a cutscene camera whose framing is solved at runtime from where the speakers actually
//     stand (npc ids, "player", "enemy"), with traces so it never parks inside a wall or behind one, and that keeps
//     re-aiming at them while it is live;
//   * shots-harness gates + metadata -- a rendered still only fires once the frame is what a player would see (no fade
//     running, Lumen and the pawn's landing settled after a teleport, the camera blend finished), and every still gets
//     a JSON sidecar line (camera, fade, letterbox, which characters are on screen) so framing can be checked by data.
#include "MissionSubsystem.h"

#include "BullyCombatComponent.h"
#include "BullyCombatPawn.h"
#include "BullyHUD.h"
#include "BullyScriptedNPC.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyMissionVisual, Log, All);

namespace
{
	/** Eye-ish point of a character (capsule top minus a head), else the actor origin lifted a little. */
	FVector SmvHead(const AActor* A)
	{
		FVector P = A->GetActorLocation();
		if (const ACharacter* C = Cast<ACharacter>(A))
		{
			if (const UCapsuleComponent* Cap = C->GetCapsuleComponent()) { P.Z += Cap->GetScaledCapsuleHalfHeight() * 0.62f; return P; }
		}
		P.Z += 60.f;
		return P;
	}

	FVector SmvFlat(const FVector& V) { return FVector(V.X, V.Y, 0.0); }

	bool SmvTraceStatic(const UWorld* W, const FVector& From, const FVector& To, FHitResult& Hit, const TArray<AActor*>& Ignore)
	{
		// camera channel (what the player's own boom collides with): trigger volumes and other query-only shapes don't
		// count; a trace that starts inside something (a head inside a hat brim, a tree canopy) is not a wall
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BullyFrameCam), true);
		Q.AddIgnoredActors(Ignore);
		TArray<FHitResult> Hits;
		W->LineTraceMultiByChannel(Hits, From, To, ECC_Camera, Q);
		for (const FHitResult& H : Hits)
		{
			if (H.bStartPenetrating || !H.bBlockingHit || Cast<APawn>(H.GetActor())) { continue; }
			Hit = H;
			return true;
		}
		return false;
	}

	/** How many of the subjects' heads a camera at CL/CR (horizontal fov, 16:9, letterboxed) actually shows unoccluded. */
	int32 SmvCountFramed(const UWorld* W, const FVector& CL, const FRotator& CR, double FovH, const TArray<AActor*>& Subjects, const TArray<AActor*>& Ignore)
	{
		const double T = FMath::Tan(FMath::DegreesToRadians(FovH * 0.5));
		const double TanH = T * 0.92, TanV = T * 9.0 / 16.0 * 0.80;   // inside the frame edge and the letterbox bars
		int32 N = 0;
		for (const AActor* S : Subjects)
		{
			if (!S) { continue; }
			const FVector H = SmvHead(S);
			const FVector L = CR.UnrotateVector(H - CL);
			FHitResult Hit;
			if (L.X > 10.0 && FMath::Abs(L.Y) < L.X * TanH && FMath::Abs(L.Z) < L.X * TanV && !SmvTraceStatic(W, CL, H, Hit, Ignore)) { ++N; }
		}
		return N;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// camera "frame" mode

bool UMissionSubsystem::ParseFrameCamera(const FJsonObject& O, FMissionAction& A)
{
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!O.TryGetArrayField(TEXT("frame"), Arr) || !Arr || Arr->Num() == 0) { return false; }
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		FString Id;
		if (V.IsValid() && V->TryGetString(Id) && !Id.IsEmpty()) { A.FrameIds.Add(FName(*Id)); }
	}
	if (A.FrameIds.Num() == 0) { return false; }
	double D = 0.0;
	if (O.TryGetNumberField(TEXT("fov"), D)) { A.Fov = float(D); }
	if (O.TryGetNumberField(TEXT("angle"), D)) { A.FrameAngle = float(D); }
	if (O.TryGetNumberField(TEXT("dist_cm"), D)) { A.FrameDist = FMath::Max(0.f, float(D)); }
	if (O.TryGetNumberField(TEXT("height_cm"), D)) { A.FrameHeight = float(D); }
	FString Side;
	if (O.TryGetStringField(TEXT("side"), Side)) { A.FrameSide = Side.Equals(TEXT("left"), ESearchCase::IgnoreCase) ? 1 : Side.Equals(TEXT("right"), ESearchCase::IgnoreCase) ? -1 : 0; }
	bool bFace = true;
	if (O.TryGetBoolField(TEXT("face"), bFace)) { A.bFrameFace = bFace; }
	return true;
}

bool UMissionSubsystem::ResolveFrameSubject(FName Id, AActor*& Out) const
{
	Out = nullptr;
	const FString S = Id.ToString();
	if (S.Equals(TEXT("player"), ESearchCase::IgnoreCase)) { Out = GetPlayerPawn(); return Out != nullptr; }
	if (S.Equals(TEXT("enemy"), ESearchCase::IgnoreCase))
	{
		// nearest live combat pawn to the player (spawn_enemy stand-ins such as arrival's prefect, wave members)
		const APawn* P = GetPlayerPawn();
		if (!P || !GetWorld()) { return false; }
		double Best = 3000.0 * 3000.0;
		for (TActorIterator<ABullyCombatPawn> It(GetWorld()); It; ++It)
		{
			ABullyCombatPawn* E = *It;
			if (!E || E == P || (E->GetCombat() && E->GetCombat()->IsOut())) { continue; }
			const double D2 = FVector::DistSquared(E->GetActorLocation(), P->GetActorLocation());
			if (D2 < Best) { Best = D2; Out = E; }
		}
		return Out != nullptr;
	}
	if (ABullyScriptedNPC* N = NpcById(Id)) { Out = N; return true; }
	return false;
}

bool UMissionSubsystem::SolveFrameCamera(const FMissionAction& A, const TArray<AActor*>& Subjects, FVector& OutLoc, FVector& OutLook, int32& OutVisible) const
{
	const UWorld* W = GetWorld();
	if (!W || Subjects.Num() == 0) { return false; }
	TArray<FVector> Heads;
	FVector M = FVector::ZeroVector;
	for (const AActor* S : Subjects) { Heads.Add(SmvHead(S)); M += Heads.Last(); }
	M /= double(Heads.Num());

	// axis: first subject (the speaker) -> last subject; one subject = his facing
	FVector U, N;
	double Span = 0.0;
	if (Subjects.Num() >= 2)
	{
		U = SmvFlat(Heads.Last() - Heads[0]).GetSafeNormal();
		for (int32 i = 0; i < Heads.Num(); ++i) { for (int32 j = i + 1; j < Heads.Num(); ++j) { Span = FMath::Max(Span, FVector::Dist2D(Heads[i], Heads[j])); } }
	}
	if (U.IsNearlyZero())
	{
		// single subject (or two on top of each other): put the camera in front of the speaker
		U = -SmvFlat(Subjects[0]->GetActorForwardVector()).GetSafeNormal();
		if (U.IsNearlyZero()) { U = FVector(1, 0, 0); }
	}
	N = FVector(-U.Y, U.X, 0.0);

	const double FovH = (A.Fov > 1.f ? A.Fov : 45.0);
	const double TanH = FMath::Tan(FMath::DegreesToRadians(FovH * 0.5));
	const double TanV = TanH * 9.0 / 16.0;
	const double Medium = 75.0 / TanV;                 // ~150 cm of height in frame: a waist-up two-shot
	const TArray<AActor*> Ignore = [&]() { TArray<AActor*> I = Subjects; if (APawn* P = GetPlayerPawn()) { I.AddUnique(P); } return I; }();

	// subjects too far apart for a readable two-shot (an npc calling across a yard): over the last subject's shoulder
	// (usually the player), looking at the speaker
	if (Subjects.Num() >= 2 && Span > 650.0 && A.FrameDist <= 0.f)
	{
		const FVector Shoulder = Heads.Last();
		FVector BestLoc = FVector::ZeroVector;
		double BestScore = -1e18;
		int32 BestVis = 0;
		for (int32 Side : { 1, -1 })
		{
			if (A.FrameSide != 0 && Side != A.FrameSide) { continue; }
			FVector Target = Shoulder + U * 190.0 + N * double(Side) * 75.0 + FVector(0, 0, 18.0);   // behind him (U points speaker -> him)
			FVector Loc = Target;
			double Got = FVector::Dist(Shoulder, Target);
			FHitResult Hit;
			if (SmvTraceStatic(W, Shoulder, Target, Hit, Ignore)) { Got = FMath::Max(0.0, double(Hit.Distance) - 30.0); Loc = Shoulder + (Target - Shoulder).GetSafeNormal() * Got; }
			const int32 Vis = SmvCountFramed(W, Loc, (Heads[0] - FVector(0, 0, 10.0) - Loc).Rotation(), FovH, { Subjects[0] }, Ignore);
			const double Score = Vis * 10000.0 + Got;
			if (Score > BestScore) { BestScore = Score; BestLoc = Loc; BestVis = Vis; }
		}
		if (BestScore > -1e17 && BestVis > 0)   // speaker not visible over the shoulder (around a corner): try a wide shot instead
		{
			OutLoc = BestLoc;
			OutLook = Heads[0] - FVector(0, 0, 10.0);
			OutVisible = BestVis + 1;
			return true;
		}
	}

	struct FCand { FVector Loc; int32 Visible = 0; double Score = -1e18; double Got = 0.0; };
	FCand Best;
	const double BaseAng = A.FrameAngle;
	const double Angles[] = { BaseAng, BaseAng + 15.0, BaseAng - 15.0, BaseAng + 35.0, 0.0 };
	const int32 Sides[] = { 1, -1 };
	for (int32 Side : Sides)
	{
		if (A.FrameSide != 0 && Side != A.FrameSide) { continue; }
		for (double Ang : Angles)
		{
			const double R = FMath::DegreesToRadians(Ang);
			// Subjects.Num()==1: U points from the subject toward the camera side already (in front of him)
			const FVector Dir = Subjects.Num() >= 2 ? (N * double(Side) * FMath::Cos(R) + U * FMath::Sin(R)).GetSafeNormal()
				: (U * FMath::Cos(R) + N * double(Side) * FMath::Sin(R)).GetSafeNormal();
			const double NeedH = (Span * FMath::Abs(FMath::Cos(R)) + 140.0) / (2.0 * TanH) / 0.8;
			const double Want = A.FrameDist > 0.f ? double(A.FrameDist) : FMath::Clamp(FMath::Max(Medium, NeedH), 200.0, 1600.0);
			FVector Target = M + Dir * Want;
			Target.Z = M.Z + (A.FrameHeight != 0.f ? A.FrameHeight : 10.0);
			// pull in against walls: never park the camera inside or behind geometry
			FHitResult Hit;
			double Got = Want;
			FVector Loc = Target;
			if (SmvTraceStatic(W, M, Target, Hit, Ignore))
			{
				Got = FMath::Max(0.0, double(Hit.Distance) - 35.0);
				Loc = M + (Target - M).GetSafeNormal() * Got;
			}
			const int32 Vis = SmvCountFramed(W, Loc, (M - FVector(0, 0, 15.0) - Loc).Rotation(), FovH, Subjects, Ignore);
			double Score = Vis * 10000.0 + FMath::Min(Got, Want) - FMath::Abs(Ang - BaseAng) * 3.0;
			if (Got < 140.0) { Score -= 5000.0; }   // a camera in someone's face reads as a clipping error
			if (Score > Best.Score) { Best.Loc = Loc; Best.Visible = Vis; Best.Score = Score; Best.Got = Got; }
		}
	}
	if (Best.Score <= -1e17) { return false; }
	// boxed in on every side (a speaker with his back to a wall): no cut at all beats a lens pressed into his face --
	// the caller stays on the gameplay camera
	if (Best.Got < 100.0 || Best.Visible == 0) { return false; }
	OutLoc = Best.Loc;
	OutLook = M - FVector(0, 0, 15.0);   // heads in the upper third
	OutVisible = Best.Visible;
	return true;
}

void UMissionSubsystem::ApplyFrameCamera(const FMissionAction& A)
{
	UWorld* World = GetWorld();
	APawn* Pawn = GetPlayerPawn();
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!World || !PC) { return; }
	TArray<AActor*> Subjects;
	for (const FName& Id : A.FrameIds)
	{
		AActor* S = nullptr;
		if (ResolveFrameSubject(Id, S)) { Subjects.AddUnique(S); }
		else { UE_LOG(LogBullyMissionVisual, Warning, TEXT("BR_MISSION camera frame: subject '%s' not found (skipped)"), *Id.ToString()); }
	}
	if (Subjects.Num() == 0) { Subjects.Add(Pawn); }
	// a subject still across the building (an npc who has not walked over yet) would drag the framing into a wide shot of
	// nobody: frame the ones actually in the scene (anchored on the player when he is a subject)
	if (Subjects.Num() >= 2)
	{
		const AActor* Anchor = Subjects.Contains(Pawn) ? static_cast<AActor*>(Pawn) : Subjects[0];
		for (int32 i = Subjects.Num() - 1; i >= 0 && Subjects.Num() > 1; --i)
		{
			if (Subjects[i] != Anchor && FVector::Dist(Subjects[i]->GetActorLocation(), Anchor->GetActorLocation()) > 1500.0)
			{
				UE_LOG(LogBullyMissionVisual, Display, TEXT("BR_MISSION camera frame: %s is %.0f m away -- left out of the shot"), *Subjects[i]->GetName(),
					FVector::Dist(Subjects[i]->GetActorLocation(), Anchor->GetActorLocation()) / 100.0);
				Subjects.RemoveAt(i);
			}
		}
	}

	// turn the speakers toward each other first, so the solved angle sees faces, not the backs of heads
	if (A.bFrameFace && Subjects.Num() >= 2)
	{
		const bool bPlayerIn = Subjects.Contains(Pawn);
		for (AActor* S : Subjects)
		{
			if (ABullyScriptedNPC* Npc = Cast<ABullyScriptedNPC>(S))
			{
				if (bPlayerIn) { Npc->FacePlayer(true); }
				else { Npc->FaceLocation(SmvHead(Subjects[0] == S ? Subjects[1] : Subjects[0]), false); }
			}
		}
		if (bPlayerIn)
		{
			const AActor* Other = Subjects[0] == Pawn ? Subjects[1] : Subjects[0];
			const FVector D = SmvFlat(Other->GetActorLocation() - Pawn->GetActorLocation());
			if (!D.IsNearlyZero())
			{
				const FRotator R(0.f, D.Rotation().Yaw, 0.f);
				Pawn->SetActorRotation(R);
				PC->SetControlRotation(R);
			}
		}
	}

	FVector Loc, Look;
	int32 Visible = 0;
	if (!SolveFrameCamera(A, Subjects, Loc, Look, Visible))
	{
		// no usable angle: a fresh camera beat stays on (or goes back to) the gameplay camera; a live re-solve holds the shot
		UE_LOG(LogBullyMissionVisual, Warning, TEXT("BR_MISSION camera frame: no usable angle -- %s"), bFrameResolving ? TEXT("holding the shot") : TEXT("staying on the gameplay camera"));
		if (bFrameResolving) { FrameLastSolveAt = World->GetTimeSeconds(); FrameSolvedVisible = FMath::Max(0, FrameLiveFramed); return; }
		if (CutsceneCam.IsValid()) { ReleaseCutsceneCamera(A.Blend); }
		return;
	}
	const FRotator Rot = (Look - Loc).Rotation();
	TArray<AActor*> FrIgnore = Subjects;
	FrIgnore.AddUnique(Pawn);
	const int32 Framed = SmvCountFramed(World, Loc, Rot, A.Fov > 1.f ? A.Fov : 45.0, Subjects, FrIgnore);
	if (bFrameResolving && FrameLiveFramed >= 0 && Framed <= FrameLiveFramed && CutsceneCam.IsValid())
	{
		// nothing better from here: hold the shot instead of cutting to an equal or worse one
		FrameSolvedVisible = FrameLiveFramed;
		FrameLastSolveAt = World->GetTimeSeconds();
		FrameCheckAt = FrameLastSolveAt + 0.3;
		UE_LOG(LogBullyMissionVisual, Display, TEXT("BR_MISSION camera frame: re-solve frames %d (live %d) -- holding the shot"), Framed, FrameLiveFramed);
		return;
	}
	ACameraActor* Cam = CutsceneCam.Get();
	if (!Cam)
	{
		FActorSpawnParameters SP; SP.ObjectFlags |= RF_Transient;
		Cam = World->SpawnActor<ACameraActor>(Loc, Rot, SP);
		CutsceneCam = Cam;
	}
	if (!Cam) { return; }
	Cam->SetActorLocationAndRotation(Loc, Rot);
	if (Cam->GetCameraComponent())
	{
		Cam->GetCameraComponent()->SetFieldOfView(A.Fov > 1.f ? A.Fov : 45.f);
		Cam->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	PC->SetViewTargetWithBlend(Cam, A.Blend, VTBlend_Cubic);
	FrameSubjects.Reset();
	FramePrevPos.Reset();
	for (AActor* S : Subjects) { FrameSubjects.Add(S); FramePrevPos.Add(S->GetActorLocation()); }
	FrameAction = MakeShared<FMissionAction>(A);
	FrameSolvedVisible = Framed;
	FrameLastSolveAt = World->GetTimeSeconds();
	FrameCheckAt = FrameLastSolveAt + FMath::Max(0.3, double(A.Blend) + 0.2);
	if (!bFrameResolving) { FrameResolves = 0; }
	FString Names;
	for (const FName& Id : A.FrameIds) { Names += (Names.IsEmpty() ? TEXT("") : TEXT(",")) + Id.ToString(); }
	UE_LOG(LogBullyMissionVisual, Display, TEXT("BR_MISSION camera frame [%s] at=%s look=%s fov=%.0f blend=%.2f visible=%d/%d framed=%d"),
		*Names, *Loc.ToCompactString(), *Look.ToCompactString(), A.Fov > 1.f ? A.Fov : 45.f, A.Blend, Visible, Subjects.Num(), Framed);
}

void UMissionSubsystem::TickCutsceneFrame(float Dt)
{
	if (FrameSubjects.Num() == 0) { return; }
	ACameraActor* Cam = CutsceneCam.Get();
	if (!Cam) { FrameSubjects.Reset(); return; }
	// a subject jumped (a fade teleport under black, an npc snapped to its spot): re-solve the framing with a hard cut
	bool bJump = false;
	for (int32 i = 0; i < FrameSubjects.Num() && i < FramePrevPos.Num(); ++i)
	{
		if (const AActor* S = FrameSubjects[i].Get())
		{
			if (FVector::Dist(S->GetActorLocation(), FramePrevPos[i]) > 250.0) { bJump = true; }
			FramePrevPos[i] = S->GetActorLocation();
		}
	}
	// ...or the shot went bad while live (an npc walked between the camera and the others, or right up to the lens, or a
	// subject walked out of frame): re-solve with a cut, at most once a second and a few times per camera action
	FString Bad;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (!bJump && FrameAction.IsValid() && Now >= FrameCheckAt && FrameResolves < 4 && Now - FrameLastSolveAt > 1.0)
	{
		FrameCheckAt = Now + 0.3;
		const FVector CL = Cam->GetActorLocation();
		TArray<AActor*> Subs;
		for (const TWeakObjectPtr<AActor>& S : FrameSubjects) { if (S.IsValid()) { Subs.Add(S.Get()); } }
		TArray<AActor*> Ignore = Subs;
		if (APawn* P = GetPlayerPawn()) { Ignore.AddUnique(P); }
		const int32 Vis = SmvCountFramed(GetWorld(), CL, Cam->GetActorRotation(), FrameAction->Fov > 1.f ? FrameAction->Fov : 45.0, Subs, Ignore);
		for (const AActor* S : Subs) { if (FVector::Dist(S->GetActorLocation(), CL) < 130.0) { Bad = FString::Printf(TEXT("%s is at the lens"), *S->GetName()); } }
		FrameLiveFramed = Bad.IsEmpty() ? Vis : -1;
		if (Bad.IsEmpty() && Vis < FrameSolvedVisible) { Bad = FString::Printf(TEXT("only %d/%d heads in frame"), Vis, FrameSolvedVisible); }
	}
	if ((bJump || !Bad.IsEmpty()) && FrameAction.IsValid())
	{
		FMissionAction Again = *FrameAction;
		Again.Blend = 0.f;
		Again.bFrameFace = bJump;   // after a teleport turn them to each other again; mid-scene leave the acting alone
		UE_LOG(LogBullyMissionVisual, Display, TEXT("BR_MISSION camera frame: %s -- re-solving"), bJump ? TEXT("a subject jumped") : *Bad);
		if (!bJump) { ++FrameResolves; } else { FrameLiveFramed = -1; }
		bFrameResolving = true;
		ApplyFrameCamera(Again);
		bFrameResolving = false;
		return;
	}
	FVector M = FVector::ZeroVector;
	int32 N = 0;
	for (const TWeakObjectPtr<AActor>& S : FrameSubjects) { if (S.IsValid()) { M += SmvHead(S.Get()); ++N; } }
	if (N == 0) { return; }
	M /= double(N);
	// far-apart subjects were framed over the last one's shoulder: keep aiming at the speaker, not the midpoint
	FVector Aim = M - FVector(0, 0, 15.0);
	if (FrameSubjects.Num() >= 2 && FrameSubjects[0].IsValid() && FrameSubjects.Last().IsValid()
		&& FVector::Dist2D(FrameSubjects[0]->GetActorLocation(), FrameSubjects.Last()->GetActorLocation()) > 650.0 && !(FrameAction.IsValid() && FrameAction->FrameDist > 0.f))
	{
		Aim = SmvHead(FrameSubjects[0].Get()) - FVector(0, 0, 10.0);
	}
	const FRotator Want = (Aim - Cam->GetActorLocation()).Rotation();
	Cam->SetActorRotation(FMath::RInterpTo(Cam->GetActorRotation(), Want, Dt, 3.f));
}

// ---------------------------------------------------------------------------------------------------------------
// shots harness: settle gates, travel-facing for driver hops, per-still metadata

void UMissionSubsystem::ShotsFaceTravel(const FVector& From, const FVector& To)
{
	APawn* Pawn = GetPlayerPawn();
	AController* Ctl = Pawn ? Pawn->GetController() : nullptr;
	const FVector D = SmvFlat(To - From);
	if (!Ctl || D.Size() < 150.0) { return; }
	// a walking player arrives facing where he was going; prefer looking at a mission npc close to the landing spot
	FVector Face = D;
	double Best = 1500.0;
	for (const auto& Pair : Npcs)
	{
		if (!Pair.Value.IsValid()) { continue; }
		const double Dd = FVector::Dist2D(Pair.Value->GetActorLocation(), To);
		if (Dd > 120.0 && Dd < Best) { Best = Dd; Face = SmvFlat(Pair.Value->GetActorLocation() - To); }
	}
	const FRotator R(0.f, Face.Rotation().Yaw, 0.f);
	Ctl->SetControlRotation(R);
	Pawn->SetActorRotation(R);
}

void UMissionSubsystem::ShotsAimAtAction()
{
	// A player looks at whoever he is dealing with. When the still would show none of the mission's characters (the
	// driver hopped him somewhere with his back to them), turn him toward the action and give the boom a beat to settle.
	if (CutsceneCam.IsValid()) { return; }
	APawn* Pawn = GetPlayerPawn();
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!PC || !World) { return; }
	FVector2D VP(1920.0, 1080.0);
	if (GEngine && GEngine->GameViewport) { GEngine->GameViewport->GetViewportSize(VP); }
	TArray<AActor*> Cands;
	for (TActorIterator<ABullyCombatPawn> It(World); It; ++It)
	{
		if (*It != Pawn && !(It->GetCombat() && It->GetCombat()->IsOut()) && FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation()) < 4000.0) { Cands.Add(*It); }
	}
	const int32 NumCombat = Cands.Num();
	for (const auto& Pair : Npcs) { if (Pair.Value.IsValid() && FVector::Dist(Pair.Value->GetActorLocation(), Pawn->GetActorLocation()) < 2500.0) { Cands.Add(Pair.Value.Get()); } }
	if (Cands.Num() == 0) { return; }
	for (const AActor* C : Cands)
	{
		FVector2D S;
		FHitResult Occ;
		const FVector CamLoc = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : SmvHead(Pawn);
		if (PC->ProjectWorldLocationToScreen(SmvHead(C), S, true) && S.X > VP.X * 0.05 && S.X < VP.X * 0.95 && S.Y > 0 && S.Y < VP.Y
			&& !SmvTraceStatic(World, CamLoc, SmvHead(C), Occ, { Pawn, const_cast<AActor*>(C) })) { return; }   // already in view (and not behind a wall)
	}
	// the action first (live wave members / defenders), else the nearest mission npc
	const AActor* Best = nullptr;
	double BestD = 1e18;
	for (int32 i = 0; i < Cands.Num(); ++i)
	{
		if (Best && i >= NumCombat && Best->IsA<ABullyCombatPawn>()) { break; }
		const double D = FVector::Dist(Cands[i]->GetActorLocation(), Pawn->GetActorLocation());
		if (D < BestD) { BestD = D; Best = Cands[i]; }
	}
	if (!Best) { return; }
	// the fight is out of sight behind a building (the driver parked him at the garage door while the wave stands in the
	// yard beyond): a player would walk over -- hop to a clear spot ~5 m short of it, on its side of the wall
	{
		FHitResult Wall;
		const FVector BH = SmvHead(Best);
		if (Best->IsA<ABullyCombatPawn>() && BestD > 700.0 && SmvTraceStatic(World, SmvHead(Pawn), BH, Wall, { Pawn, const_cast<AActor*>(Best) }))
		{
			const FVector Dir = SmvFlat(Pawn->GetActorLocation() - Best->GetActorLocation()).GetSafeNormal();
			for (double Ang : { 0.0, 35.0, -35.0, 70.0, -70.0, 110.0, -110.0 })
			{
				FVector Spot = Best->GetActorLocation() + Dir.RotateAngleAxis(Ang, FVector::UpVector) * 500.0 + FVector(0, 0, 60.0);
				FHitResult H;
				if (SmvTraceStatic(World, BH, Spot, H, { Pawn, const_cast<AActor*>(Best) })) { continue; }
				if (!World->FindTeleportSpot(Pawn, Spot, Pawn->GetActorRotation())) { continue; }
				SnapTeleportToFloor(Pawn, Spot);
				const FRotator R(0.f, SmvFlat(Best->GetActorLocation() - Spot).Rotation().Yaw, 0.f);
				Pawn->SetActorLocation(Spot, false, nullptr, ETeleportType::TeleportPhysics);
				Pawn->SetActorRotation(R);
				PC->SetControlRotation(FRotator(-8.f, R.Yaw, 0.f));
				ShotLastTeleportAt = World->GetTimeSeconds();
				UE_LOG(LogBullyMissionVisual, Display, TEXT("BR_MISSIONSHOT aim: %s is %.0f m away behind cover -- the player walks over (hop to %s)"), *Best->GetName(), BestD / 100.0, *Spot.ToCompactString());
				return;
			}
		}
	}
	const FVector D = SmvFlat(Best->GetActorLocation() - Pawn->GetActorLocation());
	if (D.IsNearlyZero()) { return; }
	PC->SetControlRotation(FRotator(-8.f, D.Rotation().Yaw, 0.f));
	ShotLastTeleportAt = FMath::Max(ShotLastTeleportAt, World->GetTimeSeconds() - ShotSettleAfterTeleport + 0.7);   // ~0.7 s for the boom
	UE_LOG(LogBullyMissionVisual, Display, TEXT("BR_MISSIONSHOT aim: no character in view -- the player turns toward %s"), *Best->GetName());
}

bool UMissionSubsystem::ShotReady(double Now, FString& OutWhy) const
{
	if (Now >= Shots.PendingAt + ShotMaxDefer) { OutWhy = TEXT("forced"); return true; }
	if (FadePhase != EFadePhase::None) { return false; }
	if (Now < ShotLastTeleportAt + ShotSettleAfterTeleport) { return false; }
	const APawn* Pawn = GetPlayerPawn();
	if (const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		if (PC->PlayerCameraManager && PC->PlayerCameraManager->PendingViewTarget.Target != nullptr) { return false; }   // still blending
	}
	OutWhy = TEXT("settled");
	return true;
}

void UMissionSubsystem::WriteShotMeta(const FString& File, const FString& Reason) const
{
	const UWorld* World = GetWorld();
	const APawn* Pawn = GetPlayerPawn();
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!World || !PC || !PC->PlayerCameraManager) { return; }
	const APlayerCameraManager* PCM = PC->PlayerCameraManager;
	const FVector CamLoc = PCM->GetCameraLocation();
	const FRotator CamRot = PCM->GetCameraRotation();
	FVector2D VP(1920.0, 1080.0);
	if (GEngine && GEngine->GameViewport) { GEngine->GameViewport->GetViewportSize(VP); }
	const float Letterbox = Hud.IsValid() ? Hud->GetState().LetterboxTarget : 0.f;

	auto Vec = [](const FVector& V) { return FString::Printf(TEXT("[%.0f,%.0f,%.0f]"), V.X, V.Y, V.Z); };
	FString Chars;
	auto AddChar = [&](const FString& Id, const AActor* A)
	{
		if (!A) { return; }
		const FVector H = SmvHead(A);
		const FVector Feet = A->GetActorLocation() - FVector(0, 0, 80.0);
		FVector2D SH, SF;
		const bool bH = PC->ProjectWorldLocationToScreen(H, SH, true);
		const bool bF = PC->ProjectWorldLocationToScreen(Feet, SF, true);
		const double Y0 = Letterbox > 0.5f ? 0.085 : 0.0;
		const bool bHeadIn = bH && SH.X >= 0 && SH.X <= VP.X && SH.Y >= VP.Y * Y0 && SH.Y <= VP.Y * (1.0 - Y0);
		FHitResult Hit;
		TArray<AActor*> Ign; Ign.Add(const_cast<AActor*>(A)); if (Pawn) { Ign.Add(const_cast<APawn*>(Pawn)); }
		const bool bOccluded = SmvTraceStatic(World, CamLoc, H, Hit, Ign);
		const FVector ToCam = SmvFlat(CamLoc - A->GetActorLocation()).GetSafeNormal();
		const double Facing = FVector::DotProduct(SmvFlat(A->GetActorForwardVector()).GetSafeNormal(), ToCam);   // 1 = faces the camera
		if (!Chars.IsEmpty()) { Chars += TEXT(","); }
		Chars += FString::Printf(TEXT("{\"id\":\"%s\",\"loc\":%s,\"dist\":%.0f,\"head_on_screen\":%s,\"head_xy\":[%.3f,%.3f],\"feet_xy\":[%.3f,%.3f],\"occluded\":%s,\"facing_cam\":%.2f}"),
			*Id, *Vec(A->GetActorLocation()), FVector::Dist(CamLoc, H), bHeadIn ? TEXT("true") : TEXT("false"),
			bH ? SH.X / VP.X : -1.0, bH ? SH.Y / VP.Y : -1.0, bF ? SF.X / VP.X : -1.0, bF ? SF.Y / VP.Y : -1.0,
			bOccluded ? TEXT("true") : TEXT("false"), Facing);
	};
	AddChar(TEXT("player"), Pawn);
	for (const auto& Pair : Npcs) { if (Pair.Value.IsValid()) { AddChar(Pair.Key.ToString(), Pair.Value.Get()); } }
	for (TActorIterator<ABullyCombatPawn> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (*It != Pawn && FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation()) < 3000.0) { AddChar(FString::Printf(TEXT("combat:%s"), *It->GetName()), *It); }
	}
	const AActor* VT = PCM->GetViewTarget();
	const FString Line = FString::Printf(TEXT("{\"file\":\"%s\",\"stage\":%d,\"t\":%.2f,\"reason\":\"%s\",\"cam\":%s,\"cam_rot\":[%.1f,%.1f],\"fov\":%.1f,\"view_target\":\"%s\",\"cutscene_cam\":%s,\"fade\":%.2f,\"letterbox\":%.2f,\"last_teleport_age\":%.2f,\"chars\":[%s]}\n"),
		*FPaths::GetCleanFilename(File), Shots.PendingStage + 1, World->GetTimeSeconds(), *Reason, *Vec(CamLoc), CamRot.Pitch, CamRot.Yaw, PCM->GetFOVAngle(),
		VT ? *VT->GetClass()->GetName() : TEXT("none"), CutsceneCam.IsValid() ? TEXT("true") : TEXT("false"),
		PCM->bEnableFading ? PCM->FadeAmount : 0.f, Letterbox, World->GetTimeSeconds() - ShotLastTeleportAt, *Chars);
	FFileHelper::SaveStringToFile(Line, *(Shots.Dir / TEXT("shots_meta.jsonl")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

// ---------------------------------------------------------------------------------------------------------------
// teleport floor snap (all teleports) + shots-pass boom clearance

void UMissionSubsystem::SnapTeleportToFloor(APawn* Pawn, FVector& At) const
{
	const ACharacter* C = Cast<ACharacter>(Pawn);
	const UWorld* W = GetWorld();
	if (!C || !W || !C->GetCapsuleComponent()) { return; }
	const double HH = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_WorldStatic);
	Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BullyTeleportFloor), false, Pawn);
	FHitResult Hit;
	// from one half-height above the point (never up through a ceiling) down past it: lift only, so the +100 cm driver
	// hops and mid-air trigger centres are left alone
	if (!W->LineTraceSingleByObjectType(Hit, At + FVector(0, 0, HH), At - FVector(0, 0, 300.0), Obj, Q) || Hit.bStartPenetrating) { return; }
	const double Want = Hit.ImpactPoint.Z + HH + 2.0;
	if (At.Z < Want)
	{
		UE_LOG(LogBullyMissionVisual, Display, TEXT("BR_MISSION teleport: lifted %.0f cm onto the floor (%s)"), Want - At.Z, Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("?"));
		At.Z = Want;
	}
}

void UMissionSubsystem::ShotsClearBoom()
{
	if (CutsceneCam.IsValid()) { return; }
	APawn* Pawn = GetPlayerPawn();
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!PC || !World) { return; }
	const USpringArmComponent* Boom = Pawn->FindComponentByClass<USpringArmComponent>();
	const double Arm = Boom ? FMath::Max(150.0, double(Boom->TargetArmLength)) : 450.0;
	const FVector Pivot = Boom ? Boom->GetComponentLocation() : SmvHead(Pawn);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BullyShotsBoom), false, Pawn);
	auto Clear = [&](double Yaw)
	{
		const FVector Back = Pivot - FRotator(-8.0, Yaw, 0.0).Vector() * Arm;
		FHitResult Hit;
		// npcs count here: a boom pressed into the back of the npc he is talking to is just as useless
		return World->SweepSingleByChannel(Hit, Pivot, Back, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.f), Q) ? double(Hit.Distance) : Arm;
	};
	const double Yaw0 = PC->GetControlRotation().Yaw;
	const double C0 = Clear(Yaw0);
	if (C0 >= Arm * 0.6) { return; }
	double BestYaw = Yaw0, BestC = C0;
	for (double Off : { 25.0, -25.0, 50.0, -50.0, 75.0, -75.0, 105.0, -105.0, 140.0, -140.0, 180.0 })
	{
		const double Cl = Clear(Yaw0 + Off);
		if (Cl >= Arm * 0.6) { BestYaw = Yaw0 + Off; BestC = Cl; break; }
		if (Cl > BestC + 40.0) { BestYaw = Yaw0 + Off; BestC = Cl; }
	}
	if (BestYaw == Yaw0) { return; }
	PC->SetControlRotation(FRotator(-8.0, BestYaw, 0.0));
	ShotLastTeleportAt = FMath::Max(ShotLastTeleportAt, World->GetTimeSeconds() - ShotSettleAfterTeleport + 0.7);
	UE_LOG(LogBullyMissionVisual, Display, TEXT("BR_MISSIONSHOT boom: %.0f cm clear at yaw %.0f -> %.0f cm at yaw %.0f"), C0, Yaw0, BestC, BestYaw);
}
