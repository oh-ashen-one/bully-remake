// OWNER: slice-weapons
// Lightweight weapon effects without particle assets: an actor that animates a few engine spheres (or any static mesh)
// with the generated soft-edged translucent material /Game/Imported/Fx/M_BullyFx (tools/ue/run_import.sh --sfx), plus
// an optional point-light flash. Recipes: Puff (dust / powder / smoke), Burst (chunks flying out), Blast (flash + fire
// ball + smoke), Splat (flattened disc on the surface), Cloud (a lingering gas cloud), Scatter (real meshes rolling out
// and lying there, e.g. marbles). Each actor destroys itself when its last part has faded.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BullyFx.generated.h"

class UAudioComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(NotPlaceable)
class ABullyFx : public AActor
{
	GENERATED_BODY()

public:
	ABullyFx();
	virtual void Tick(float DeltaSeconds) override;

	static ABullyFx* Puff(UWorld* W, const FVector& At, const FLinearColor& Color, float Radius, float Seconds, float Opacity = 0.55f, float Rise = 40.f);
	static ABullyFx* Burst(UWorld* W, const FVector& At, const FVector& Normal, const FLinearColor& Color, int32 Count, float Speed, float Size);
	static ABullyFx* Blast(UWorld* W, const FVector& At, float Radius, const FLinearColor& FireColor);
	static ABullyFx* Splat(UWorld* W, const FVector& At, const FVector& Normal, const FLinearColor& Color, float Radius, float Seconds);
	static ABullyFx* Cloud(UWorld* W, const FVector& At, const FLinearColor& Color, float Radius, float Seconds, FName LoopSfx = NAME_None);
	static ABullyFx* Scatter(UWorld* W, const FVector& At, UStaticMesh* Mesh, int32 Count, float Radius, float Seconds);

	static int32 TotalSpawned;   // evidence counter (headless smoke)

private:
	struct FPart
	{
		TObjectPtr<UStaticMeshComponent> Comp = nullptr;
		TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;
		FVector Velocity = FVector::ZeroVector;
		float Gravity = 0.f;                   // cm/s^2 downward
		FVector Scale0 = FVector(1.f), Scale1 = FVector(1.f);
		float Opacity0 = 0.5f, Opacity1 = 0.f;
		float Delay = 0.f, Life = 1.f, Age = 0.f;
		bool bGroundStop = false;              // falls until it hits the ground, then rests (scatter / chunks)
		FLinearColor Color = FLinearColor::White;
	};

	FPart& AddSphere(const FVector& Loc, const FLinearColor& Color, float Diameter0, float Diameter1, float Life, float Opacity0, float Opacity1 = 0.f);
	FPart& AddMesh(UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, float Life);
	void Flash(const FLinearColor& Color, float Intensity, float Radius, float Seconds);
	static ABullyFx* Make(UWorld* W, const FVector& At);

	TArray<FPart> Parts;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	UPROPERTY() TObjectPtr<UAudioComponent> LoopAudio;
	float LightIntensity = 0.f, LightLife = 0.f, LightAge = 0.f;
	float AudioStopAt = -1.f, Age = 0.f;
};
