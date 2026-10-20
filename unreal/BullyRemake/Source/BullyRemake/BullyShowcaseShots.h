// OWNER: slice-weapons
// Rendered evidence for the rides + arsenal (-BRShowcaseShots=<dir>, launched by tools/ue/run_showcase_shots.sh in a
// real renderer with no window). Waits for shader/asset compilation to drain, then for every ride in the sandbox
// lineup: mounts Jimmy, drives it a few metres and grabs a 3/4 view plus the chase-camera view; then for every weapon:
// a close-up of the held weapon, a wide shot at the release frame of the throw/shot, and one just after the impact on a
// passive target. Writes <dir>/NN_<name>.png and quits.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BullyShowcaseShots.generated.h"

class ABullyCharacter;
class ABullyCombatPawn;
class ABullyVehicle;
class ACameraActor;

UCLASS()
class UBullyShowcaseShots : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bActive; }

private:
	void Shot(const FString& Name);
	void Frame(const FVector& Eye, const FVector& LookAt, float Fov = 60.f);
	void UseGameCamera();

	bool bActive = false;
	FString Dir;
	int32 ResX = 1920, ResY = 1080;
	int32 Count = 0;
	int32 Step = 0;
	double StepStart = 0.0;
	double IdleSince = -1.0;
	double LastLog = -100.0;
	int32 Index = 0;
	int32 DetonationsBefore = 0;
	TArray<TWeakObjectPtr<ABullyVehicle>> Rides;
	TWeakObjectPtr<ABullyCombatPawn> Target;
	TWeakObjectPtr<ACameraActor> Cam;
};
