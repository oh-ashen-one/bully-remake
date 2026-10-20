// OWNER: slice-town
#include "BullyTownLifeSubsystem.h"

#include "AmbientWalker.h"
#include "BullyCombatAI.h"
#include "BullyVehicle.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyTown, Log, All);

namespace
{
	TSharedPtr<FJsonObject> ReadJson(const FString& Path)
	{
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root)) { return nullptr; }
		return Root;
	}

	FVector Vec(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		return A.Num() >= 3 ? FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()) : FVector::ZeroVector;
	}
}

bool UBullyTownLifeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer)) { return false; }
	const UWorld* W = Cast<UWorld>(Outer);
	return W && W->IsGameWorld() && W->GetNetMode() != NM_DedicatedServer;
}

TStatId UBullyTownLifeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBullyTownLifeSubsystem, STATGROUP_Tickables);
}

bool UBullyTownLifeSubsystem::LoadConfig()
{
	const TSharedPtr<FJsonObject> Root = ReadJson(FPaths::ProjectConfigDir() / TEXT("TownLife.json"));
	if (!Root.IsValid()) { return false; }
	bool bEnabled = true;
	Root->TryGetBoolField(TEXT("enabled"), bEnabled);
	double N = 0.0;
	if (Root->TryGetNumberField(TEXT("walkers_per_loop"), N)) { WalkersPerLoop = FMath::Clamp(int32(N), 0, 6); }
	if (Root->TryGetNumberField(TEXT("parked_fraction"), N)) { ParkedFraction = FMath::Clamp(float(N), 0.f, 1.f); }
	if (Root->TryGetNumberField(TEXT("active_radius_m"), N)) { ActiveRadius = float(N) * 100.f; }
	if (Root->TryGetNumberField(TEXT("despawn_margin_m"), N)) { DespawnMargin = float(N) * 100.f; }
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (Root->TryGetArrayField(TEXT("walk_speed_cm_s"), A) && A->Num() == 2) { WalkSpeed = FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber()); }
	if (Root->TryGetArrayField(TEXT("peds"), A))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FPed P;
			if (O.IsValid() && O->TryGetStringField(TEXT("model"), P.Model))
			{
				O->TryGetStringField(TEXT("footsteps"), P.Footsteps);
				// only peds that are actually imported on this machine
				if (FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/Imported/Characters/%s/SK_%s"), *P.Model, *P.Model))) { Peds.Add(P); }
				else { UE_LOG(LogBullyTown, Display, TEXT("BR_TOWN ped %s not imported here (tools/ue/run_import_npc.sh --peds %s), skipped"), *P.Model, *P.Model); }
			}
		}
	}
	TArray<FString> Ids;
	if (Root->TryGetStringArrayField(TEXT("parked_vehicles"), Ids)) { for (const FString& S : Ids) { if (FBullyVehicleDef::Find(FName(*S))) { CarIds.Add(FName(*S)); } } }
	return bEnabled;
}

bool UBullyTownLifeSubsystem::LoadData()
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("Imported/TownLife/townlife.json");
	const TSharedPtr<FJsonObject> Root = ReadJson(Path);
	const TArray<TSharedPtr<FJsonValue>>* Ds = nullptr;
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("districts"), Ds))
	{
		UE_LOG(LogBullyTown, Display, TEXT("BR_TOWN no %s (python3 -m tools.importers townlife + run_import.sh --townlife): town stays empty"), *Path);
		return false;
	}
	for (const TSharedPtr<FJsonValue>& DV : *Ds)
	{
		const TSharedPtr<FJsonObject> DO = DV->AsObject();
		if (!DO.IsValid()) { continue; }
		FDistrict D;
		DO->TryGetStringField(TEXT("name"), D.Name);
		FVector Sum = FVector::ZeroVector;
		int32 N = 0;
		const TArray<TSharedPtr<FJsonValue>>* Ls = nullptr;
		if (DO->TryGetArrayField(TEXT("loops"), Ls))
		{
			for (const TSharedPtr<FJsonValue>& LV : *Ls)
			{
				const TSharedPtr<FJsonObject> LO = LV->AsObject();
				const TArray<TSharedPtr<FJsonValue>>* Pts = nullptr;
				if (!LO.IsValid() || !LO->TryGetArrayField(TEXT("waypoints_cm"), Pts) || Pts->Num() < 2) { continue; }
				FLoop L;
				LO->TryGetStringField(TEXT("id"), L.Id);
				for (const TSharedPtr<FJsonValue>& P : *Pts) { L.Points.Add(Vec(P->AsArray())); Sum += L.Points.Last(); ++N; }
				D.Loops.Add(MoveTemp(L));
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Ps = nullptr;
		if (DO->TryGetArrayField(TEXT("parking"), Ps))
		{
			for (const TSharedPtr<FJsonValue>& PV : *Ps)
			{
				const TSharedPtr<FJsonObject> PO = PV->AsObject();
				const TArray<TSharedPtr<FJsonValue>>* Loc = nullptr;
				if (!PO.IsValid() || !PO->TryGetArrayField(TEXT("location_cm"), Loc)) { continue; }
				FParking P;
				P.Location = Vec(*Loc);
				double Yaw = 0.0;
				PO->TryGetNumberField(TEXT("yaw_deg"), Yaw);
				P.Yaw = float(Yaw);
				D.Parking.Add(P);
				Sum += P.Location;
				++N;
			}
		}
		if (N == 0) { continue; }
		D.Centre = Sum / N;
		Districts.Add(MoveTemp(D));
	}
	int32 L = 0, P = 0;
	for (const FDistrict& D : Districts) { L += D.Loops.Num(); P += D.Parking.Num(); }
	UE_LOG(LogBullyTown, Display, TEXT("BR_TOWN %d district(s), %d loop(s), %d parking spot(s) from %s"), Districts.Num(), L, P, *Path);
	return Districts.Num() > 0;
}

void UBullyTownLifeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	TArray<FString> Tokens, Switches;
	FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);
	bool bForced = false, bOff = false, bHarness = false;
	for (const FString& S : Switches)
	{
		if (S.Equals(TEXT("BRTownLife"), ESearchCase::IgnoreCase)) { bForced = true; }
		else if (S.Equals(TEXT("BRNoTownLife"), ESearchCase::IgnoreCase)) { bOff = true; }
		else if (S.StartsWith(TEXT("BR"), ESearchCase::IgnoreCase)) { bHarness = true; }
	}
	const bool bEnabled = LoadConfig();
	bActive = !bOff && (bForced || (bEnabled && !bHarness)) && LoadData();
	Rng.Initialize(0xB011E);
	UE_LOG(LogBullyTown, Display, TEXT("BR_TOWN %s: %d ped model(s), %d parked ride type(s)"), bActive ? TEXT("on") : TEXT("off"), Peds.Num(), CarIds.Num());
}

