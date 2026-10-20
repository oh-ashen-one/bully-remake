// OWNER: slice-weapons
#include "BullyFx.h"

#include "BullySfx.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

int32 ABullyFx::TotalSpawned = 0;

namespace
{
	UStaticMesh* SphereMesh()
	{
		static TWeakObjectPtr<UStaticMesh> S;
		if (!S.IsValid()) { S = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn | LOAD_Quiet); }
		return S.Get();
	}

	UMaterialInterface* FxMaterial(bool& bSoft)
	{
		static TWeakObjectPtr<UMaterialInterface> M;
		static bool bIsSoft = false;
		if (!M.IsValid())
		{
			M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Imported/Fx/M_BullyFx.M_BullyFx"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			bIsSoft = M.IsValid();
			if (!bIsSoft) { M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")); }
		}
		bSoft = bIsSoft;
		return M.Get();
	}
}

ABullyFx::ABullyFx()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ABullyFx* ABullyFx::Make(UWorld* W, const FVector& At)
{
	if (!W || W->bIsTearingDown) { return nullptr; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABullyFx* F = W->SpawnActor<ABullyFx>(ABullyFx::StaticClass(), At, FRotator::ZeroRotator, SP);
	if (F) { ++TotalSpawned; }
	return F;
}

ABullyFx::FPart& ABullyFx::AddSphere(const FVector& Loc, const FLinearColor& Color, float Diameter0, float Diameter1, float Life, float Opacity0, float Opacity1)
{
	FPart& P = Parts.AddDefaulted_GetRef();
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(SphereMesh());
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	C->SetWorldLocation(Loc);
	bool bSoft = false;
	if (UMaterialInterface* M = FxMaterial(bSoft))
	{
		P.MID = UMaterialInstanceDynamic::Create(M, this);
		// colours above 1 mean "glowing" (fire, flash, sparks): base colour normalised, the excess drives the emissive
		const float Peak = FMath::Max3(Color.R, Color.G, Color.B);
		const bool bGlow = Peak > 1.01f;
		P.MID->SetVectorParameterValue(TEXT("Color"), bGlow ? FLinearColor(Color.R / Peak, Color.G / Peak, Color.B / Peak) : Color);
		P.MID->SetScalarParameterValue(TEXT("Glow"), bGlow ? Peak * 12.f : 0.f);
		P.MID->SetScalarParameterValue(TEXT("Opacity"), Opacity0);
		C->SetMaterial(0, P.MID);
	}
	P.Comp = C;
	P.Color = Color;
	P.Scale0 = FVector(Diameter0 / 100.f);
	P.Scale1 = FVector(Diameter1 / 100.f);
	P.Opacity0 = Opacity0;
	P.Opacity1 = Opacity1;
	P.Life = FMath::Max(0.05f, Life);
	C->SetWorldScale3D(P.Scale0);
	return P;
}

ABullyFx::FPart& ABullyFx::AddMesh(UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, float Life)
{
	FPart& P = Parts.AddDefaulted_GetRef();
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	C->SetWorldLocationAndRotation(Loc, Rot);
	P.Comp = C;
	P.Life = Life;
	P.Opacity0 = P.Opacity1 = -1.f;   // opaque mesh: no fade, just disappears at the end
	return P;
}

void ABullyFx::Flash(const FLinearColor& Color, float Intensity, float Radius, float Seconds)
{
	Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(RootComponent);
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(Radius);
	Light->SetCastShadows(false);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(Intensity);
	Light->RegisterComponent();
	LightIntensity = Intensity;
	LightLife = Seconds;
	LightAge = 0.f;
}

void ABullyFx::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	bool bAlive = false;
	UWorld* W = GetWorld();
	for (FPart& P : Parts)
	{
		if (!P.Comp) { continue; }
		P.Age += Dt;
		if (P.Age < P.Delay) { P.Comp->SetVisibility(false); bAlive = true; continue; }
		const float T = FMath::Clamp((P.Age - P.Delay) / P.Life, 0.f, 1.f);
		if (T >= 1.f) { P.Comp->DestroyComponent(); P.Comp = nullptr; continue; }
		bAlive = true;
		P.Comp->SetVisibility(true);
		if (!P.Velocity.IsNearlyZero() || P.Gravity > 0.f)
		{
			P.Velocity.Z -= P.Gravity * Dt;
			FVector Loc = P.Comp->GetComponentLocation();
			FVector Next = Loc + P.Velocity * Dt;
			if (P.bGroundStop && W)
			{
				FHitResult Hit;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(BullyFx), false);
				if (W->LineTraceSingleByChannel(Hit, Loc, Next - FVector(0, 0, 2.f), ECC_WorldStatic, Q))
				{
					Next = Hit.ImpactPoint + Hit.ImpactNormal * 2.f;
					P.Velocity = FVector::VectorPlaneProject(P.Velocity, Hit.ImpactNormal) * 0.55f;   // skid to a stop
					if (P.Velocity.SizeSquared() < 100.f) { P.Velocity = FVector::ZeroVector; P.Gravity = 0.f; }
				}
			}
			else
			{
				P.Velocity *= FMath::Max(0.f, 1.f - 2.2f * Dt);   // air drag for smoke
			}
			P.Comp->SetWorldLocation(Next);
		}
		const float E = 1.f - FMath::Square(1.f - T);       // ease out
		if (P.Scale0 != P.Scale1) { P.Comp->SetWorldScale3D(FMath::Lerp(P.Scale0, P.Scale1, E)); }
		if (P.MID && P.Opacity0 >= 0.f) { P.MID->SetScalarParameterValue(TEXT("Opacity"), FMath::Lerp(P.Opacity0, P.Opacity1, T)); }
	}
	if (Light)
	{
		LightAge += Dt;
		const float K = FMath::Clamp(1.f - LightAge / FMath::Max(0.01f, LightLife), 0.f, 1.f);
		Light->SetIntensity(LightIntensity * K * K);
		if (K > 0.f) { bAlive = true; }
	}
	if (LoopAudio)
	{
		if (AudioStopAt >= 0.f && Age >= AudioStopAt) { LoopAudio->FadeOut(0.4f, 0.f); AudioStopAt = -1.f; }
		if (LoopAudio->IsPlaying()) { bAlive = true; }
	}
	if (!bAlive) { Destroy(); }
}

