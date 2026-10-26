// OWNER: slice-weapons
#include "BullyWeaponComponent.h"
#include "BullyStorySubsystem.h"

#include "BullyAnimInstance.h"
#include "BullySfx.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "BullyCharacter.h"
#include "BullyCombatComponent.h"
#include "BullyCombatPawn.h"
#include "BullyCombatTest.h"
#include "Camera/PlayerCameraManager.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyWeapons, Log, All);

TWeakObjectPtr<UBullyWeaponComponent> UBullyWeaponComponent::LocalArmed;

UBullyWeaponComponent::UBullyWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
}

void UBullyWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	FallbackSphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!LoadDefs()) { return; }
	// Sandbox/harness runs: every weapon unlocked from the start. Story mode (-BRStory): fists only -- weapons arrive
	// through the campaign's give_weapon/give_ammo grants (1_03 slingshot, ...) and a Continue restores the saved ammo.
	const bool bStory = UBullyStorySubsystem::IsStoryRunRequested();
	for (const FBullyWeaponDef& D : Defs) { Ammo.Add(D.Id, bStory ? 0 : D.AmmoStart); }
	Smoke.bEnabled = FParse::Param(FCommandLine::Get(), TEXT("BRWeaponSmoke"));
}

void UBullyWeaponComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (LocalArmed.Get() == this) { LocalArmed.Reset(); }
	Super::EndPlay(Reason);
}

bool UBullyWeaponComponent::LoadDefs()
{
	Defs.Reset();
	FString Text;
	const FString Path = FPaths::ProjectConfigDir() / TEXT("Weapons.json");
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogBullyWeapons, Warning, TEXT("BR_WEAPON no %s: arsenal empty"), *Path);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { return false; }
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("weapons"), Arr)) { return false; }
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
		if (!O.IsValid()) { continue; }
		FBullyWeaponDef D;
		FString S;
		double N = 0.0;
		if (!O->TryGetStringField(TEXT("id"), S) || S.IsEmpty()) { continue; }
		D.Id = FName(*S);
		D.Spec.WeaponId = D.Id;
		D.Name = O->TryGetStringField(TEXT("name"), S) ? S : D.Id.ToString();
		D.bThrown = O->TryGetStringField(TEXT("category"), S) && S.Equals(TEXT("thrown"), ESearchCase::IgnoreCase);
		if (O->TryGetNumberField(TEXT("speed_cm_s"), N)) { D.Spec.Speed = float(N); }
		if (O->TryGetNumberField(TEXT("gravity"), N)) { D.Spec.GravityScale = float(N); }
		if (O->TryGetNumberField(TEXT("radius_cm"), N)) { D.Spec.Radius = float(N); }
		if (O->TryGetNumberField(TEXT("lifetime_s"), N)) { D.Spec.Lifetime = float(N); }
		if (O->TryGetNumberField(TEXT("damage"), N)) { D.Spec.Damage = float(N); }
		if (O->TryGetStringField(TEXT("reaction"), S)) { D.Spec.Reaction = FBullyProjectileSpec::ParseReaction(S); }
		if (O->TryGetNumberField(TEXT("aoe_cm"), N)) { D.Spec.AoeRadius = float(N); }
		if (O->TryGetStringField(TEXT("effect"), S)) { D.Spec.Effect = FBullyProjectileSpec::ParseEffect(S); }
		if (O->TryGetNumberField(TEXT("effect_s"), N)) { D.Spec.EffectSeconds = float(N); }
		if (O->TryGetNumberField(TEXT("ammo"), N)) { D.AmmoStart = int32(N); }
		if (O->TryGetNumberField(TEXT("ammo_max"), N)) { D.AmmoMax = int32(N); }
		if (O->TryGetNumberField(TEXT("fire_interval_s"), N)) { D.FireInterval = float(N); }
		if (O->TryGetNumberField(TEXT("throw_pitch_deg"), N)) { D.ThrowPitchDeg = float(N); }
		O->TryGetStringField(TEXT("mesh"), D.MeshPath);
		if (O->TryGetNumberField(TEXT("mesh_yaw"), N)) { D.Spec.MeshYaw = float(N); }
		if (O->TryGetNumberField(TEXT("spin_dps"), N)) { D.Spec.SpinDps = float(N); }
		O->TryGetBoolField(TEXT("trail"), D.Spec.bTrail);
		if (O->TryGetStringField(TEXT("fx"), S)) { D.Spec.Fx = FName(*S); }
		if (O->TryGetStringField(TEXT("sfx_impact"), S)) { D.Spec.SfxImpact = FName(*S); }
		if (O->TryGetStringField(TEXT("sfx_fire"), S)) { D.SfxFire = FName(*S); }
		if (O->TryGetStringField(TEXT("fire_clip"), S)) { D.FireClip = FName(*S); }
		if (O->TryGetStringField(TEXT("hold_clip"), S)) { D.HoldClip = FName(*S); }
		if (O->TryGetNumberField(TEXT("fire_rate"), N)) { D.FireRate = FMath::Max(0.1f, float(N)); }
		if (O->TryGetNumberField(TEXT("release_s"), N)) { D.ReleaseSeconds = FMath::Max(0.f, float(N)); }
		O->TryGetStringField(TEXT("held_mesh"), D.HeldMeshPath);
		if (O->TryGetNumberField(TEXT("held_scale"), N)) { D.HeldScale = float(N); }
		auto Arr3 = [&O](const TCHAR* Key, FVector& Out)
		{
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (O->TryGetArrayField(Key, A) && A->Num() == 3) { Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber()); }
		};
		FVector Col(D.Spec.FxColor.R, D.Spec.FxColor.G, D.Spec.FxColor.B), Rot(0.f);
		Arr3(TEXT("fx_color"), Col);
		D.Spec.FxColor = FLinearColor(Col.X, Col.Y, Col.Z);
		Arr3(TEXT("held_offset_cm"), D.HeldOffset);
		Arr3(TEXT("held_rot"), Rot);   // pitch, yaw, roll
		D.HeldRot = FRotator(Rot.X, Rot.Y, Rot.Z);
		D.AmmoMax = FMath::Max(D.AmmoMax, D.AmmoStart);
		Defs.Add(D);
	}
	UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON arsenal loaded: %d weapon(s) from %s"), Defs.Num(), *Path);
	return Defs.Num() > 0;
}

