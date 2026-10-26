// OWNER: sm-visual
#include "BullySceneFixups.h"

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullySceneFix, Log, All);

bool UBullySceneFixups::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && W->IsGameWorld() && !FParse::Param(FCommandLine::Get(), TEXT("BRNoSceneFixups"));
}

TStatId UBullySceneFixups::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBullySceneFixups, STATGROUP_Tickables);
}

void UBullySceneFixups::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bBegun = true;
	if (HiddenScenes.Num() == 0) { return; }
	TMap<FString, int32> Counts;
	for (const FString& S : HiddenScenes) { Counts.Add(S.ToLower(), 0); }
	int32 Total = 0;
	for (TActorIterator<AStaticMeshActor> It(&InWorld); It; ++It)
	{
		AStaticMeshActor* A = *It;
		for (const FName& Tag : A->Tags)
		{
			const FString T = Tag.ToString();
			if (!T.StartsWith(TEXT("br_scene:"))) { continue; }
			int32* N = Counts.Find(T.Mid(9).ToLower());
			if (!N) { break; }
			A->SetActorHiddenInGame(true);
			A->SetActorEnableCollision(false);
			if (UStaticMeshComponent* C = A->GetStaticMeshComponent()) { C->SetCastShadow(false); }
			++*N;
			++Total;
			break;
		}
	}
	FString Line;
	for (const auto& P : Counts) { Line += FString::Printf(TEXT(" %s=%d"), *P.Key, P.Value); }
	UE_LOG(LogBullySceneFix, Display, TEXT("BR_SCENEFIX hid %d actor(s) in overlapping scenes:%s"), Total, *Line);
}

void UBullySceneFixups::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* W = GetWorld();
	if (!bBegun || !W || InteriorScenePrefixes.Num() == 0) { return; }
	const double Now = W->GetTimeSeconds();
	if (Now < NextUpdate) { return; }
	NextUpdate = Now + 0.15;
	UpdateCharacterChannels();
}

int32 UBullySceneFixups::ClassifyFloor(const ACharacter* C) const
{
	const UWorld* W = GetWorld();
	const UCapsuleComponent* Cap = C->GetCapsuleComponent();
	const FVector From = C->GetActorLocation();
	const FVector To = From - FVector(0, 0, (Cap ? Cap->GetScaledCapsuleHalfHeight() : 90.0) + 400.0);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BullySceneFixFloor), true, C);
	FHitResult Hit;
	if (!W->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Q)) { return -1; }
	const AActor* Floor = Hit.GetActor();
	if (!Floor) { return -1; }
	for (const FName& Tag : Floor->Tags)
	{
		const FString T = Tag.ToString();
		if (!T.StartsWith(TEXT("br_scene:"))) { continue; }
		const FString Scene = T.Mid(9);
		for (const FString& P : InteriorScenePrefixes) { if (Scene.StartsWith(P, ESearchCase::IgnoreCase)) { return 1; } }
		return 0;
	}
	return -1;
}

void UBullySceneFixups::UpdateCharacterChannels()
{
	UWorld* W = GetWorld();
	for (auto It = Indoors.CreateIterator(); It; ++It) { if (!It.Key().IsValid()) { It.RemoveCurrent(); } }
	for (TActorIterator<ACharacter> It(W); It; ++It)
	{
		ACharacter* C = *It;
		const int32 Cls = ClassifyFloor(C);
		if (Cls < 0) { continue; }   // airborne / untagged floor: keep whatever he had
		const bool bIn = Cls == 1;
		const bool* Was = Indoors.Find(C);
		if (Was && *Was == bIn) { continue; }
		if (!Was && !bIn) { Indoors.Add(C, false); continue; }   // default channels already = outdoors
		TArray<UPrimitiveComponent*> Prims;
		C->GetComponents<UPrimitiveComponent>(Prims, true);
		for (UPrimitiveComponent* P : Prims) { P->SetLightingChannels(!bIn, bIn, false); }
		// attached actors (held weapons/props) follow their owner
		TArray<AActor*> Attached;
		C->GetAttachedActors(Attached, true, true);
		for (AActor* A : Attached)
		{
			TArray<UPrimitiveComponent*> P2;
			A->GetComponents<UPrimitiveComponent>(P2);
			for (UPrimitiveComponent* P : P2) { P->SetLightingChannels(!bIn, bIn, false); }
		}
		Indoors.Add(C, bIn);
		UE_LOG(LogBullySceneFix, Display, TEXT("BR_SCENEFIX %s -> %s lighting"), *C->GetName(), bIn ? TEXT("interior") : TEXT("exterior"));
	}
}

bool UBullySceneFixups::IsIndoors(const ACharacter* C) const
{
	const bool* B = Indoors.Find(const_cast<ACharacter*>(C));
	return B && *B;
}