// ---------------------------------------------------------------------------------------------------------------
// recipes

ABullyFx* ABullyFx::Puff(UWorld* W, const FVector& At, const FLinearColor& Color, float Radius, float Seconds, float Opacity, float Rise)
{
	ABullyFx* F = Make(W, At);
	if (!F) { return nullptr; }
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Off = FMath::VRand() * Radius * 0.35f;
		FPart& P = F->AddSphere(At + Off, Color, Radius * 0.6f, Radius * FMath::FRandRange(1.6f, 2.2f), Seconds * FMath::FRandRange(0.8f, 1.15f), Opacity);
		P.Velocity = Off.GetSafeNormal() * Radius * 0.8f + FVector(0, 0, Rise);
		P.Delay = i * 0.03f;
	}
	return F;
}

ABullyFx* ABullyFx::Burst(UWorld* W, const FVector& At, const FVector& Normal, const FLinearColor& Color, int32 Count, float Speed, float Size)
{
	ABullyFx* F = Make(W, At);
	if (!F) { return nullptr; }
	const FVector N = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
	for (int32 i = 0; i < Count; ++i)
	{
		FVector Dir = (FMath::VRand() + N * 1.2f).GetSafeNormal();
		FPart& P = F->AddSphere(At, Color, Size, Size * 0.7f, FMath::FRandRange(0.5f, 0.9f), 1.f, 0.9f);
		P.Velocity = Dir * Speed * FMath::FRandRange(0.5f, 1.f);
		P.Gravity = 980.f;
		P.bGroundStop = true;
	}
	return F;
}