// ---------------------------------------------------------------------------------------------------------------
// equip / cycle

bool UBullyWeaponComponent::Equip(FName Id)
{
	if (Id.IsNone()) { Equipped = -1; return true; }
	for (int32 i = 0; i < Defs.Num(); ++i)
	{
		if (Defs[i].Id == Id)
		{
			if (GetAmmo(Id) <= 0) { return false; }
			Equipped = i;
			UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON equip %s (%d/%d)"), *Defs[i].Name, GetAmmo(Id), Defs[i].AmmoMax);
			return true;
		}
	}
	return false;
}

void UBullyWeaponComponent::Cycle(int32 Dir)
{
	// Ring: Fists, then each weapon that still has ammo.
	TArray<int32> Ring = { -1 };
	for (int32 i = 0; i < Defs.Num(); ++i) { if (GetAmmo(Defs[i].Id) > 0) { Ring.Add(i); } }
	int32 Pos = Ring.IndexOfByKey(Equipped);
	if (Pos == INDEX_NONE) { Pos = 0; }
	Pos = (Pos + (Dir >= 0 ? 1 : -1) + Ring.Num()) % Ring.Num();
	Equipped = Ring[Pos];
	UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON cycle -> %s"), HasEquipped() ? *Defs[Equipped].Name : TEXT("Fists"));
}

void UBullyWeaponComponent::RefillAll()
{
	for (const FBullyWeaponDef& D : Defs) { Ammo.Add(D.Id, D.AmmoMax); }
	UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON refilled all %d weapon(s)"), Defs.Num());
}

