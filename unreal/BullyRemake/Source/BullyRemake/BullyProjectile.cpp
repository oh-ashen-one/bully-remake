// OWNER: slice-weapons
#include "BullyProjectile.h"

#include "BullyFx.h"
#include "BullySfx.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyWeapon, Log, All);

int32 ABullyProjectile::TotalDetonations = 0;
int32 ABullyProjectile::TotalFighterHits = 0;
int32 ABullyProjectile::TotalEffectTicks = 0;

// ---------------------------------------------------------------------------------------------------------------
// spec parsing

EBullyHitReaction FBullyProjectileSpec::ParseReaction(const FString& S)
{
	if (S.Equals(TEXT("none"), ESearchCase::IgnoreCase)) { return EBullyHitReaction::None; }
	if (S.Equals(TEXT("stagger"), ESearchCase::IgnoreCase)) { return EBullyHitReaction::Stagger; }
	if (S.Equals(TEXT("knockdown"), ESearchCase::IgnoreCase)) { return EBullyHitReaction::Knockdown; }
	if (S.Equals(TEXT("ground"), ESearchCase::IgnoreCase)) { return EBullyHitReaction::GroundHit; }
	if (S.Equals(TEXT("ko"), ESearchCase::IgnoreCase)) { return EBullyHitReaction::KO; }
	return EBullyHitReaction::Flinch;
}

EBullyWeaponEffect FBullyProjectileSpec::ParseEffect(const FString& S)
{
	if (S.Equals(TEXT("stun"), ESearchCase::IgnoreCase)) { return EBullyWeaponEffect::Stun; }
	if (S.Equals(TEXT("gas"), ESearchCase::IgnoreCase)) { return EBullyWeaponEffect::Gas; }
	if (S.Equals(TEXT("slip"), ESearchCase::IgnoreCase)) { return EBullyWeaponEffect::Slip; }
	if (S.Equals(TEXT("fire"), ESearchCase::IgnoreCase)) { return EBullyWeaponEffect::Fire; }
	if (S.Equals(TEXT("knockback"), ESearchCase::IgnoreCase)) { return EBullyWeaponEffect::Knockback; }
	return EBullyWeaponEffect::None;
}

const TCHAR* FBullyProjectileSpec::EffectName(EBullyWeaponEffect E)
{
	switch (E)
	{
	case EBullyWeaponEffect::Stun: return TEXT("stun");
	case EBullyWeaponEffect::Gas: return TEXT("gas");
	case EBullyWeaponEffect::Slip: return TEXT("slip");
	case EBullyWeaponEffect::Fire: return TEXT("fire");
	case EBullyWeaponEffect::Knockback: return TEXT("knockback");
	default: return TEXT("none");
	}
}

