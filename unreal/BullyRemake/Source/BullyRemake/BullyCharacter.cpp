// OWNER: builder-B3 (jimmy)
#include "BullyCharacter.h"

#include "BullyLanding.h"
#include "TimerManager.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"

#include "BullyAnimInstance.h"
#include "BullyCombatAnimInstance.h"
#include "BullyCombatComponent.h"
#include "BullyWeaponComponent.h"
#include "BullyVehicle.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ConfigCacheIni.h"
#include "InputModifiers.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyChar, Log, All);

namespace
{
	const TCHAR* GaitName(EBullyGait G)
	{
		switch (G)
		{
		case EBullyGait::Walk: return TEXT("walk");
		case EBullyGait::Run: return TEXT("run");
		case EBullyGait::Sprint: return TEXT("sprint");
		}
		return TEXT("?");
	}
}

ABullyCharacter::ABullyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	// The controller drives the camera only; the body turns toward the movement direction (Bully / GTA style).
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, RunTurnRate, 0.f);
	Move->MaxWalkSpeed = RunSpeed;
	Move->AirControl = AirControl;
	Move->bUseSeparateBrakingFriction = true;
	Move->BrakingFriction = 0.f;
	Move->GroundFriction = GroundFriction;
	Move->MaxStepHeight = MaxStepHeight;
	Move->SetWalkableFloorAngle(50.f);
	Move->NavAgentProps.bCanCrouch = false;
	Move->bCanWalkOffLedges = true;
	JumpMaxHoldTime = 0.f;   // fixed-height jump (the game's LocomotionJump has one height per jump type)

	USkeletalMeshComponent* SMC = GetMesh();
	SMC->SetRelativeLocation(FVector(0.f, 0.f, -CapsuleHalfHeight));
	SMC->SetRelativeRotation(FRotator(0.f, MeshYawOffset, 0.f));
	SMC->SetAnimInstanceClass(UBullyCombatAnimInstance::StaticClass());
	SMC->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SMC->bCastDynamicShadow = true;
	SMC->CastShadow = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeSize = 12.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// slice-combat: melee for the player (the enemy pawn subclass flips the team)
	Combat = CreateDefaultSubobject<UBullyCombatComponent>(TEXT("Combat"));
	Weapons = CreateDefaultSubobject<UBullyWeaponComponent>(TEXT("Weapons"));

	// Placeholder body (engine primitives): shown only when the imported Jimmy assets are missing.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	BodyVis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVis"));
	BodyVis->SetupAttachment(RootComponent);
	BodyVis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadVis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadVis"));
	HeadVis->SetupAttachment(RootComponent);
	HeadVis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CylinderFinder.Succeeded())
	{
		BodyVis->SetStaticMesh(CylinderFinder.Object);
		BodyVis->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.35f));
		BodyVis->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	}
	if (SphereFinder.Succeeded())
	{
		HeadVis->SetStaticMesh(SphereFinder.Object);
		HeadVis->SetRelativeScale3D(FVector(0.34f));
		HeadVis->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	}
}

void ABullyCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	LoadJimmy();
}

// ---------------------------------------------------------------------------------------------------------------
// sm-models: story-cast stand-ins. Config/PedModels.json maps each original ped model (Nemesis_Gary, ...) to the
// generic ped used before it was imported; when /Game/Imported/Characters/<Model>/SK_<Model> is absent (clean clone,
// import not run) the body falls back to the stand-in instead of the grey placeholder.
namespace
{
	const TMap<FString, FString>& PedStandIns()
	{
		static TMap<FString, FString> Map;
		static bool bLoaded = false;
		if (bLoaded) { return Map; }
		bLoaded = true;
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectConfigDir() / TEXT("PedModels.json")))) { return Map; }
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { return Map; }
		const TArray<TSharedPtr<FJsonValue>>* Peds = nullptr;
		if (!Root->TryGetArrayField(TEXT("peds"), Peds)) { return Map; }
		for (const TSharedPtr<FJsonValue>& V : *Peds)
		{
			const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
			FString Model, StandIn;
			if (O && O->TryGetStringField(TEXT("model"), Model) && O->TryGetStringField(TEXT("stand_in"), StandIn) && !StandIn.IsEmpty())
			{
				Map.Add(Model, StandIn);
			}
		}
		return Map;
	}

	// Every ped plays Jimmy's C_Player clips. Their absolute bone translations sink or stretch a ped whose proportions
	// differ (tall prefects/Russell 10-16 cm into the ground, the secretary ~30 cm), so non-Jimmy ped skeletons take
	// bone lengths from their own reference pose: Dummy (0) = Animation (placement), the pelvis carrier "Root" =
	// AnimationRelative (bob/knock-down offsets kept, re-based on this ped's own hip height), the rest = Skeleton.
	// Same rule as BullyScriptedNPC's EnsureGestureRetargeting; runtime-only, one time per skeleton.
	void ApplyPedRetargeting(USkeletalMesh* SK)
	{
		USkeleton* Sk = SK ? SK->GetSkeleton() : nullptr;
		if (!Sk) { return; }
		static TSet<TWeakObjectPtr<USkeleton>> Done;
		if (Done.Contains(Sk)) { return; }
		Done.Add(Sk);
		const FReferenceSkeleton& Ref = Sk->GetReferenceSkeleton();
		const int32 Pelvis = Ref.FindBoneIndex(TEXT("Root"));
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			Sk->SetBoneTranslationRetargetingMode(i, i == 0 ? EBoneTranslationRetargetingMode::Animation
				: i == Pelvis ? EBoneTranslationRetargetingMode::AnimationRelative : EBoneTranslationRetargetingMode::Skeleton);
		}
		UE_LOG(LogBullyChar, Display, TEXT("BR_CHAR ped retargeting on %s (%d bones: Dummy=Animation, Root=AnimationRelative, rest=Skeleton)"),
			*Sk->GetName(), Ref.GetNum());
	}
}