bool UBullyWeaponComponent::GiveWeapon(FName Id)
{
	for (const FBullyWeaponDef& D : Defs)
	{
		if (D.Id != Id) { continue; }
		Ammo.Add(Id, FMath::Max(Ammo.FindRef(Id), D.AmmoStart));
		UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON give_weapon %s (ammo %d)"), *Id.ToString(), Ammo[Id]);
		return true;
	}
	UE_LOG(LogBullyWeapons, Warning, TEXT("BR_WEAPON give_weapon %s: unknown weapon id"), *Id.ToString());
	return false;
}

bool UBullyWeaponComponent::GiveAmmo(FName Id, int32 N)
{
	for (const FBullyWeaponDef& D : Defs)
	{
		if (D.Id != Id) { continue; }
		Ammo.Add(Id, FMath::Min(D.AmmoMax, FMath::Max(Ammo.FindRef(Id), N)));
		UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON give_ammo %s x%d -> %d"), *Id.ToString(), N, Ammo[Id]);
		return true;
	}
	UE_LOG(LogBullyWeapons, Warning, TEXT("BR_WEAPON give_ammo %s: unknown weapon id"), *Id.ToString());
	return false;
}

// ---------------------------------------------------------------------------------------------------------------
// aim + fire

FVector UBullyWeaponComponent::MuzzleLocation() const
{
	const AActor* O = GetOwner();
	const FVector Body = O->GetActorLocation() + O->GetActorForwardVector() * 45.f + FVector(0.f, 0.f, 55.f);   // chest height
	// the hand (or the weapon in it) at the release frame, kept in front of the chest so a shot never starts behind
	const ACharacter* C = Cast<ACharacter>(O);
	const USkeletalMeshComponent* SK = C ? C->GetMesh() : nullptr;
	if (SK && SK->GetBoneIndex(TEXT("Root-R-Hand")) != INDEX_NONE)
	{
		FVector Hand = SK->GetBoneLocation(TEXT("Root-R-Hand"));
		if (Held && Held->IsVisible() && Held->GetStaticMesh()) { Hand = Held->Bounds.Origin; }
		const FVector Fwd = O->GetActorForwardVector();
		const float Ahead = FVector::DotProduct(Hand - O->GetActorLocation(), Fwd);
		if (Ahead < 25.f) { Hand += Fwd * (25.f - Ahead); }
		if (FVector::Dist(Hand, Body) < 150.f) { return Hand; }
	}
	return Body;
}

bool UBullyWeaponComponent::AimPoint(FVector& Out) const
{
	const ABullyCharacter* C = Cast<ABullyCharacter>(GetOwner());
	UBullyCombatComponent* Combat = C ? C->GetCombat() : nullptr;
	if (Combat && Combat->IsLockedOn() && Combat->GetLockTarget() && Combat->GetLockTarget()->GetOwner())
	{
		Out = Combat->GetLockTarget()->GetOwner()->GetActorLocation() + FVector(0.f, 0.f, 40.f);   // their chest
		return true;
	}
	// Free aim: whatever is under the centre of the screen.
	const APawn* P = Cast<APawn>(GetOwner());
	const APlayerController* PC = P ? Cast<APlayerController>(P->GetController()) : nullptr;
	FVector Eye = MuzzleLocation();
	FVector Dir = GetOwner()->GetActorForwardVector();
	if (PC && PC->PlayerCameraManager)
	{
		Eye = PC->PlayerCameraManager->GetCameraLocation();
		Dir = PC->PlayerCameraManager->GetCameraRotation().Vector();
	}
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BullyWeaponAim), true, GetOwner());
	const FVector End = Eye + Dir * 8000.f;
	const bool bWorld = GetWorld()->LineTraceSingleByChannel(Hit, Eye, End, ECC_Visibility, Q);
	// sm-humandrive: people under the crosshair count too. Character capsules ignore the visibility channel, so the ray
	// went through a jock to the grass behind him and the shot, leaving the hand right of the camera, passed beside him
	// (1_04: every free-aimed pellet from the tree missed). Take the nearer of the world hit and a pawn hit.
	FHitResult PawnHit;
	if (GetWorld()->LineTraceSingleByObjectType(PawnHit, Eye, bWorld ? FVector(Hit.ImpactPoint) : End, FCollisionObjectQueryParams(ECC_Pawn), Q))
	{
		Out = PawnHit.ImpactPoint;
		return true;
	}
	if (bWorld)
	{
		Out = Hit.ImpactPoint;
		return true;
	}
	Out = End;
	return false;
}