void UBullyTownLifeSubsystem::Tick(float Dt)
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	const double Now = W->GetTimeSeconds();
	if (Now < NextCheck) { return; }
	NextCheck = Now + 1.0;
	const APlayerController* PC = W->GetFirstPlayerController();
	const APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P) { return; }
	const FVector Me = P->GetActorLocation();
	for (FDistrict& D : Districts)
	{
		const float Dist = FVector::Dist2D(Me, D.Centre);
		if (!D.bLive && Dist < ActiveRadius) { SpawnDistrict(D); }
		else if (D.bLive && Dist > ActiveRadius + DespawnMargin) { DespawnDistrict(D); }
	}
}

void UBullyTownLifeSubsystem::SpawnDistrict(FDistrict& D)
{
	UWorld* W = GetWorld();
	D.bLive = true;
	if (Peds.Num() > 0)
	{
		for (const FLoop& L : D.Loops)
		{
			for (int32 i = 0; i < WalkersPerLoop; ++i)
			{
				const FPed& Ped = Peds[Rng.RandRange(0, Peds.Num() - 1)];
				const int32 Start = (i * L.Points.Num()) / FMath::Max(1, WalkersPerLoop);
				AAmbientWalker* Wk = W->SpawnActorDeferred<AAmbientWalker>(AAmbientWalker::StaticClass(),
					FTransform(FRotator::ZeroRotator, L.Points[Start] + FVector(0, 0, 100.f)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
				if (!Wk) { continue; }
				Wk->MeshPath = FString::Printf(TEXT("/Game/Imported/Characters/%s/SK_%s.SK_%s"), *Ped.Model, *Ped.Model, *Ped.Model);
				Wk->LoopId = D.Name + TEXT("/") + L.Id;
				Wk->StudentId = Ped.Model.ToLower();
				Wk->Waypoints = L.Points;
				Wk->StartIndex = Start;
				Wk->WalkSpeed = Rng.FRandRange(WalkSpeed.X, WalkSpeed.Y);
				Wk->IdleSecondsRange = FVector2D(0.5, 3.0);
				Wk->FinishSpawning(Wk->GetActorTransform());
				D.Walkers.Add(Wk);
			}
		}
	}
	if (CarIds.Num() > 0)
	{
		for (const FParking& Spot : D.Parking)
		{
			if (Rng.FRand() > ParkedFraction) { continue; }
			if (ABullyVehicle* V = ABullyVehicle::SpawnVehicle(W, CarIds[Rng.RandRange(0, CarIds.Num() - 1)], Spot.Location, Spot.Yaw))
			{
				D.Cars.Add(V);
			}
		}
	}
	UE_LOG(LogBullyTown, Display, TEXT("BR_TOWN %s live: %d walker(s), %d parked ride(s)"), *D.Name, D.Walkers.Num(), D.Cars.Num());
}

void UBullyTownLifeSubsystem::DespawnDistrict(FDistrict& D)
{
	int32 Kept = 0;
	for (TWeakObjectPtr<AAmbientWalker>& Wk : D.Walkers)
	{
		if (Wk.IsValid() && Cast<ABullyCombatAIController>(Wk->GetController())) { ++Kept; continue; }   // brawling with the player: leave them be
		if (Wk.IsValid()) { Wk->Destroy(); }
	}
	for (TWeakObjectPtr<ABullyVehicle>& V : D.Cars)
	{
		if (V.IsValid() && V->IsOccupied()) { ++Kept; continue; }
		if (V.IsValid()) { V->Destroy(); }
	}
	D.Walkers.Reset();
	D.Cars.Reset();
	D.bLive = false;
	UE_LOG(LogBullyTown, Display, TEXT("BR_TOWN %s asleep (%d kept)"), *D.Name, Kept);
}

int32 UBullyTownLifeSubsystem::GetLiveWalkers() const
{
	int32 N = 0;
	for (const FDistrict& D : Districts) { for (const TWeakObjectPtr<AAmbientWalker>& Wk : D.Walkers) { N += Wk.IsValid() ? 1 : 0; } }
	return N;
}

int32 UBullyTownLifeSubsystem::GetLiveCars() const
{
	int32 N = 0;
	for (const FDistrict& D : Districts) { for (const TWeakObjectPtr<ABullyVehicle>& V : D.Cars) { N += V.IsValid() ? 1 : 0; } }
	return N;
}