void ABullyCharacter::LoadJimmy()
{
	USkeletalMesh* SK = MeshPath.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	FString Model;
	if (!SK && MeshPath.Split(TEXT("/Game/Imported/Characters/"), nullptr, &Model))
	{
		Model.Split(TEXT("/"), &Model, nullptr);
		if (const FString* StandIn = PedStandIns().Find(Model))
		{
			const FString Fallback = FString::Printf(TEXT("/Game/Imported/Characters/%s/SK_%s.SK_%s"), **StandIn, **StandIn, **StandIn);
			SK = LoadObject<USkeletalMesh>(nullptr, *Fallback, nullptr, LOAD_NoWarn | LOAD_Quiet);
			UE_LOG(LogBullyChar, Warning, TEXT("BR_CHAR ped %s not imported (run tools/ue/run_import_npc.sh --cast): stand-in %s %s"),
				*Model, **StandIn, SK ? TEXT("loaded") : TEXT("MISSING too"));
			if (SK) { MeshPath = Fallback; }   // clips (Anims/ next to the mesh) follow the body actually shown
		}
	}
	bHasJimmyMesh = SK != nullptr;
	if (SK && MeshPath.StartsWith(TEXT("/Game/Imported/Characters/")) && !MeshPath.StartsWith(TEXT("/Game/Imported/Characters/Jimmy/")))
	{
		ApplyPedRetargeting(SK);
	}
	if (SK)
	{
		GetMesh()->SetSkeletalMeshAsset(SK);
		GetMesh()->SetAnimInstanceClass(UBullyCombatAnimInstance::StaticClass());   // locomotion + the combat layer
		const int32 Bones = SK->GetRefSkeleton().GetNum();
		UE_LOG(LogBullyChar, Display, TEXT("BR_CHAR Jimmy mesh %s: %d bones, %d materials, anim instance %s"),
			*MeshPath, Bones, SK->GetMaterials().Num(), *GetNameSafe(GetMesh()->GetAnimInstance()));
	}
	else
	{
		UE_LOG(LogBullyChar, Warning, TEXT("BR_CHAR Jimmy mesh MISSING (%s): run tools/ue/run_import_character.sh; using the placeholder body"), *MeshPath);
	}
	BodyVis->SetVisibility(!bHasJimmyMesh);
	HeadVis->SetVisibility(!bHasJimmyMesh);
	GetMesh()->SetVisibility(bHasJimmyMesh);
}

void ABullyCharacter::ApplyTuning()
{
	GetCapsuleComponent()->SetCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -CapsuleHalfHeight), FRotator(0.f, MeshYawOffset, 0.f));

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->GravityScale = GravityScale;
	Move->AirControl = AirControl;
	Move->GroundFriction = GroundFriction;
	Move->MaxStepHeight = MaxStepHeight;
	const float G = FMath::Abs(GetWorld() ? GetWorld()->GetGravityZ() : -980.f) * GravityScale;
	Move->JumpZVelocity = FMath::Sqrt(2.f * G * JumpHeight);

	CameraBoom->TargetArmLength = CameraDistance;
	CameraBoom->TargetOffset = FVector(0.f, 0.f, CameraLookAtHeight - CapsuleHalfHeight);
	CameraBoom->SocketOffset = FVector::ZeroVector;
	CameraBoom->bEnableCameraLag = CameraLagSpeed > 0.f;
	CameraBoom->CameraLagSpeed = CameraLagSpeed;
	CameraBoom->CameraLagMaxDistance = CameraLagMaxDistance;
	CameraBoom->bEnableCameraRotationLag = false;
	FollowCamera->SetFieldOfView(CameraFOV);
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = CameraMinPitch;
			PC->PlayerCameraManager->ViewPitchMax = CameraMaxPitch;
		}
	}
	UpdateGait();
	UE_LOG(LogBullyChar, Display, TEXT("BR_CHAR tuning: walk %.0f (min %.0f) run %.0f sprint %.0f cm/s; jump %.0f cm (vz %.0f, g %.0f); capsule r%.0f hh%.0f; camera dist %.0f lookat %.0f pitch %.0f fov %.0f lag %.1f; mesh yaw %.0f"),
		WalkSpeed, WalkSpeedMin, RunSpeed, SprintSpeed, JumpHeight, Move->JumpZVelocity, G, CapsuleRadius, CapsuleHalfHeight,
		CameraDistance, CameraLookAtHeight, CameraDefaultPitch, CameraFOV, CameraLagSpeed, MeshYawOffset);
}

void ABullyCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyTuning();
	if (IsPlayerControlled() && FParse::Param(FCommandLine::Get(), TEXT("BRTeleportSmoke"))) { StartTeleportSmoke(); }
	if (bSnapToGroundOnBeginPlay)
	{
		// The PlayerStart is placed above the ground on purpose; drop the capsule onto the first surface below it.
		FHitResult Hit;
		FCollisionQueryParams P(SCENE_QUERY_STAT(BullyCharSnap), /*bTraceComplex*/ true, this);
		const FVector Start = GetActorLocation() + FVector(0, 0, 50.f);
		const FVector End = Start - FVector(0, 0, 5000.f);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, P))
		{
			SetActorLocation(Hit.ImpactPoint + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
			UE_LOG(LogBullyChar, Display, TEXT("BR_PAWN snapped to ground z=%.1f (%s)"), Hit.ImpactPoint.Z, *GetNameSafe(Hit.GetActor()));
		}
		else
		{
			UE_LOG(LogBullyChar, Warning, TEXT("BR_PAWN ground snap MISSED at %s"), *GetActorLocation().ToString());
		}
	}
	FallApexZ = GetActorLocation().Z;
}

float ABullyCharacter::GetHorizontalSpeed() const
{
	return FVector(GetVelocity().X, GetVelocity().Y, 0.f).Size();
}