bool UBullyWeaponComponent::CanFireNow() const
{
	if (bPending || !HasEquipped() || GetAmmo(Defs[Equipped].Id) <= 0) { return false; }
	if (GetWorld()->GetTimeSeconds() < NextFireAt) { return false; }
	const ABullyCharacter* C = Cast<ABullyCharacter>(GetOwner());
	const UBullyCombatComponent* Combat = C ? C->GetCombat() : nullptr;
	return !(Combat && (Combat->IsBusy() || Combat->IsDown()));   // no firing mid-reaction / knocked down
}

bool UBullyWeaponComponent::Fire()
{
	FVector Target;
	const bool bHasTarget = AimPoint(Target);
	const ABullyCharacter* C = Cast<ABullyCharacter>(GetOwner());
	UBullyCombatComponent* Combat = C ? C->GetCombat() : nullptr;
	PendingActor = Combat && Combat->IsLockedOn() && Combat->GetLockTarget() ? Combat->GetLockTarget()->GetOwner() : nullptr;
	return LaunchEquipped(Target, bHasTarget);
}

bool UBullyWeaponComponent::FireAt(const FVector& Target)
{
	PendingActor.Reset();
	return LaunchEquipped(Target, true);
}

bool UBullyWeaponComponent::FireAtActor(AActor* Target)
{
	if (!Target) { return false; }
	PendingActor = Target;
	return LaunchEquipped(Target->GetActorLocation() + FVector(0.f, 0.f, 40.f), true);
}

UStaticMesh* UBullyWeaponComponent::VisualFor(const FBullyWeaponDef& D, bool& bIsFallback)
{
	bIsFallback = true;
	if (!D.MeshPath.IsEmpty())
	{
		if (const TObjectPtr<UStaticMesh>* Hit = MeshCache.Find(D.Id)) { if (*Hit) { bIsFallback = false; return *Hit; } }
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *D.MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			MeshCache.Add(D.Id, M);
			bIsFallback = false;
			return M;
		}
	}
	return FallbackSphere;
}

UStaticMesh* UBullyWeaponComponent::LoadMesh(const FString& Path)
{
	if (Path.IsEmpty()) { return nullptr; }
	if (const TObjectPtr<UStaticMesh>* Hit = PathCache.Find(Path)) { return *Hit; }
	UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	PathCache.Add(Path, M);
	return M;
}

void UBullyWeaponComponent::UpdateHeld()
{
	ACharacter* C = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* SK = C ? C->GetMesh() : nullptr;
	if (!SK) { return; }
	if (!Held)
	{
		if (SK->GetBoneIndex(TEXT("Root-R-Hand")) == INDEX_NONE) { return; }
		Held = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("HeldWeapon"));
		Held->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Held->SetupAttachment(SK, TEXT("Root-R-Hand"));
		Held->RegisterComponent();
		Held->SetVisibility(false);
	}
	if (HeldShownFor != Equipped)
	{
		HeldShownFor = Equipped;
		if (UBullyAnimInstance* AI = Cast<UBullyAnimInstance>(SK->GetAnimInstance())) { AI->SetUpperHold(HasEquipped() ? Defs[Equipped].HoldClip : NAME_None); }
		UStaticMesh* M = HasEquipped() ? LoadMesh(Defs[Equipped].HeldMeshPath) : nullptr;
		Held->SetStaticMesh(M);
		if (M)
		{
			const FBullyWeaponDef& D = Defs[Equipped];
			Held->SetRelativeLocationAndRotation(D.HeldOffset, D.HeldRot);
			Held->SetRelativeScale3D(FVector(D.HeldScale));
		}
	}
	Held->SetVisibility(Held->GetStaticMesh() != nullptr && GetWorld()->GetTimeSeconds() >= HideHeldUntil);
}