ABullyFx* ABullyFx::Blast(UWorld* W, const FVector& At, float Radius, const FLinearColor& FireColor)
{
	ABullyFx* F = Make(W, At);
	if (!F) { return nullptr; }
	F->AddSphere(At, FireColor * 6.f, Radius * 0.25f, Radius * 0.9f, 0.18f, 0.7f);                      // fireball
	F->AddSphere(At, FLinearColor(1.f, 0.95f, 0.8f) * 12.f, Radius * 0.15f, Radius * 0.45f, 0.08f, 0.9f); // core flash
	for (int32 i = 0; i < 6; ++i)                                                                         // smoke
	{
		const FVector Off = FMath::VRand() * Radius * 0.3f;
		FPart& P = F->AddSphere(At + Off, FLinearColor(0.12f, 0.11f, 0.1f), Radius * 0.4f, Radius * FMath::FRandRange(1.2f, 1.8f), FMath::FRandRange(1.4f, 2.2f), 0.5f);
		P.Delay = 0.08f + i * 0.02f;
		P.Velocity = Off.GetSafeNormal() * Radius * 0.6f + FVector(0, 0, 70.f);
	}
	for (int32 i = 0; i < 8; ++i)                                                                         // sparks
	{
		FPart& P = F->AddSphere(At, FireColor * 10.f, 6.f, 3.f, FMath::FRandRange(0.3f, 0.6f), 1.f, 1.f);
		P.Velocity = (FMath::VRand() + FVector(0, 0, 0.6f)).GetSafeNormal() * FMath::FRandRange(400.f, 900.f);
		P.Gravity = 980.f;
	}
	F->Flash(FireColor, 4000.f, Radius * 4.f, 0.35f);
	return F;
}

ABullyFx* ABullyFx::Splat(UWorld* W, const FVector& At, const FVector& Normal, const FLinearColor& Color, float Radius, float Seconds)
{
	ABullyFx* F = Make(W, At);
	if (!F) { return nullptr; }
	const FVector N = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
	FPart& Disc = F->AddSphere(At + N * 0.6f, Color, Radius * 0.5f, Radius * 1.1f, Seconds, 0.85f, 0.f);
	Disc.Comp->SetWorldRotation(FRotationMatrix::MakeFromZ(N).Rotator());
	Disc.Scale0 = FVector(Radius * 0.5f / 100.f, Radius * 0.5f / 100.f, 0.008f);   // a thin smear on the surface
	Disc.Scale1 = FVector(Radius * 1.1f / 100.f, Radius * 1.1f / 100.f, 0.008f);
	for (int32 i = 0; i < 6; ++i)                                                                         // droplets
	{
		FPart& P = F->AddSphere(At, Color, 5.f, 4.f, FMath::FRandRange(0.35f, 0.6f), 1.f, 0.8f);
		P.Velocity = (FMath::VRand() + N * 1.4f).GetSafeNormal() * FMath::FRandRange(150.f, 380.f);
		P.Gravity = 980.f;
	}
	return F;
}

ABullyFx* ABullyFx::Cloud(UWorld* W, const FVector& At, const FLinearColor& Color, float Radius, float Seconds, FName LoopSfx)
{
	ABullyFx* F = Make(W, At);
	if (!F) { return nullptr; }
	for (int32 i = 0; i < 9; ++i)
	{
		const FVector Off = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(0.f, 0.5f)) * Radius * 0.6f;
		FPart& P = F->AddSphere(At + Off + FVector(0, 0, 40.f), Color, Radius * 0.5f, Radius * FMath::FRandRange(0.9f, 1.3f), Seconds * FMath::FRandRange(0.85f, 1.1f), 0.42f, 0.f);
		P.Delay = i * 0.06f;
		P.Velocity = FVector(Off.X, Off.Y, 25.f) * 0.25f;
	}
	if (!LoopSfx.IsNone())
	{
		F->LoopAudio = FBullySfx::Loop(F, F->GetRootComponent(), LoopSfx);
		F->AudioStopAt = Seconds;
	}
	return F;
}

ABullyFx* ABullyFx::Scatter(UWorld* W, const FVector& At, UStaticMesh* Mesh, int32 Count, float Radius, float Seconds)
{
	ABullyFx* F = Make(W, At);
	if (!F || !Mesh) { return F; }
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Dir = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
		FPart& P = F->AddMesh(Mesh, At + FVector(0, 0, 10.f), FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f), Seconds);
		P.Velocity = Dir * Radius * FMath::FRandRange(1.5f, 3.f) + FVector(0, 0, 120.f);
		P.Gravity = 980.f;
		P.bGroundStop = true;
	}
	return F;
}