void ABullyCharacter::SetScriptedGait(EBullyGait InGait)
{
	ScriptedGait = InGait;
	bHasScriptedGait = true;
	UpdateGait();
}

void ABullyCharacter::UpdateGait()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	float Intent = MoveIntent;
	if (Now - LastMoveInputTime > 0.2)
	{
		Intent = 0.f;   // no move input this frame (Triggered stops firing on release)
	}
	float Speed;
	if (bHasScriptedGait)
	{
		Gait = ScriptedGait;
		Speed = Gait == EBullyGait::Walk ? WalkSpeed : Gait == EBullyGait::Run ? RunSpeed : SprintSpeed;
	}
	else
	{
		const bool bStickWalk = Intent > 0.f && Intent < WalkStickThreshold;
		Gait = (bWalkHeld || bStickWalk) ? EBullyGait::Walk : (bSprintHeld ? EBullyGait::Sprint : EBullyGait::Run);
		if (Gait == EBullyGait::Walk)
		{
			// analog walk: 1.3 m/s just past the dead zone up to the clip's 2.05 m/s at the walk/run threshold (or any key)
			const float A = bWalkHeld ? 1.f : FMath::Clamp((Intent - StickDeadZone) / FMath::Max(0.01f, WalkStickThreshold - StickDeadZone), 0.f, 1.f);
			Speed = FMath::Lerp(WalkSpeedMin, WalkSpeed, A);
		}
		else
		{
			Speed = Gait == EBullyGait::Run ? RunSpeed : SprintSpeed;
		}
	}
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const float AccelTime = Gait == EBullyGait::Walk ? WalkAccelTime : Gait == EBullyGait::Run ? RunAccelTime : SprintAccelTime;
	const float StopTime = Gait == EBullyGait::Walk ? WalkStopTime : RunStopTime;
	if (Combat)
	{
		Speed *= Combat->GetMoveSpeedScale();   // locked-on strafing / blocking (0 = rooted)
	}
	Move->MaxWalkSpeed = Speed;
	Move->MaxAcceleration = Speed / FMath::Max(0.05f, AccelTime);
	// constant deceleration from the current speed (sprint stops take longer, as in the game's skid)
	Move->BrakingDecelerationWalking = FMath::Max(Speed, RunSpeed) / FMath::Max(0.05f, StopTime);
	const float Turn = Move->IsFalling() ? AirTurnRate : (Gait == EBullyGait::Walk ? WalkTurnRate : Gait == EBullyGait::Run ? RunTurnRate : SprintTurnRate);
	Move->RotationRate = FRotator(0.f, Turn, 0.f);
}

void ABullyCharacter::SnapCamera()
{
	if (CameraBoom && CameraLagSpeed > 0.f)
	{
		CameraBoom->bEnableCameraLag = false;
		LagRestoreFrames = 2;
	}
}

void ABullyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (LagRestoreFrames > 0 && --LagRestoreFrames == 0 && CameraBoom)
	{
		CameraBoom->bEnableCameraLag = CameraLagSpeed > 0.f;
	}
	UpdateGait();
	if (GetCharacterMovement()->IsFalling())
	{
		const float Z = GetActorLocation().Z;
		FallApexZ = FMath::Max(FallApexZ, Z);
		MaxJumpRise = FMath::Max(MaxJumpRise, Z - LastJumpZ);
	}
	UpdateCamera(DeltaSeconds);
	UpdateCameraShake(DeltaSeconds);
	UpdateFootDiagnostics(DeltaSeconds);
}

void ABullyCharacter::AddCameraShake(float Amplitude, float Duration)
{
	if (!bCameraShake || Amplitude <= 0.f || Duration <= 0.f)
	{
		return;
	}
	// A stronger shake takes over; a weaker one does not stomp the tail of a bigger one still playing.
	const float RemainingAmp = (ShakeDuration > 0.f) ? ShakeAmp * (ShakeTimeLeft / ShakeDuration) : 0.f;
	if (Amplitude >= RemainingAmp)
	{
		ShakeAmp = Amplitude;
		ShakeDuration = Duration;
		ShakeTimeLeft = Duration;
		UE_LOG(LogBullyChar, Display, TEXT("BR_CHAR shake amp=%.2f dur=%.2f"), Amplitude, Duration);
	}
}

void ABullyCharacter::UpdateCameraShake(float Dt)
{
	if (!FollowCamera)
	{
		return;
	}
	if (ShakeTimeLeft <= 0.f)
	{
		if (!bShakeSettled)
		{
			FollowCamera->SetRelativeLocation(FVector::ZeroVector);
			FollowCamera->SetRelativeRotation(FRotator::ZeroRotator);
			bShakeSettled = true;
		}
		return;
	}
	ShakeTimeLeft = FMath::Max(0.f, ShakeTimeLeft - Dt);
	ShakePhase += Dt * ShakeFrequency;
	// Squared decay envelope so the shake dies out smoothly rather than cutting off.
	const float Env = (ShakeDuration > 0.f) ? (ShakeTimeLeft / ShakeDuration) : 0.f;
	const float A = ShakeAmp * Env * Env;
	const float t = ShakePhase * 2.f * PI;
	// Detuned per-axis oscillators (multipliers near 1, offset phases) read as an organic jolt, not a sine wobble.
	const FVector Loc(FMath::Sin(t * 1.00f), FMath::Sin(t * 1.27f + 1.3f), FMath::Sin(t * 0.91f + 2.1f));
	const FRotator Rot(FMath::Sin(t * 1.33f + 2.4f) /*pitch*/, FMath::Sin(t * 0.97f + 4.0f) /*yaw*/, FMath::Sin(t * 1.13f + 0.7f) /*roll*/);
	FollowCamera->SetRelativeLocation(Loc * (A * ShakeTranslateScale));
	FollowCamera->SetRelativeRotation(Rot * (A * ShakeRotateScale));
	bShakeSettled = false;
}