bool UBullyWeaponComponent::LaunchEquipped(const FVector& Target, bool bHasTarget)
{
	if (!CanFireNow()) { return false; }
	const FBullyWeaponDef& D = Defs[Equipped];
	// face the shot now; the clip winds up and the projectile leaves at its release frame
	FRotator Face = GetOwner()->GetActorRotation();
	Face.Yaw = (Target - GetOwner()->GetActorLocation()).Rotation().Yaw;
	GetOwner()->SetActorRotation(Face);
	const ACharacter* Ch = Cast<ACharacter>(GetOwner());
	UBullyAnimInstance* AI = Ch && Ch->GetMesh() ? Cast<UBullyAnimInstance>(Ch->GetMesh()->GetAnimInstance()) : nullptr;
	if (!D.FireClip.IsNone() && AI && AI->PlayUpperBody(D.FireClip, D.FireRate) && D.ReleaseSeconds > 0.f)
	{
		bPending = true;
		PendingAt = GetWorld()->GetTimeSeconds() + D.ReleaseSeconds / D.FireRate;
		PendingTarget = Target;
		bPendingHasTarget = bHasTarget;
		NextFireAt = GetWorld()->GetTimeSeconds() + FMath::Max(D.FireInterval, D.ReleaseSeconds / D.FireRate + 0.05f);
		return true;
	}
	return LaunchNow(Target, bHasTarget);
}

bool UBullyWeaponComponent::LaunchNow(const FVector& Target, bool bHasTarget)
{
	bPending = false;
	if (!HasEquipped() || GetAmmo(Defs[Equipped].Id) <= 0) { return false; }
	UWorld* W = GetWorld();
	const FBullyWeaponDef& D = Defs[Equipped];
	const FVector Muzzle = MuzzleLocation();
	FVector ToTarget = Target - Muzzle;
	if (ToTarget.IsNearlyZero()) { ToTarget = GetOwner()->GetActorForwardVector(); }

	FVector Velocity;
	if (!D.bThrown)
	{
		Velocity = ToTarget.GetSafeNormal() * D.Spec.Speed;
	}
	else
	{
		bool bArc = false;
		if (bHasTarget && D.Spec.GravityScale > 0.f)
		{
			// A real lob that lands on the target (arc 0.5 = between flat and high).
			bArc = UGameplayStatics::SuggestProjectileVelocity_CustomArc(this, Velocity, Muzzle, Target, W->GetGravityZ() * D.Spec.GravityScale, 0.5f);
		}
		if (!bArc)
		{
			FRotator R = ToTarget.Rotation();
			R.Pitch = FMath::Clamp(R.Pitch + D.ThrowPitchDeg, -10.f, 60.f);
			Velocity = R.Vector() * D.Spec.Speed;
		}
	}

	// Face the throw.
	FRotator Yaw = GetOwner()->GetActorRotation();
	Yaw.Yaw = ToTarget.Rotation().Yaw;
	GetOwner()->SetActorRotation(Yaw);

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SP.Instigator = Cast<APawn>(GetOwner());
	ABullyProjectile* Proj = W->SpawnActor<ABullyProjectile>(ABullyProjectile::StaticClass(), Muzzle, Velocity.Rotation(), SP);
	if (!Proj) { return false; }
	const ABullyCharacter* C = Cast<ABullyCharacter>(GetOwner());
	Proj->Launch(D.Spec, Muzzle, Velocity, C ? C->GetCombat() : nullptr);
	bool bFallback = true;
	Proj->SetVisual(VisualFor(D, bFallback), bFallback);

	if (!D.SfxFire.IsNone()) { FBullySfx::Play(this, D.SfxFire, Muzzle); }
	if (D.bThrown) { HideHeldUntil = W->GetTimeSeconds() + 0.45; }   // it left the hand

	int32& A = Ammo.FindOrAdd(D.Id);
	A = FMath::Max(0, A - 1);
	NextFireAt = FMath::Max(NextFireAt, W->GetTimeSeconds() + D.FireInterval * 0.5);
	UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON fire %s (%s, %s) ammo %d/%d"),
		*D.Name, D.bThrown ? TEXT("thrown") : TEXT("ranged"), bHasTarget ? TEXT("aimed") : TEXT("free"), A, D.AmmoMax);
	if (A == 0)
	{
		Equipped = -1;   // out: back to fists
		UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON %s empty -> Fists"), *D.Name);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
// HUD

FString UBullyWeaponComponent::HudLine()
{
	const UBullyWeaponComponent* W = LocalArmed.Get();
	if (!W) { return FString(); }
	if (!W->HasEquipped()) { return TEXT("WEAPON  Fists    (mouse wheel / D-pad to pick a weapon)"); }
	const FBullyWeaponDef& D = W->Defs[W->Equipped];
	return FString::Printf(TEXT("WEAPON  %s  %d/%d    FIRE LMB"), *D.Name, W->GetAmmo(D.Id), D.AmmoMax);
}

// ---------------------------------------------------------------------------------------------------------------
// tick + headless smoke

void UBullyWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* P = Cast<APawn>(GetOwner());
	if (P && P->IsPlayerControlled() && P->IsLocallyControlled()) { LocalArmed = this; }
	if (bPending && GetWorld()->GetTimeSeconds() >= PendingAt)
	{
		// a locked-on target may have moved during the wind-up: throw where it is now
		if (const AActor* A = PendingActor.Get()) { PendingTarget = A->GetActorLocation() + FVector(0.f, 0.f, 40.f); bPendingHasTarget = true; }
		LaunchNow(PendingTarget, bPendingHasTarget);
	}
	UpdateHeld();
	if (Smoke.bEnabled && P && P->IsPlayerControlled()) { TickSmoke(); }
}

