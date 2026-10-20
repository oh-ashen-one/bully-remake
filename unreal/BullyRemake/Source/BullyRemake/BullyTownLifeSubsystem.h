// OWNER: slice-town
// Town life: townspeople walking loops and parked cars in the eight outdoor town districts, streamed in around the
// player. The loops and kerbside parking spots are derived from the original game's WALKABLE ped-area meshes by
// `python3 -m tools.importers townlife` (imported/townlife.json, copied to Content/Imported/TownLife/ by
// tools/ue/run_import.sh --models ... --townlife); who walks and which cars park comes from Config/TownLife.json.
//
// A district within active_radius_m of the player spawns its walkers (AAmbientWalker with an imported townsperson
// mesh) and parked rides (ABullyVehicle, rideable like the sandbox lineup); one more than despawn_margin_m beyond it
// despawns them again (except a walker that is brawling or a ride somebody is on). Off for test-harness runs
// (-BR* switches) unless -BRTownLife; -BRNoTownLife turns it off.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BullyTownLifeSubsystem.generated.h"

class AAmbientWalker;
class ABullyVehicle;

UCLASS()
class UBullyTownLifeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bActive; }

	int32 GetLiveWalkers() const;
	int32 GetLiveCars() const;

private:
	struct FLoop { FString Id; TArray<FVector> Points; };
	struct FParking { FVector Location = FVector::ZeroVector; float Yaw = 0.f; };
	struct FDistrict
	{
		FString Name;
		FVector Centre = FVector::ZeroVector;
		TArray<FLoop> Loops;
		TArray<FParking> Parking;
		bool bLive = false;
		TArray<TWeakObjectPtr<AAmbientWalker>> Walkers;
		TArray<TWeakObjectPtr<ABullyVehicle>> Cars;
	};
	struct FPed { FString Model; FString Footsteps; };

	bool LoadConfig();
	bool LoadData();
	void SpawnDistrict(FDistrict& D);
	void DespawnDistrict(FDistrict& D);

	bool bActive = false;
	double NextCheck = 0.0;
	TArray<FDistrict> Districts;
	TArray<FPed> Peds;
	TArray<FName> CarIds;
	int32 WalkersPerLoop = 2;
	float ParkedFraction = 0.6f;
	float ActiveRadius = 25000.f;     // cm
	float DespawnMargin = 8000.f;     // cm
	FVector2D WalkSpeed = FVector2D(140.f, 190.f);
	FRandomStream Rng;
};