void ABullyCharacter::UpdateCamera(float Dt)
{
	if (!Controller || !GetWorld() || !IsPlayerControlled())
	{
		return;
	}
	if (Combat && Combat->IsLockedOn() && LockOnCameraYawRate > 0.f)
	{
		// lock-on: keep Jimmy and the target in frame (camera looks along player -> target)
		FRotator R = Controller->GetControlRotation();
		const FVector To = Combat->GetLockTarget()->GetOwner()->GetActorLocation() - GetActorLocation();
		const float DYaw = FMath::FindDeltaAngleDegrees(R.Yaw, To.Rotation().Yaw);
		R.Yaw += FMath::Clamp(DYaw, -LockOnCameraYawRate * Dt, LockOnCameraYawRate * Dt);
		Controller->SetControlRotation(R);
		return;
	}
	if (!bCameraAutoFollow)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const float Speed = GetHorizontalSpeed();
	if (Now - LastLookInputTime < CameraFollowDelay || Speed < 50.f || bDoorBusy)
	{
		return;
	}
	FRotator R = Controller->GetControlRotation();
	R.Pitch = FRotator::NormalizeAxis(R.Pitch);
	const float TargetYaw = GetVelocity().Rotation().Yaw;
	const float DYaw = FMath::FindDeltaAngleDegrees(R.Yaw, TargetYaw);
	// running toward the camera: keep it (GTA-era follow cams only swing when the player turns away)
	if (FMath::Abs(DYaw) < 135.f)
	{
		const float Rate = CameraFollowYawRate * FMath::Clamp(Speed / FMath::Max(1.f, RunSpeed), 0.3f, 1.6f);
		R.Yaw += FMath::Clamp(DYaw, -Rate * Dt, Rate * Dt);
	}
	R.Pitch = FMath::FInterpConstantTo(R.Pitch, CameraDefaultPitch, Dt, CameraFollowPitchRate);
	Controller->SetControlRotation(R);
}

void ABullyCharacter::UpdateFootDiagnostics(float Dt)
{
	if (!bHasJimmyMesh || Dt <= 0.f || !GetWorld() || !bLogLocomotion)
	{
		return;
	}
	static const FName Feet[2] = { TEXT("Root-L-Foot"), TEXT("Root-R-Foot") };
	const float Bottom = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const bool bGrounded = !GetCharacterMovement()->IsFalling();
	FVector Cur[2];
	for (int32 i = 0; i < 2; ++i)
	{
		Cur[i] = GetMesh()->GetBoneLocation(Feet[i]);
	}
	if (FootPlantHeight <= 0.f && bGrounded && GetHorizontalSpeed() < 5.f)
	{
		// calibrate: ankle height when standing in the idle pose (+4 cm tolerance)
		FootPlantHeight = FMath::Min(Cur[0].Z, Cur[1].Z) - Bottom + 4.f;
	}
	const float Speed = GetHorizontalSpeed();
	if (bPrevFootValid && bGrounded && Speed > 30.f && FootPlantHeight > 0.f)
	{
		// the lower foot, if it is down, should not move horizontally in the world
		const int32 Low = Cur[0].Z < Cur[1].Z ? 0 : 1;
		if (Cur[Low].Z - Bottom < FootPlantHeight)
		{
			const FVector D = Cur[Low] - PrevFoot[Low];
			SlideSamples.Add(FVector(D.X, D.Y, 0.f).Size() / Dt);
		}
		SpeedSum += Speed;
		++SpeedSamples;
	}
	PrevFoot[0] = Cur[0];
	PrevFoot[1] = Cur[1];
	bPrevFootValid = true;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextDiagLog)
	{
		NextDiagLog = Now + 1.0;
		const UBullyAnimInstance* AI = Cast<UBullyAnimInstance>(GetMesh()->GetAnimInstance());
		SlideSamples.Sort();
		const float Median = SlideSamples.Num() ? SlideSamples[SlideSamples.Num() / 2] : 0.f;
		UE_LOG(LogBullyChar, Display, TEXT("BR_CHAR t=%.1f pos=(%.0f,%.0f,%.0f) speed=%.0f gait=%s anim=%s phase=%.2f planted-foot-slide median %.1f cm/s (n=%d, mean speed %.0f) layers=[%s]"),
			Now, GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, Speed, GaitName(Gait), AI ? AI->GetStateName() : TEXT("none"),
			AI ? AI->GetPhase() : 0.f, Median, SlideSamples.Num(), SpeedSamples ? SpeedSum / SpeedSamples : 0.0,
			AI ? *AI->DescribeLayers() : TEXT(""));
		SlideSamples.Reset(); SpeedSum = 0.0; SpeedSamples = 0;
	}
}

void ABullyCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	++JumpCount;
	LastJumpZ = GetActorLocation().Z;
	FallApexZ = LastJumpZ;
	MaxJumpRise = 0.f;
	UE_LOG(LogBullyChar, Display, TEXT("BR_CHAR jump #%d at %s speed %.0f vz %.0f"), JumpCount, *GetActorLocation().ToString(), GetHorizontalSpeed(), GetVelocity().Z);
}

void ABullyCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
	if (GetCharacterMovement()->IsFalling())
	{
		FallApexZ = FMath::Max(FallApexZ, GetActorLocation().Z);
		if (PrevMovementMode == MOVE_Walking)
		{
			FallApexZ = GetActorLocation().Z;
		}
	}
}

void ABullyCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	++LandCount;
	LastLandDrop = FallApexZ - GetActorLocation().Z;
	UE_LOG(LogBullyChar, Display, TEXT("BR_CHAR land #%d drop %.0f cm, jump rise %.0f cm, speed %.0f"), LandCount, LastLandDrop, MaxJumpRise, GetHorizontalSpeed());
	if (LastLandDrop > FallDamageHeight)
	{
		++HardLandings;
		// STUB: the game's Fall_Damage node (DistToGround > 5 m) hurts Jimmy; health does not exist yet.
		UE_LOG(LogBullyChar, Display, TEXT("BR_CHAR fall damage (stub): drop %.1f m > %.1f m"), LastLandDrop / 100.f, FallDamageHeight / 100.f);
	}
	FallApexZ = GetActorLocation().Z;
}

// ---------------------------------------------------------------------------------------------------------------
// input

void ABullyCharacter::BuildInputAssets()
{
	if (MappingContext)
	{
		return;
	}
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_BullyDefault"));
	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	LookAction = NewObject<UInputAction>(this, TEXT("IA_Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;
	LookPadAction = NewObject<UInputAction>(this, TEXT("IA_LookPad"));
	LookPadAction->ValueType = EInputActionValueType::Axis2D;
	JumpAction = NewObject<UInputAction>(this, TEXT("IA_Jump"));
	JumpAction->ValueType = EInputActionValueType::Boolean;
	SprintAction = NewObject<UInputAction>(this, TEXT("IA_Sprint"));
	SprintAction->ValueType = EInputActionValueType::Boolean;
	WalkAction = NewObject<UInputAction>(this, TEXT("IA_Walk"));
	WalkAction->ValueType = EInputActionValueType::Boolean;
	// slice-combat
	AttackAction = NewObject<UInputAction>(this, TEXT("IA_Attack"));
	AttackAction->ValueType = EInputActionValueType::Boolean;
	GrappleAction = NewObject<UInputAction>(this, TEXT("IA_Grapple"));
	GrappleAction->ValueType = EInputActionValueType::Boolean;
	BlockAction = NewObject<UInputAction>(this, TEXT("IA_Block"));
	BlockAction->ValueType = EInputActionValueType::Boolean;
	LockOnAction = NewObject<UInputAction>(this, TEXT("IA_LockOn"));
	LockOnAction->ValueType = EInputActionValueType::Boolean;
	WeaponNextAction = NewObject<UInputAction>(this, TEXT("IA_WeaponNext"));
	WeaponNextAction->ValueType = EInputActionValueType::Boolean;
	WeaponPrevAction = NewObject<UInputAction>(this, TEXT("IA_WeaponPrev"));
	WeaponPrevAction->ValueType = EInputActionValueType::Boolean;

	auto Swizzle = [this]() { UInputModifierSwizzleAxis* M = NewObject<UInputModifierSwizzleAxis>(this); M->Order = EInputAxisSwizzle::YXZ; return M; };
	auto Negate = [this](bool X, bool Y, bool Z) { UInputModifierNegate* M = NewObject<UInputModifierNegate>(this); M->bX = X; M->bY = Y; M->bZ = Z; return M; };
	auto Deadzone = [this](float Lower) { UInputModifierDeadZone* M = NewObject<UInputModifierDeadZone>(this); M->LowerThreshold = Lower; M->Type = EDeadZoneType::Radial; return M; };

	// Move: Axis2D = (right, forward). Keys give magnitude 1 (= run); the left stick is analog (walk below 0.5).
	MappingContext->MapKey(MoveAction, EKeys::D);
	MappingContext->MapKey(MoveAction, EKeys::A).Modifiers.Add(Negate(true, false, false));
	{ FEnhancedActionKeyMapping& W = MappingContext->MapKey(MoveAction, EKeys::W); W.Modifiers.Add(Swizzle()); }
	{ FEnhancedActionKeyMapping& S = MappingContext->MapKey(MoveAction, EKeys::S); S.Modifiers.Add(Swizzle()); S.Modifiers.Add(Negate(true, false, false)); }
	// radial dead zone WITHOUT rescaling the magnitude would be ideal; UE's dead zone rescales [lower,1] -> [0,1],
	// so the walk threshold is applied to the rescaled magnitude.
	MappingContext->MapKey(MoveAction, EKeys::Gamepad_Left2D).Modifiers.Add(Deadzone(StickDeadZone));

	// Mouse: raw per-frame counts (scaled to degrees in OnLook). Gamepad right stick is its own action: it is a rate
	// (-1..1 held), not a delta, so it is scaled by deg/s * dt in OnLookPad and never mixed with the mouse scale.
	MappingContext->MapKey(LookAction, EKeys::Mouse2D).Modifiers.Add(Negate(false, true, false));
	{
		FEnhancedActionKeyMapping& R = MappingContext->MapKey(LookPadAction, EKeys::Gamepad_Right2D);
		R.Modifiers.Add(Deadzone(0.2f));
		R.Modifiers.Add(Negate(false, true, false));
	}

	MappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	MappingContext->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	// hold to sprint (the original console scheme taps for sprint charge; not modelled yet)
	MappingContext->MapKey(SprintAction, EKeys::LeftShift);
	MappingContext->MapKey(SprintAction, EKeys::Gamepad_LeftThumbstick);
	MappingContext->MapKey(SprintAction, EKeys::Gamepad_FaceButton_Left);
	MappingContext->MapKey(WalkAction, EKeys::LeftAlt);
	MappingContext->MapKey(WalkAction, EKeys::LeftControl);

	// slice-combat. Keyboard/mouse: LMB attack, F grapple, Q (hold) block, RMB (hold) lock-on.
	// Gamepad: B attack, RB grapple, LB (hold) block, RT (hold) lock-on. (A jump, X sprint, Y interact are taken;
	// Esc stays on the controller's pause menu.)
	MappingContext->MapKey(AttackAction, EKeys::LeftMouseButton);
	MappingContext->MapKey(AttackAction, EKeys::Gamepad_FaceButton_Right);
	MappingContext->MapKey(GrappleAction, EKeys::F);
	MappingContext->MapKey(GrappleAction, EKeys::Gamepad_RightShoulder);
	MappingContext->MapKey(BlockAction, EKeys::Q);
	MappingContext->MapKey(BlockAction, EKeys::Gamepad_LeftShoulder);
	MappingContext->MapKey(LockOnAction, EKeys::RightMouseButton);
	MappingContext->MapKey(LockOnAction, EKeys::Gamepad_RightTrigger);

	// slice-weapons: mouse wheel / D-pad left-right cycle Fists -> each weapon with ammo. LMB / B fires when one is out.
	// (The number row stays on the interior quick-teleport cheat.)
	MappingContext->MapKey(WeaponNextAction, EKeys::MouseScrollUp);
	MappingContext->MapKey(WeaponNextAction, EKeys::Gamepad_DPad_Right);
	MappingContext->MapKey(WeaponPrevAction, EKeys::MouseScrollDown);
	MappingContext->MapKey(WeaponPrevAction, EKeys::Gamepad_DPad_Left);
}

const UInputAction* ABullyCharacter::GetCombatAction(FName Which) const
{
	if (Which == TEXT("attack")) { return AttackAction; }
	if (Which == TEXT("grapple")) { return GrappleAction; }
	if (Which == TEXT("block")) { return BlockAction; }
	if (Which == TEXT("lockon")) { return LockOnAction; }
	if (Which == TEXT("move")) { return MoveAction; }
	return nullptr;
}

void ABullyCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	BuildInputAssets();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->AddMappingContext(MappingContext, 0);
		}
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = CameraMinPitch;
			PC->PlayerCameraManager->ViewPitchMax = CameraMaxPitch;
		}
		PC->SetControlRotation(FRotator(CameraDefaultPitch, GetActorRotation().Yaw, 0.f));
	}
}

void ABullyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInputAssets();

	// exploration cheat: number row 1..0 teleports into the imported interiors (same spots as the BRGo console command)
	{
		const FKey SlotKeys[10] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
			EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero };
		const TCHAR* SlotNames[10] = { TEXT("pool"), TEXT("audt"), TEXT("lib"), TEXT("chem"), TEXT("bio"),
			TEXT("art"), TEXT("auto"), TEXT("boil"), TEXT("clas"), TEXT("gdrm") };
		for (int32 i = 0; i < 10; ++i)
		{
			FInputKeyBinding B(FInputChord(SlotKeys[i]), IE_Pressed);
			B.bConsumeInput = false;
			const FString Spot = SlotNames[i];
			B.KeyDelegate.GetDelegateForManualSet().BindLambda([this, Spot]() { BRGo(Spot); });
			PlayerInputComponent->KeyBindings.Add(MoveTemp(B));
		}
	}
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABullyCharacter::OnMove);
		EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &ABullyCharacter::OnMoveCompleted);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABullyCharacter::OnLook);
		EIC->BindAction(LookPadAction, ETriggerEvent::Triggered, this, &ABullyCharacter::OnLookPad);
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ABullyCharacter::OnJumpPressed);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &ABullyCharacter::OnSprint);
		EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &ABullyCharacter::OnSprintEnd);
		EIC->BindAction(WalkAction, ETriggerEvent::Started, this, &ABullyCharacter::OnWalk);
		EIC->BindAction(WalkAction, ETriggerEvent::Completed, this, &ABullyCharacter::OnWalkEnd);
		EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &ABullyCharacter::OnAttack);
		EIC->BindAction(GrappleAction, ETriggerEvent::Started, this, &ABullyCharacter::OnGrapple);
		EIC->BindAction(BlockAction, ETriggerEvent::Started, this, &ABullyCharacter::OnBlock);
		EIC->BindAction(BlockAction, ETriggerEvent::Completed, this, &ABullyCharacter::OnBlockEnd);
		EIC->BindAction(LockOnAction, ETriggerEvent::Started, this, &ABullyCharacter::OnLockOn);
		EIC->BindAction(LockOnAction, ETriggerEvent::Completed, this, &ABullyCharacter::OnLockOnEnd);
		EIC->BindAction(WeaponNextAction, ETriggerEvent::Started, this, &ABullyCharacter::OnWeaponNext);
		EIC->BindAction(WeaponPrevAction, ETriggerEvent::Started, this, &ABullyCharacter::OnWeaponPrev);
	}
	else
	{
		UE_LOG(LogBullyChar, Error, TEXT("BR_PAWN input component is not an EnhancedInputComponent"));
	}
}

void ABullyCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}
	bHasScriptedGait = false;   // a player took over
	if (Combat && !Combat->CanMove())
	{
		MoveIntent = 0.f;           // attacking / hit / grappling: the stick does nothing (no sliding out of a swing)
		return;
	}
	MoveIntent = FMath::Clamp(V.Size(), 0.f, 1.f);
	LastMoveInputTime = GetWorld()->GetTimeSeconds();
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Dir = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * V.Y + FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * V.X;
	// full input strength: the gait (from the stick magnitude) sets the speed, not the analog scale
	AddMovementInput(Dir.GetSafeNormal(), 1.f);
}

void ABullyCharacter::OnLook(const FInputActionValue& Value)
{
	// raw Mouse2D counts -> degrees. Y was negated by the mapping; invert flips it back.
	const float Base = LookBaseDegPerCount * LookSensitivity;
	const FVector2D In = Value.Get<FVector2D>();
	const FVector2D V(In.X * Base * LookSensitivityX, In.Y * Base * LookSensitivityY * (bInvertLookY ? -1.f : 1.f));
	if (!V.IsNearlyZero())
	{
		NoteLookInput();
	}
	AddControllerYawInput(V.X);
	AddControllerPitchInput(V.Y);
}

