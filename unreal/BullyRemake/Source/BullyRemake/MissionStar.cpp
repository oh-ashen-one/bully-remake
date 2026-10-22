// OWNER: campaign-spine
#include "MissionStar.h"

#include "Components/BoxComponent.h"
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
	Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(Box);
	Beacon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beacon->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	Beacon->SetRelativeScale3D(FVector(0.8f, 0.8f, 3.0f));   // tall thin marker post
}

void AMissionStar::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Beacon->SetStaticMesh(Mesh);
	}
	// Placeholder colour: MID over the engine basic-shape material ("Color" param where present — a no-op otherwise).
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("Color"), bGold ? FLinearColor(1.0f, 0.75f, 0.1f) : FLinearColor(0.9f, 0.15f, 0.1f));
		Beacon->SetMaterial(0, MID);
	}
	UE_LOG(LogBullyStar, Display, TEXT("BR_STAR spawned '%s' (%s) at %s"), *MissionId.ToString(), bGold ? TEXT("gold") : TEXT("red"), *GetActorLocation().ToCompactString());
}

void AMissionStar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Beacon->AddLocalRotation(FRotator(0.f, 45.f * DeltaSeconds, 0.f));

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