void UBullyWeaponComponent::TickSmoke()
{
	UWorld* W = GetWorld();
	const double Now = W->GetTimeSeconds();
	const double T = Now - Smoke.StepStart;
	ABullyCharacter* Me = Cast<ABullyCharacter>(GetOwner());
	auto Next = [&](int32 To) { Smoke.Step = To; Smoke.StepStart = Now; };
	auto Fail = [&](const FString& Why) { Smoke.Failures.Add(Why); UE_LOG(LogBullyWeapons, Error, TEXT("BR_WEAPON_SMOKE FAIL: %s"), *Why); };

	switch (Smoke.Step)
	{
	case 0:   // world settle, then a clear patch of ground (the academy drop-off, facing the courtyard)
		if (Now < 3.0) { break; }
		Me->SetActorLocation(FVector(29836.f, 7223.f, 700.f), false, nullptr, ETeleportType::TeleportPhysics);
		Me->SetActorRotation(FRotator(0.f, 180.f, 0.f));
		if (AController* Ctl = Me->GetController()) { Ctl->SetControlRotation(FRotator(0.f, 180.f, 0.f)); }
		if (Me->GetCombat()) { Me->GetCombat()->bInvulnerable = true; }
		UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON_SMOKE start: %d weapon(s)"), Defs.Num());
		Next(1);
		break;

	case 1:   // a passive target in front
		if (T < 1.0) { break; }
		if (ABullyCombatPawn* E = UBullyCombatTest::SpawnEnemyInFront(W, Me, 500.f, 0.f))
		{
			E->bAttackAllowed = false;
			E->bBlockAllowed = false;
			if (E->GetCharacterMovement()) { E->GetCharacterMovement()->MaxWalkSpeed = 0.f; }   // stands still through the wind-up
			Smoke.Target = E;
			Next(2);
		}
		else
		{
			Fail(TEXT("could not spawn a target"));
			Next(9);
		}
		break;

	case 2:   // per weapon: reset the target in front, then fire at its chest
	{
		if (T < 1.2) { break; }   // let the previous weapon's lingering effect clear
		ABullyCombatPawn* E = Smoke.Target.Get();
		if (!E || !Defs.IsValidIndex(Smoke.Weapon)) { Next(8); break; }
		const FBullyWeaponDef& D = Defs[Smoke.Weapon];
		E->GetCombat()->ResetFighter();
		E->SetActorLocation(Me->GetActorLocation() + Me->GetActorForwardVector() * 500.f, false, nullptr, ETeleportType::TeleportPhysics);
		if (E->GetCharacterMovement()) { E->GetCharacterMovement()->StopMovementImmediately(); }
		RefillAll();
		NextFireAt = 0.0;
		bPending = false;
		Smoke.HitsBefore = ABullyProjectile::TotalFighterHits;
		Smoke.DetonationsBefore = ABullyProjectile::TotalDetonations;
		Smoke.HealthBefore = E->GetCombat()->Health;
		if (!Equip(D.Id) || !FireAtActor(E))
		{
			Fail(FString::Printf(TEXT("%s: did not fire"), *D.Name));
			++Smoke.Weapon;
			Next(2);
			break;
		}
		Next(3);
		break;
	}

	case 3:   // wait for the detonation, then judge
	{
		const FBullyWeaponDef& D = Defs[Smoke.Weapon];
		const bool bDetonated = ABullyProjectile::TotalDetonations > Smoke.DetonationsBefore;
		if (!bDetonated && T < 4.0) { break; }
		ABullyCombatPawn* E = Smoke.Target.Get();
		const int32 Hits = ABullyProjectile::TotalFighterHits - Smoke.HitsBefore;
		const float HealthAfter = E ? E->GetCombat()->Health : 0.f;
		if (!bDetonated) { Fail(FString::Printf(TEXT("%s: never detonated"), *D.Name)); }
		else if (Hits <= 0) { Fail(FString::Printf(TEXT("%s: detonated but hit no one"), *D.Name)); }
		else
		{
			++Smoke.Passes;
			UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON_SMOKE PASS: %s hit %d fighter(s), target hp %.1f -> %.1f, ammo now %d"),
				*D.Name, Hits, Smoke.HealthBefore, HealthAfter, GetAmmo(D.Id));
		}
		++Smoke.Weapon;
		Next(Smoke.Weapon < Defs.Num() ? 2 : 8);
		break;
	}

	case 8:   // cycling: Fists -> first weapon -> back around
	{
		Equip(NAME_None);
		Cycle(+1);
		const bool bCycleOk = HasEquipped() && Equipped == 0;
		Cycle(-1);
		if (!bCycleOk || HasEquipped()) { Fail(TEXT("weapon cycle ring is wrong")); }
		else { UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON_SMOKE PASS: cycle Fists -> %s -> Fists"), *Defs[0].Name); }
		Next(9);
		break;
	}

	case 9:
		UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON_SMOKE RESULT %s: %d/%d weapons hit, %d effect tick(s), %d failure(s)"),
			Smoke.Failures.Num() == 0 ? TEXT("PASS") : TEXT("FAIL"), Smoke.Passes, Defs.Num(), ABullyProjectile::TotalEffectTicks, Smoke.Failures.Num());
		Smoke.bEnabled = false;
		GEngine->Exec(W, TEXT("quit"));
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// sm-humandrive: fists-only reset for the human-mode mission smoke

void UBullyWeaponComponent::EmptyAll()
{
	for (const FBullyWeaponDef& D : Defs) { Ammo.Add(D.Id, 0); }
	Equipped = -1;
	UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON emptied all %d weapon(s) (fists only)"), Defs.Num());
}

void UBullyWeaponComponent::RestoreAmmoTable(const TMap<FName, int32>& Table)
{
	Ammo = Table;
	if (const FBullyWeaponDef* E = GetEquipped(); E && GetAmmo(E->Id) <= 0) { Equipped = -1; }
	FString Line;
	for (const auto& P : Ammo) { if (P.Value > 0) { Line += FString::Printf(TEXT(" %s=%d"), *P.Key.ToString(), P.Value); } }
	UE_LOG(LogBullyWeapons, Display, TEXT("BR_WEAPON ammo restored:%s"), Line.IsEmpty() ? TEXT(" none") : *Line);
}