namespace
{
	// Repeat Tick every Interval seconds for Total seconds on the world's timer manager. Self-clearing; safe if the
	// world goes away (weak capture). Used for effects that outlive the (already destroyed) projectile.
	void ScheduleTicks(UWorld* W, float Interval, float Total, TFunction<void()> Tick)
	{
		if (!W || Interval <= 0.f || Total < Interval) { return; }
		TSharedRef<int32> Left = MakeShared<int32>(FMath::Max(1, FMath::FloorToInt(Total / Interval)));
		TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
		TWeakObjectPtr<UWorld> WW = W;
		W->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda([WW, Handle, Left, Tick]()
		{
			if (!WW.IsValid()) { return; }
			Tick();
			if (--(*Left) <= 0) { WW->GetTimerManager().ClearTimer(*Handle); }
		}), Interval, true);
	}

	// Every live fighter in W within Radius of At, minus Skip.
	TArray<TWeakObjectPtr<UBullyCombatComponent>> FightersNear(UWorld* W, const FVector& At, float Radius, const UBullyCombatComponent* Skip)
	{
		TArray<TWeakObjectPtr<UBullyCombatComponent>> Out;
		for (const TWeakObjectPtr<UBullyCombatComponent>& C : UBullyCombatComponent::Registry())
		{
			if (!C.IsValid() || C.Get() == Skip || C->GetWorld() != W || !C->GetOwner()) { continue; }
			if (FVector::DistSquared(C->GetOwner()->GetActorLocation(), At) <= FMath::Square(Radius)) { Out.Add(C); }
		}
		return Out;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// actor

ABullyProjectile::ABullyProjectile()
{
	PrimaryActorTick.bCanEverTick = true;   // tumble + trail
	InitialLifeSpan = 0.f;   // set on Launch from the weapon's fuse

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(8.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->OnComponentHit.AddDynamic(this, &ABullyProjectile::OnHit);
	RootComponent = Collision;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(Collision);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetCastShadow(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->InitialSpeed = 0.f;
	Movement->MaxSpeed = 0.f;      // 0 = no cap; velocity is set explicitly on Launch
}

void ABullyProjectile::Launch(const FBullyProjectileSpec& InSpec, const FVector& Origin, const FVector& Velocity, UBullyCombatComponent* FiredBy)
{
	Spec = InSpec;
	Shooter = FiredBy;
	Collision->SetSphereRadius(FMath::Max(1.f, Spec.Radius));
	if (FiredBy && FiredBy->GetOwner())
	{
		Collision->IgnoreActorWhenMoving(FiredBy->GetOwner(), true);   // never clip the thrower on the way out
	}
	SetActorLocation(Origin);
	Movement->ProjectileGravityScale = Spec.GravityScale;
	Movement->Velocity = Velocity;
	SetLifeSpan(FMath::Max(0.25f, Spec.Lifetime));
	UE_LOG(LogBullyWeapon, Display, TEXT("BR_WEAPON %s launched from %s v=%s (%.0f cm/s) grav=%.2f aoe=%.0f dmg=%.0f effect=%s"),
		*Spec.WeaponId.ToString(), *Origin.ToCompactString(), *Velocity.ToCompactString(), Velocity.Size(), Spec.GravityScale, Spec.AoeRadius, Spec.Damage, FBullyProjectileSpec::EffectName(Spec.Effect));
}

void ABullyProjectile::SetVisual(UStaticMesh* Mesh, bool bIsFallbackSphere)
{
	if (!MeshComp || !Mesh) { return; }
	MeshComp->SetStaticMesh(Mesh);
	// The engine sphere is 100 cm across: scale it to the collision diameter so what you see is what hits.
	MeshComp->SetRelativeScale3D(bIsFallbackSphere ? FVector(FMath::Max(1.f, Spec.Radius) * 2.f / 100.f) : FVector(1.f));
	MeshComp->SetRelativeRotation(FRotator(0.f, bIsFallbackSphere ? 0.f : Spec.MeshYaw, 0.f));
}

void ABullyProjectile::Tick(float Dt)
{
	Super::Tick(Dt);
	if (bDetonated) { return; }
	if (Spec.SpinDps != 0.f && MeshComp)
	{
		Spin = FMath::Fmod(Spin + Spec.SpinDps * Dt, 360.f);
		MeshComp->SetRelativeRotation(FRotator(Spin, Spec.MeshYaw, Spin * 0.37f));
	}
	if (Spec.bTrail)
	{
		TrailClock += Dt;
		if (TrailClock >= 0.035f)
		{
			TrailClock = 0.f;
			ABullyFx::Puff(GetWorld(), GetActorLocation(), FLinearColor(0.55f, 0.53f, 0.5f), 10.f, 0.7f, 0.35f, 15.f);
		}
	}
}

void ABullyProjectile::OnHit(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	if (bDetonated) { return; }
	UBullyCombatComponent* Direct = Other ? Other->FindComponentByClass<UBullyCombatComponent>() : nullptr;
	Detonate(Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(Hit.ImpactPoint), Direct,
		Hit.ImpactNormal.IsNearlyZero() ? FVector::UpVector : FVector(Hit.ImpactNormal));
}

void ABullyProjectile::LifeSpanExpired()
{
	// Fuse ran out (thrown into open air / over a ledge): go off where it is rather than vanish.
	if (!bDetonated) { Detonate(GetActorLocation(), nullptr); }
	Super::LifeSpanExpired();
}

void ABullyProjectile::Detonate(const FVector& At, UBullyCombatComponent* DirectHit, const FVector& Normal)
{
	if (bDetonated) { return; }
	bDetonated = true;
	++TotalDetonations;
	UWorld* W = GetWorld();

	// Marbles: the "hit" is the slip itself, so it lands as a knockdown.
	const EBullyHitReaction Reaction = Spec.Effect == EBullyWeaponEffect::Slip ? EBullyHitReaction::Knockdown : Spec.Reaction;

	TArray<TWeakObjectPtr<UBullyCombatComponent>> Victims;
	if (Spec.AoeRadius > 0.f)
	{
		Victims = FightersNear(W, At, Spec.AoeRadius, Shooter.Get());
	}
	else if (DirectHit && DirectHit != Shooter.Get())
	{
		Victims.Add(DirectHit);
	}
	for (const TWeakObjectPtr<UBullyCombatComponent>& V : Victims)
	{
		if (V.IsValid()) { V->ApplyWeaponHit(Reaction, Spec.Damage); ++TotalFighterHits; }
	}

	if (Victims.Num() == 0 && Spec.AoeRadius > 0.f)
	{
		for (const TWeakObjectPtr<UBullyCombatComponent>& C : UBullyCombatComponent::Registry())
		{
			if (C.IsValid() && C->GetOwner() && C.Get() != Shooter.Get())
			{
				UE_LOG(LogBullyWeapon, Display, TEXT("BR_WEAPON %s missed %s at %s, %.0f cm (aoe %.0f, same world %d)"), *Spec.WeaponId.ToString(),
					*C->GetOwner()->GetName(), *C->GetOwner()->GetActorLocation().ToCompactString(), FVector::Dist(C->GetOwner()->GetActorLocation(), At), Spec.AoeRadius, C->GetWorld() == W);
			}
		}
	}
	UE_LOG(LogBullyWeapon, Display, TEXT("BR_WEAPON %s detonated at %s: %d fighter(s) hit, effect=%s %.1fs"),
		*Spec.WeaponId.ToString(), *At.ToCompactString(), Victims.Num(), FBullyProjectileSpec::EffectName(Spec.Effect), Spec.EffectSeconds);

	ImpactFx(At, Normal, Victims.Num());
	ApplyEffect(At, Victims);
	Destroy();
}

void ABullyProjectile::ImpactFx(const FVector& At, const FVector& Normal, int32 Victims)
{
	UWorld* W = GetWorld();
	if (!Spec.SfxImpact.IsNone()) { FBullySfx::Play(this, Spec.SfxImpact, At); }
	const FLinearColor C = Spec.FxColor;
	const float Aoe = FMath::Max(Spec.AoeRadius, 60.f);
	if (Spec.Fx == TEXT("dust")) { ABullyFx::Puff(W, At, C, 18.f, 0.6f, 0.5f, 25.f); }
	else if (Spec.Fx == TEXT("chunks")) { ABullyFx::Burst(W, At, Normal, C, 10, 520.f, 7.f); ABullyFx::Puff(W, At, C * 0.8f, 22.f, 0.5f, 0.45f, 20.f); }
	else if (Spec.Fx == TEXT("blast")) { ABullyFx::Blast(W, At, Aoe * 0.55f, C); }
	else if (Spec.Fx == TEXT("bang")) { ABullyFx::Blast(W, At, Aoe * 0.4f, C); ABullyFx::Puff(W, At, FLinearColor(0.2f, 0.19f, 0.18f), 40.f, 1.6f, 0.5f, 60.f); }
	else if (Spec.Fx == TEXT("splat")) { ABullyFx::Splat(W, At, Normal, C, 22.f, 4.f); }
	else if (Spec.Fx == TEXT("gas")) { ABullyFx::Cloud(W, At, C, Aoe, FMath::Max(1.f, Spec.EffectSeconds), TEXT("gas_stinkbomb")); }
	else if (Spec.Fx == TEXT("powder")) { ABullyFx::Puff(W, At, C, Aoe * 0.35f, 2.2f, 0.55f, 30.f); }
	else if (Spec.Fx == TEXT("scatter"))
	{
		ABullyFx::Scatter(W, At, MeshComp ? MeshComp->GetStaticMesh() : nullptr, 14, Aoe * 0.5f, FMath::Max(2.f, Spec.EffectSeconds));
	}
}

void ABullyProjectile::ApplyEffect(const FVector& At, const TArray<TWeakObjectPtr<UBullyCombatComponent>>& Victims)
{
	UWorld* W = GetWorld();
	const FName Id = Spec.WeaponId;
	TWeakObjectPtr<UBullyCombatComponent> Skip = Shooter;

	switch (Spec.Effect)
	{
	case EBullyWeaponEffect::Knockback:
		for (const TWeakObjectPtr<UBullyCombatComponent>& V : Victims)
		{
			if (ACharacter* C = V.IsValid() ? Cast<ACharacter>(V->GetOwner()) : nullptr)
			{
				const FVector Away = (C->GetActorLocation() - At).GetSafeNormal2D();
				C->LaunchCharacter(Away * 700.f + FVector(0.f, 0.f, 280.f), true, true);
			}
		}
		break;

	case EBullyWeaponEffect::Gas:
	{
		// A lingering cloud: anyone standing in it keeps coughing (flinch + a little damage) until it clears.
		const float Radius = FMath::Max(150.f, Spec.AoeRadius);
		ScheduleTicks(W, 1.0f, Spec.EffectSeconds, [W, At, Radius, Skip, Id]()
		{
			for (const TWeakObjectPtr<UBullyCombatComponent>& V : FightersNear(W, At, Radius, Skip.Get()))
			{
				if (V.IsValid()) { V->ApplyWeaponHit(EBullyHitReaction::Flinch, 0.5f); ++ABullyProjectile::TotalEffectTicks; }
			}
			UE_LOG(LogBullyWeapon, Verbose, TEXT("BR_WEAPON %s gas tick"), *Id.ToString());
		});
		break;
	}

	case EBullyWeaponEffect::Fire:
		// Burning: the victims flail and take small damage until it burns out.
		ScheduleTicks(W, 0.5f, Spec.EffectSeconds, [Victims, Id]()
		{
			for (const TWeakObjectPtr<UBullyCombatComponent>& V : Victims)
			{
				if (V.IsValid() && !V->IsDown()) { V->ApplyWeaponHit(EBullyHitReaction::Flinch, 0.5f); ++ABullyProjectile::TotalEffectTicks; }
				if (V.IsValid() && V->GetOwner())   // they smoulder while they burn
				{
					ABullyFx::Puff(V->GetWorld(), V->GetOwner()->GetActorLocation() + FMath::VRand() * 20.f + FVector(0, 0, 30.f),
						FLinearColor(3.f, 1.1f, 0.2f), 9.f, 0.4f, 0.8f, 90.f);
				}
			}
		});
		break;

	case EBullyWeaponEffect::Stun:
		// Itching powder: repeated stun beats for the duration (a quick slingshot sting, < 1 s, is just the hit).
		ScheduleTicks(W, 0.75f, Spec.EffectSeconds, [Victims]()
		{
			for (const TWeakObjectPtr<UBullyCombatComponent>& V : Victims)
			{
				if (V.IsValid() && !V->IsDown()) { V->ApplyWeaponHit(EBullyHitReaction::Flinch, 0.f); ++ABullyProjectile::TotalEffectTicks; }
			}
		});
		break;

	case EBullyWeaponEffect::Slip:   // delivered as the knockdown in Detonate
	case EBullyWeaponEffect::None:
	default:
		break;
	}
}
