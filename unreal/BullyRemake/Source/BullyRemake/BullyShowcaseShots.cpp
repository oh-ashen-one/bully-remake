// OWNER: slice-weapons
#include "BullyShowcaseShots.h"

#include "BullyCharacter.h"
#include "BullyCombatComponent.h"
#include "BullyCombatPawn.h"
#include "BullyCombatTest.h"
#include "BullyProjectile.h"
#include "BullyVehicle.h"
#include "BullyWeaponComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HighResScreenshot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBullyShowcase, Log, All);

namespace
{
	enum EStep : int32
	{
		WaitCompile, RideMount, RideDrive, RideSide, RideChase, RideOff,
		WeaponSetup, WeaponHeld, WeaponFire, WeaponRelease, WeaponImpact, WeaponNext, Done, Quit
	};
}

bool UBullyShowcaseShots::ShouldCreateSubsystem(UObject* Outer) const
{
	FString Unused;
	const UWorld* W = Cast<UWorld>(Outer);
	return W && W->IsGameWorld() && FParse::Value(FCommandLine::Get(), TEXT("BRShowcaseShots="), Unused);
}

void UBullyShowcaseShots::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FParse::Value(FCommandLine::Get(), TEXT("BRShowcaseShots="), Dir);
	FParse::Value(FCommandLine::Get(), TEXT("BRShotResX="), ResX);
	FParse::Value(FCommandLine::Get(), TEXT("BRShotResY="), ResY);
	bActive = !Dir.IsEmpty();
}

TStatId UBullyShowcaseShots::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBullyShowcaseShots, STATGROUP_Tickables);
}

void UBullyShowcaseShots::Shot(const FString& Name)
{
	const FString File = Dir / FString::Printf(TEXT("%02d_%s.png"), Count++, *Name);
	FHighResScreenshotConfig& HR = GetHighResScreenshotConfig();
	HR.SetResolution(uint32(ResX), uint32(ResY), 1.0f);
	HR.SetFilename(File);
	HR.bMaskEnabled = false;
	HR.bDumpBufferVisualizationTargets = false;
	const bool bOk = GEngine->GameViewport && GEngine->GameViewport->Viewport && GEngine->GameViewport->Viewport->TakeHighResScreenShot();
	UE_LOG(LogBullyShowcase, Display, TEXT("BR_SHOWCASE %s %s"), bOk ? TEXT("shot") : TEXT("REFUSED"), *File);
}

void UBullyShowcaseShots::Frame(const FVector& Eye, const FVector& LookAt, float Fov)
{
	UWorld* W = GetWorld();
	if (!Cam.IsValid())
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Eye, FRotator::ZeroRotator, SP);
		Cam->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	Cam->SetActorLocationAndRotation(Eye, (LookAt - Eye).Rotation());
	Cam->GetCameraComponent()->SetFieldOfView(Fov);
	if (APlayerController* PC = W->GetFirstPlayerController()) { PC->SetViewTarget(Cam.Get()); }
}

void UBullyShowcaseShots::UseGameCamera()
{
	UWorld* W = GetWorld();
	if (APlayerController* PC = W->GetFirstPlayerController()) { PC->SetViewTarget(PC->GetPawn()); }
}

