// OWNER: campaign-spine
#include "MissionStar.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MissionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyStar, Log, All);

AMissionStar::AMissionStar()
{
	PrimaryActorTick.bCanEverTick = true;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(150.f, 150.f, 180.f));
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Box;
	// Beacon: a tall thin beam (the "pillar of light" read from across the grounds). The box root sits at the
	// spawn point (about pawn-centre height), so the beam's centre is lifted to run from just under the ground to
	// ~12 m up. Core: the star gem, a gold diamond that spins and bobs at head height. Glow: a warm point light.
	Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(Box);
	Beacon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beacon->SetCastShadow(false);
	Beacon->SetMobility(EComponentMobility::Movable);   // static-mesh components default to Static; the star spins/bobs
	Beacon->SetRelativeLocation(FVector(0.f, 0.f, 520.f));
	Beacon->SetRelativeScale3D(FVector(0.35f, 0.35f, 12.f));   // 100 cm cylinder -> 35 cm wide, 12 m tall
	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	Core->SetupAttachment(Box);
	Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Core->SetCastShadow(false);
	Core->SetMobility(EComponentMobility::Movable);
	Core->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	Core->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.7f));
	Core->SetRelativeRotation(FRotator(45.f, 0.f, 45.f));   // cube balanced on a corner = a diamond
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Box);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
	Glow->SetIntensity(9000.f);
	Glow->SetAttenuationRadius(900.f);
	Glow->SetCastShadows(false);
	Glow->SetMobility(EComponentMobility::Movable);
}

void AMissionStar::BeginPlay()
{
	Super::BeginPlay();
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Cyl) { Beacon->SetStaticMesh(Cyl); }
	if (Cube) { Core->SetStaticMesh(Cube); }
	// Look: a tall saturated-gold pillar of light-colour plus a solid gold gem at its foot, both on the engine basic-shape
	// material ("Color" drives it; verified in an offscreen render, docs in tools/ue/run_story_shots.sh). The engine's
	// emissive/unlit debug materials were tried and do NOT render usefully here (EmissiveMeshMaterial draws nothing,
	// M_SimpleUnlitTranslucent draws solid black), so the glow is the point light + the bright flat colour.
	const FLinearColor Hue = bGold ? FLinearColor(1.0f, 0.70f, 0.04f) : FLinearColor(0.95f, 0.08f, 0.05f);
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Base)
	{
		UMaterialInstanceDynamic* BeamMid = UMaterialInstanceDynamic::Create(Base, this);
		BeamMid->SetVectorParameterValue(TEXT("Color"), Hue);
		Beacon->SetMaterial(0, BeamMid);
		UMaterialInstanceDynamic* CoreMid = UMaterialInstanceDynamic::Create(Base, this);
		CoreMid->SetVectorParameterValue(TEXT("Color"), Hue);
		Core->SetMaterial(0, CoreMid);
	}
	Glow->SetLightColor(Hue);
	SpawnedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	// A star that appears under the player's feet (continue-from-save, or a pass that ends on a start point) must not
	// instantly start its mission: the player has to leave and re-enter.
	if (const APawn* P = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		const FVector Local = Box->GetComponentTransform().InverseTransformPosition(P->GetActorLocation());
		const FVector Ext = Box->GetScaledBoxExtent();
		bPlayerInside = FMath::Abs(Local.X) <= Ext.X && FMath::Abs(Local.Y) <= Ext.Y && FMath::Abs(Local.Z) <= Ext.Z;
	}
	UE_LOG(LogBullyStar, Display, TEXT("BR_STAR spawned '%s' (%s%s) at %s"), *MissionId.ToString(), bGold ? TEXT("gold") : TEXT("red"), Base ? TEXT("") : TEXT(", NO MATERIAL"), *GetActorLocation().ToCompactString());
}

void AMissionStar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double T = GetWorld() ? GetWorld()->GetTimeSeconds() - SpawnedAt : 0.0;
	Core->SetRelativeLocation(FVector(0.f, 0.f, 70.f + 18.f * FMath::Sin(float(T) * 2.2f)));
	Core->SetRelativeRotation(FRotator(45.f, float(T) * 70.f, 45.f));
	Glow->SetIntensity(9000.f * (0.85f + 0.15f * FMath::Sin(float(T) * 3.f)));

	const APawn* P = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	bool bInside = false;
	if (P)
	{
		const FVector Local = Box->GetComponentTransform().InverseTransformPosition(P->GetActorLocation());
		const FVector Ext = Box->GetScaledBoxExtent();
		bInside = FMath::Abs(Local.X) <= Ext.X && FMath::Abs(Local.Y) <= Ext.Y && FMath::Abs(Local.Z) <= Ext.Z;
	}
	if (bInside && !bPlayerInside)
	{
		UMissionSubsystem* M = UMissionSubsystem::Get(GetWorld());
		if (M && !M->IsActive())   // Idle, or sitting in the end-card state after the previous mission
		{
			UE_LOG(LogBullyStar, Display, TEXT("BR_STAR entered '%s' -> StartMission"), *MissionId.ToString());
			if (!M->StartMission(MissionId.ToString()) && GetWorld() && GetWorld()->GetTimeSeconds() >= NextStartLogAt)
			{
				NextStartLogAt = GetWorld()->GetTimeSeconds() + 5.0;
				UE_LOG(LogBullyStar, Display, TEXT("BR_STAR start of '%s' refused (unavailable or missing Config/Missions/%s.json); star stays up"), *MissionId.ToString(), *MissionId.ToString());
			}
		}
	}
	bPlayerInside = bInside;
}