void ABullyCharacter::OnLookPad(const FInputActionValue& Value)
{
	constexpr float DefaultLookSensX = 0.6f, DefaultLookSensY = 0.5f;   // shipped defaults (the 1.0x point for gamepad rates)
	// stick (rate) -> degrees this frame. The menu sliders scale it relative to their defaults so one slider moves both devices.
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	const FVector2D In = Value.Get<FVector2D>();
	const FVector2D V(In.X * GamepadLookYawRate * Dt * LookSensitivity * (LookSensitivityX / DefaultLookSensX),
	                  In.Y * GamepadLookPitchRate * Dt * LookSensitivity * (LookSensitivityY / DefaultLookSensY) * (bInvertLookY ? -1.f : 1.f));
	if (!V.IsNearlyZero())
	{
		NoteLookInput();
	}
	AddControllerYawInput(V.X);
	AddControllerPitchInput(V.Y);
}

void ABullyCharacter::SetLookSettings(float NewX, float NewY, bool bNewInvertY)
{
	LookSensitivityX = FMath::Clamp(NewX, MinLookSens, MaxLookSens);
	LookSensitivityY = FMath::Clamp(NewY, MinLookSens, MaxLookSens);
	bInvertLookY = bNewInvertY;
	// future pawns (respawn / travel) pick the live values up from the class defaults
	if (ABullyCharacter* CDO = GetMutableDefault<ABullyCharacter>(); CDO != this)
	{
		CDO->LookSensitivityX = LookSensitivityX;
		CDO->LookSensitivityY = LookSensitivityY;
		CDO->bInvertLookY = bInvertLookY;
	}
}

void ABullyCharacter::SaveLookSettings() const
{
	// The user layer is the LAST file in the Game.ini hierarchy (Saved/Config/<platform>/Game.ini): it overrides
	// Config/DefaultGame.ini on the next launch and is never checked in. Edit only our three keys in that file.
	const FString Path = FConfigCacheIni::GetDestIniFilename(TEXT("Game"), nullptr, *FPaths::GeneratedConfigDir());
	FConfigFile File;
	if (IFileManager::Get().FileExists(*Path))
	{
		File.Read(Path);
	}
	const TCHAR* Section = TEXT("/Script/BullyRemake.BullyCharacter");
	File.SetFloat(Section, TEXT("LookSensitivityX"), LookSensitivityX);
	File.SetFloat(Section, TEXT("LookSensitivityY"), LookSensitivityY);
	File.SetBool(Section, TEXT("bInvertLookY"), bInvertLookY);
	const bool bOk = File.Write(Path);
	UE_LOG(LogBullyChar, Display, TEXT("BR_SETTINGS saved look X=%.3f Y=%.3f invertY=%d -> %s (write ok=%d exists=%d)"), LookSensitivityX, LookSensitivityY, bInvertLookY ? 1 : 0, *Path, bOk ? 1 : 0, IFileManager::Get().FileExists(*Path) ? 1 : 0);
}

// ---------------------------------------------------------------------------------------------------------------
// slice-combat input

void ABullyCharacter::OnJumpPressed(const FInputActionValue& Value)
{
	if (Combat && !Combat->CanMove())
	{
		return;
	}
	Jump();
}

void ABullyCharacter::OnAttack(const FInputActionValue& Value)
{
	// A weapon out: LMB fires it. Fists: melee as before.
	if (Weapons && Weapons->HasEquipped()) { Weapons->Fire(); return; }
	if (Combat) { Combat->RequestAttack(); }
}

void ABullyCharacter::OnWeaponNext(const FInputActionValue& Value)
{
	if (Weapons) { Weapons->Cycle(+1); }
}

void ABullyCharacter::OnWeaponPrev(const FInputActionValue& Value)
{
	if (Weapons) { Weapons->Cycle(-1); }
}

void ABullyCharacter::BRGiveWeapons()
{
	if (Weapons) { Weapons->RefillAll(); }
}

void ABullyCharacter::BRSpawnVehicle(const FString& Id)
{
	const FVector Fwd = FRotator(0.f, GetActorRotation().Yaw, 0.f).Vector();
	ABullyVehicle::SpawnVehicle(GetWorld(), FName(*Id), GetActorLocation() + Fwd * 450.f, GetActorRotation().Yaw);
}

void ABullyCharacter::OnGrapple(const FInputActionValue& Value)
{
	if (Combat) { Combat->RequestGrapple(); }
}

void ABullyCharacter::OnBlock(const FInputActionValue& Value)
{
	if (Combat) { Combat->SetBlockHeld(true); }
}

void ABullyCharacter::OnBlockEnd(const FInputActionValue& Value)
{
	if (Combat) { Combat->SetBlockHeld(false); }
}

void ABullyCharacter::OnLockOn(const FInputActionValue& Value)
{
	if (Combat) { Combat->SetLockOnHeld(true); NoteLookInput(); }
}

void ABullyCharacter::OnLockOnEnd(const FInputActionValue& Value)
{
	if (Combat) { Combat->SetLockOnHeld(false); }
}

