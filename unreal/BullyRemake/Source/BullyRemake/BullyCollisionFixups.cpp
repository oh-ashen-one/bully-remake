// OWNER: sm-humandrive
#include "BullyCollisionFixups.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyCollisionFix, Log, All);

namespace
{
	// base-colour textures of the original's light-effect cards (imported texture asset names; case-insensitive substrings)
	const TCHAR* const LightCardTextures[] = {
		TEXT("glow_beam"),       // floodlight shine, lamp beams, window glow beams
		TEXT("lightbloom"),      // AS00_lightbloom / AS00_lightbloom2 volume-light blooms
		TEXT("sunbeams"),        // sun shafts
		TEXT("glow_halo"),       // halos
		TEXT("WindowCorona"),    // window coronas
		TEXT("CN00_Bloom"),      // carnival bloom
	};

	// scenes imported over the campus that the original never shows there (go-kart tracks)
	const TCHAR* const OverlappingScenePrefix = TEXT("br_scene:iMGRace");

	bool IsLightCardMaterial(const UMaterialInterface* M)
	{
		if (!M || M->GetBlendMode() != BLEND_Additive) { return false; }
		UTexture* Tex = nullptr;
		if (!M->GetTextureParameterValue(FHashedMaterialParameterInfo(TEXT("BaseColor")), Tex) || !Tex) { return false; }
		const FString Name = Tex->GetName();
		for (const TCHAR* Stem : LightCardTextures) { if (Name.Contains(Stem, ESearchCase::IgnoreCase)) { return true; } }
		return false;
	}
}

bool UBullyCollisionFixups::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && W->IsGameWorld() && !FParse::Param(FCommandLine::Get(), TEXT("BRNoCollisionFixups"));
}

bool UBullyCollisionFixups::IsLightCard(const UStaticMeshComponent* C)
{
	if (!C || !C->GetStaticMesh()) { return false; }
	const int32 N = C->GetNumMaterials();
	if (N <= 0) { return false; }
	for (int32 I = 0; I < N; ++I) { if (!IsLightCardMaterial(C->GetMaterial(I))) { return false; } }
	return true;
}

void UBullyCollisionFixups::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	int32 Cards = 0, Tracks = 0;
	TSet<FString> Meshes;
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		AActor* A = *It;
		bool bTrack = false;
		for (const FName& T : A->Tags) { if (T.ToString().StartsWith(OverlappingScenePrefix, ESearchCase::IgnoreCase)) { bTrack = true; break; } }
		if (bTrack)
		{
			if (A->GetActorEnableCollision() || !A->IsHidden())
			{
				A->SetActorHiddenInGame(true);
				A->SetActorEnableCollision(false);
				++Tracks;
			}
			continue;
		}
		TArray<UStaticMeshComponent*> Comps;
		A->GetComponents<UStaticMeshComponent>(Comps);
		for (UStaticMeshComponent* C : Comps)
		{
			if (C->GetCollisionEnabled() == ECollisionEnabled::NoCollision || !IsLightCard(C)) { continue; }
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCanEverAffectNavigation(false);
			++Cards;
			Meshes.Add(C->GetStaticMesh()->GetName());
		}
	}
	TArray<FString> Names = Meshes.Array();
	Names.Sort();
	UE_LOG(LogBullyCollisionFix, Display, TEXT("BR_COLLISIONFIX race-track actor(s) hidden + collision off: %d; light-card collision off on %d component(s), %d mesh(es): %s"),
		Tracks, Cards, Names.Num(), *FString::Join(Names, TEXT(" ")));
}
