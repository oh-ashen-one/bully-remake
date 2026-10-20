// OWNER: slice-town
#include "BullyLanding.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyLanding, Log, All);

namespace
{
	constexpr float BandUp = 3000.f;     // look this far above the spot (the spot itself may sit under the street)
	constexpr float BandDown = 3000.f;   // and this far below
	constexpr float StoreyGap = 200.f;   // a walkable surface this far over another one is a roof / deck / upper floor

	bool IsWater(const FHitResult& H)
	{
		const AActor* A = H.GetActor();
		if (!A) { return false; }
		for (const FName& T : A->Tags) { if (T.ToString().Contains(TEXT("water"), ESearchCase::IgnoreCase)) { return true; } }
		return A->GetName().Contains(TEXT("water"), ESearchCase::IgnoreCase);
	}

	/** Every surface hit straight down through the band at XY, top to bottom. */
	void Surfaces(const UWorld* W, const FVector2D& XY, float Top, float Bottom, const AActor* Ignore, TArray<FHitResult>& Out)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BullyLanding), true, Ignore);
		float From = Top;
		for (int32 i = 0; i < 12 && From > Bottom; ++i)
		{
			FHitResult H;
			if (!W->LineTraceSingleByObjectType(H, FVector(XY, From), FVector(XY, Bottom), FCollisionObjectQueryParams(ECC_WorldStatic), Q)) { break; }
			Out.Add(H);
			From = H.ImpactPoint.Z - 25.f;   // continue under this surface (thin roofs, floors)
		}
	}
}

bool FBullyLanding::Find(const UWorld* W, const FVector& Spot, float Radius, FVector& OutGround, const AActor* Ignore, float ClearHalf)
{
	if (!W) { return false; }
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BullyLandingFit), false, Ignore);
	FCollisionObjectQueryParams Blockers;
	Blockers.AddObjectTypesToQuery(ECC_WorldStatic);
	Blockers.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FHitResult> Hits;
	for (float Ring = 0.f; Ring <= Radius; Ring += 150.f)
	{
		const int32 Count = Ring < 1.f ? 1 : FMath::Max(8, int32(2.f * PI * Ring / 200.f));
		for (int32 k = 0; k < Count; ++k)
		{
			const float A = 2.f * PI * k / Count;
			const FVector2D XY(Spot.X + Ring * FMath::Cos(A), Spot.Y + Ring * FMath::Sin(A));
			Hits.Reset();
			Surfaces(W, XY, Spot.Z + BandUp, Spot.Z - BandDown, Ignore, Hits);
			for (int32 i = 0; i < Hits.Num(); ++i)
			{
				const FHitResult& H = Hits[i];
				if (H.ImpactNormal.Z < 0.8f || IsWater(H)) { continue; }
				// ground = nothing walkable under it (roofs, awnings, decks and upper floors all have something)
				bool bOver = false;
				for (int32 j = i + 1; j < Hits.Num(); ++j)
				{
					if (Hits[j].ImpactNormal.Z >= 0.8f && H.ImpactPoint.Z - Hits[j].ImpactPoint.Z > StoreyGap) { bOver = true; break; }
				}
				if (bOver) { continue; }
				// a standing capsule fits, and an open patch around it is clear of walls / props (not a corner or a gap)
				const FVector G = H.ImpactPoint;
				if (W->OverlapAnyTestByObjectType(G + FVector(0, 0, 95.f), FQuat::Identity, Blockers, FCollisionShape::MakeCapsule(36.f, 88.f), Q)) { continue; }
				if (W->OverlapAnyTestByObjectType(G + FVector(0, 0, 140.f), FQuat::Identity, Blockers, FCollisionShape::MakeBox(FVector(ClearHalf, ClearHalf, 110.f)), Q)) { continue; }
				// the patch is level ground, not a ledge
				bool bFlat = true;
				for (int32 c = 0; c < 4 && bFlat; ++c)
				{
					const FVector2D C = XY + FVector2D(c & 1 ? ClearHalf : -ClearHalf, c & 2 ? ClearHalf : -ClearHalf);
					FHitResult E;
					bFlat = W->LineTraceSingleByObjectType(E, FVector(C, G.Z + 80.f), FVector(C, G.Z - 120.f), FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(BullyLandingEdge), true, Ignore))
						&& FMath::Abs(E.ImpactPoint.Z - G.Z) < 60.f && !IsWater(E);
				}
				if (!bFlat) { continue; }
				OutGround = G;
				UE_LOG(LogBullyLanding, Display, TEXT("BR_LANDING ground %s (%.0f cm from the spot, %d surface(s) in the column)"), *G.ToCompactString(), Ring, Hits.Num());
				return true;
			}
		}
	}
	UE_LOG(LogBullyLanding, Warning, TEXT("BR_LANDING no open ground within %.0f cm of %s"), Radius, *Spot.ToCompactString());
	return false;
}