void UBullyShowcaseShots::Tick(float Dt)
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	const double Now = W->GetTimeSeconds();
	const double T = Now - StepStart;
	auto Next = [&](int32 S) { Step = S; StepStart = Now; };
	APlayerController* PC = W->GetFirstPlayerController();
	ABullyCharacter* Me = nullptr;
	for (TActorIterator<ABullyCharacter> It(W); It; ++It) { if (It->IsPlayerControlled() || It->GetAttachParentActor()) { Me = *It; break; } }
	if (!PC || !Me) { return; }
	UBullyWeaponComponent* Weapons = Me->GetWeapons();

	switch (Step)
	{
	case WaitCompile:
	{
		int32 Assets = 0, Jobs = 0;
#if WITH_EDITOR
		Assets = FAssetCompilingManager::Get().GetNumRemainingAssets();
		Jobs = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
#endif
		if (Assets == 0 && Jobs == 0) { if (IdleSince < 0.0) { IdleSince = Now; } }
		else { IdleSince = -1.0; }
		if (Now - LastLog > 10.0) { LastLog = Now; UE_LOG(LogBullyShowcase, Display, TEXT("BR_SHOWCASE waiting for compilation: %d assets, %d shader jobs"), Assets, Jobs); }
		if (IdleSince < 0.0 || Now - IdleSince < 8.0) { break; }
		for (TActorIterator<ABullyVehicle> It(W); It; ++It) { Rides.Add(*It); }
		Rides.Sort([](const TWeakObjectPtr<ABullyVehicle>& A, const TWeakObjectPtr<ABullyVehicle>& B) { return A->GetDef().MaxSpeed < B->GetDef().MaxSpeed; });
		UE_LOG(LogBullyShowcase, Display, TEXT("BR_SHOWCASE start: %d ride(s), %d weapon(s)"), Rides.Num(), Weapons ? Weapons->GetDefs().Num() : 0);
		Index = 0;
		Next(Rides.Num() > 0 ? RideMount : WeaponSetup);
		break;
	}

	// ---- rides
	case RideMount:
	{
		ABullyVehicle* V = Rides.IsValidIndex(Index) ? Rides[Index].Get() : nullptr;
		if (!V) { Next(RideOff); break; }
		if (Me->GetAttachParentActor() == nullptr) { V->Mount(Me); }
		V->SetScriptedInput(true, 0.7f, 0.f);
		Next(RideDrive);
		break;
	}
	case RideDrive:
	{
		ABullyVehicle* V = Rides[Index].Get();
		if (T < 1.8) { break; }
		V->SetScriptedInput(true, 0.25f, 0.15f);   // keep rolling, a slight lean into a turn
		const FVector E = V->GetDef().HalfExtent;
		const float D = E.X * 2.4f + 260.f;
		const FVector P = V->GetActorLocation();
		Frame(P + V->GetActorForwardVector() * D * 0.75f + V->GetActorRightVector() * D * 0.85f + FVector(0, 0, E.Z + 90.f), P + FVector(0, 0, 50.f), 55.f);
		Next(RideSide);
		break;
	}
	case RideSide:
		if (T < 0.5) { break; }
		Shot(FString::Printf(TEXT("ride_%s_side"), *Rides[Index]->GetDef().Id.ToString()));
		UseGameCamera();
		Next(RideChase);
		break;
	case RideChase:
		if (T < 1.0) { break; }
		Shot(FString::Printf(TEXT("ride_%s_chase"), *Rides[Index]->GetDef().Id.ToString()));
		Next(RideOff);
		break;
	case RideOff:
	{
		if (T < 0.4) { break; }
		if (ABullyVehicle* V = Rides.IsValidIndex(Index) ? Rides[Index].Get() : nullptr)
		{
			V->SetScriptedInput(true, 0.f, 0.f, true);
			V->Dismount();
			V->ResetMotion();
			V->SetScriptedInput(false);
		}
		++Index;
		Next(Index < Rides.Num() ? RideMount : WeaponSetup);
		break;
	}

	// ---- weapons
	case WeaponSetup:
	{
		if (T < 0.6) { break; }
		if (!Weapons || Weapons->GetDefs().Num() == 0) { Next(Done); break; }
		if (Me->GetCombat()) { Me->GetCombat()->bInvulnerable = true; }
		if (ABullyCombatPawn* E = UBullyCombatTest::SpawnEnemyInFront(W, Me, 650.f, 0.f))
		{
			E->bAttackAllowed = false;
			E->bBlockAllowed = false;
			Target = E;
		}
		Index = 0;
		Next(WeaponHeld);
		break;
	}
	case WeaponHeld:
	{
		if (T < 0.8) { break; }
		const FBullyWeaponDef& D = Weapons->GetDefs()[Index];
		Weapons->RefillAll();
		Weapons->ResetCooldown();
		Weapons->Equip(D.Id);
		if (ABullyCombatPawn* E = Target.Get())
		{
			E->GetCombat()->ResetFighter();
			E->SetActorLocation(Me->GetActorLocation() + Me->GetActorForwardVector() * 650.f, false, nullptr, ETeleportType::TeleportPhysics);
			if (E->GetCharacterMovement()) { E->GetCharacterMovement()->StopMovementImmediately(); }
		}
		const FVector P = Me->GetActorLocation();
		Frame(P + Me->GetActorForwardVector() * 170.f + Me->GetActorRightVector() * 110.f + FVector(0, 0, 35.f), P + Me->GetActorRightVector() * 15.f + FVector(0, 0, 20.f), 50.f);
		Next(WeaponFire);
		break;
	}
	case WeaponFire:
	{
		if (T < 0.6) { break; }
		const FBullyWeaponDef& D = Weapons->GetDefs()[Index];
		Shot(FString::Printf(TEXT("held_%s"), *D.Id.ToString()));
		ABullyCombatPawn* E = Target.Get();
		const FVector P = Me->GetActorLocation();
		const FVector Aim = E ? E->GetActorLocation() + FVector(0, 0, 40.f) : P + Me->GetActorForwardVector() * 650.f;
		Frame(P - Me->GetActorForwardVector() * 120.f + Me->GetActorRightVector() * 330.f + FVector(0, 0, 60.f), (P * 0.7f + Aim * 0.3f), 62.f);
		DetonationsBefore = ABullyProjectile::TotalDetonations;
		if (!Weapons->FireAt(Aim)) { UE_LOG(LogBullyShowcase, Warning, TEXT("BR_SHOWCASE %s did not fire"), *D.Name); }
		Next(WeaponRelease);
		break;
	}
	case WeaponRelease:
	{
		const FBullyWeaponDef& D = Weapons->GetDefs()[Index];
		if (T < D.ReleaseSeconds / D.FireRate + 0.06f) { break; }
		Shot(FString::Printf(TEXT("throw_%s"), *D.Id.ToString()));
		Next(WeaponImpact);
		break;
	}
	case WeaponImpact:
	{
		const FBullyWeaponDef& D = Weapons->GetDefs()[Index];
		const bool bBoom = ABullyProjectile::TotalDetonations > DetonationsBefore;
		if (!bBoom && T < 4.0) { break; }
		if (ABullyCombatPawn* E = Target.Get())
		{
			const FVector P = E->GetActorLocation();
			const FVector Fwd = (P - Me->GetActorLocation()).GetSafeNormal2D();
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
			Frame(P - Fwd * 260.f + Right * 260.f + FVector(0, 0, 90.f), P - FVector(0, 0, 20.f), 60.f);
		}
		Next(WeaponNext);
		UE_LOG(LogBullyShowcase, Display, TEXT("BR_SHOWCASE %s %s"), *D.Name, bBoom ? TEXT("detonated") : TEXT("did NOT detonate"));
		break;
	}
	case WeaponNext:
		if (T < 0.18) { break; }
		Shot(FString::Printf(TEXT("impact_%s"), *Weapons->GetDefs()[Index].Id.ToString()));
		++Index;
		Next(Index < Weapons->GetDefs().Num() ? WeaponHeld : Done);
		break;

	case Done:
		if (T < 2.0) { break; }
		UE_LOG(LogBullyShowcase, Display, TEXT("BR_SHOWCASE done: %d shot(s) in %s; quitting"), Count, *Dir);
		Next(Quit);
		GEngine->Exec(W, TEXT("quit"));
		break;
	default:
		break;
	}
}