// Console exploration cheat: "BRGo <name>" teleports into an imported interior pocket; "BRGo list" names them.
// Spots come from Config/InteriorSpots.json (derived from the import manifest); the landing is floor-snapped.
void ABullyCharacter::BRGo(const FString& Name)
{
	const FString Path = FPaths::ProjectConfigDir() / TEXT("InteriorSpots.json");
	FString Raw;
	TSharedPtr<FJsonObject> RootObj;
	if (!FFileHelper::LoadFileToString(Raw, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BR_GO no readable %s"), *Path);
		return;
	}
	const TSharedPtr<FJsonObject>* Spots;
	if (!RootObj->TryGetObjectField(TEXT("spots"), Spots)) { return; }

	FString Names;
	for (const auto& It : (*Spots)->Values)
	{
		if (!Names.IsEmpty()) { Names += TEXT(", "); }
		Names += FString(It.Key);
	}
	if (Name.IsEmpty() || Name.Equals(TEXT("list"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTemp, Display, TEXT("BR_GO spots: %s"), *Names);
		if (GEngine) { GEngine->AddOnScreenDebugMessage(INDEX_NONE, 8.f, FColor::Yellow, FString::Printf(TEXT("BRGo: %s"), *Names)); }
		return;
	}
	const TSharedPtr<FJsonObject>* Spot;
	if (!(*Spots)->TryGetObjectField(Name.ToLower(), Spot))
	{
		if (GEngine) { GEngine->AddOnScreenDebugMessage(INDEX_NONE, 6.f, FColor::Red, FString::Printf(TEXT("BRGo: unknown '%s' — try: %s"), *Name, *Names)); }
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* L;
	if (!(*Spot)->TryGetArrayField(TEXT("location_cm"), L) || L->Num() < 3) { return; }
	FVector Dest(float((*L)[0]->AsNumber()), float((*L)[1]->AsNumber()), float((*L)[2]->AsNumber()));

	// land on real, open ground near the spot (never a roof / upper floor / gap between walls); interiors search a
	// tighter radius so you stay in the room you picked
	FString Scene;
	(*Spot)->TryGetStringField(TEXT("scene"), Scene);
	const bool bInterior = Scene.StartsWith(TEXT("i"), ESearchCase::IgnoreCase);
	const float HalfH = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	FVector Ground;
	const TArray<TSharedPtr<FJsonValue>>* G = nullptr;
	bool bFound = false;
	if ((*Spot)->TryGetArrayField(TEXT("ground_cm"), G) && G->Num() == 3)
	{
		// outdoor: a sidewalk node of the game's own ped network; stay within a storey of it (terrain here is stacked)
		const FVector Node((*G)[0]->AsNumber(), (*G)[1]->AsNumber(), (*G)[2]->AsNumber());
		bFound = FBullyLanding::Find(GetWorld(), Node, 2500.f, Ground, this, 150.f, 200.f, 120.f)
			|| FBullyLanding::Find(GetWorld(), Node, 2500.f, Ground, this, 60.f, 200.f, 120.f);
	}
	if (!bFound)
	{
		bFound = FBullyLanding::Find(GetWorld(), Dest, bInterior ? 2500.f : 8000.f, Ground, this, bInterior ? 90.f : 150.f)
			|| FBullyLanding::Find(GetWorld(), Dest, bInterior ? 2500.f : 8000.f, Ground, this, 45.f);   // tight rooms: a capsule's worth
	}
	if (bFound)
	{
		Dest = Ground + FVector(0, 0, HalfH + 3.f);
	}
	else
	{
		FCollisionQueryParams P(SCENE_QUERY_STAT(BRGo), true, this);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Dest + FVector(0, 0, 300.f), Dest - FVector(0, 0, 2000.f), ECC_Visibility, P))
		{
			Dest = Hit.Location + FVector(0, 0, HalfH + 8.f);
		}
	}
	SetActorLocation(Dest, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();
	SnapCamera();
	UE_LOG(LogTemp, Display, TEXT("BR_GO %s -> %s"), *Name, *Dest.ToCompactString());
	if (GEngine) { GEngine->AddOnScreenDebugMessage(INDEX_NONE, 4.f, FColor::Green, FString::Printf(TEXT("BRGo: %s"), *Name)); }
}

void ABullyCharacter::StartTeleportSmoke()
{
	// every spot in turn: warp, let the capsule settle 1.5 s, then it must be walking on the ground near where it landed
	TArray<FString> Keys;
	FString Raw;
	TSharedPtr<FJsonObject> Root;
	const TSharedPtr<FJsonObject>* Spots = nullptr;
	if (FFileHelper::LoadFileToString(Raw, *(FPaths::ProjectConfigDir() / TEXT("InteriorSpots.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) && Root.IsValid() && Root->TryGetObjectField(TEXT("spots"), Spots))
	{
		for (const auto& It : (*Spots)->Values) { Keys.Add(FString(It.Key)); }
	}
	struct FState { int32 Index = -1; int32 Pass = 0; TArray<FString> Fails; FVector Landed = FVector::ZeroVector; };
	TSharedRef<FState> St = MakeShared<FState>();
	TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
	TWeakObjectPtr<ABullyCharacter> WeakThis = this;
	GetWorldTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda([WeakThis, Keys, St, Handle]()
	{
		ABullyCharacter* C = WeakThis.Get();
		if (!C) { return; }
		if (St->Index >= 0)
		{
			const FString& K = Keys[St->Index];
			const FVector P = C->GetActorLocation();
			const bool bOnGround = C->GetCharacterMovement()->IsMovingOnGround();
			const float Drop = St->Landed.Z - P.Z;
			if (bOnGround && Drop < 150.f) { ++St->Pass; UE_LOG(LogTemp, Display, TEXT("BR_TELEPORT_SMOKE PASS %s: standing at %s"), *K, *P.ToCompactString()); }
			else
			{
				St->Fails.Add(K);
				UE_LOG(LogTemp, Error, TEXT("BR_TELEPORT_SMOKE FAIL %s: on ground %d, dropped %.0f cm after landing (now %s)"), *K, bOnGround, Drop, *P.ToCompactString());
			}
		}
		if (++St->Index >= Keys.Num())
		{
			UE_LOG(LogTemp, Display, TEXT("BR_TELEPORT_SMOKE RESULT %s: %d/%d spots land on open ground; failed: %s"),
				St->Fails.Num() == 0 ? TEXT("PASS") : TEXT("FAIL"), St->Pass, Keys.Num(), *FString::Join(St->Fails, TEXT(", ")));
			C->GetWorldTimerManager().ClearTimer(*Handle);
			GEngine->Exec(C->GetWorld(), TEXT("quit"));
			return;
		}
		C->BRGo(Keys[St->Index]);
		St->Landed = C->GetActorLocation();
	}), 1.5f, true, 4.f);
}
