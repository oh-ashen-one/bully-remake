// OWNER: slice-vehicles
#include "BullyVehicle.h"

#include "BullyAnimInstance.h"
#include "BullyCharacter.h"
#include "BullyCombatComponent.h"
#include "BullySfx.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyVehicle, Log, All);

int32 ABullyVehicle::TotalMounts = 0;
int32 ABullyVehicle::TotalDismounts = 0;
int32 ABullyVehicle::TotalRunOvers = 0;
int32 ABullyVehicle::TotalRescues = 0;

namespace
{
	double GLastRideToggle = -100.0;   // one E press must not both dismount and re-mount
	constexpr float StepUp = 25.f;     // curb height the ride climbs without a ramp

	bool ReadVec(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, FVector& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O->TryGetArrayField(Key, A) || A->Num() != 3) { return false; }
		Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		return true;
	}

	float Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, float Default)
	{
		double N = 0.0;
		return O->TryGetNumberField(Key, N) ? float(N) : Default;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// config

const TArray<FBullyVehicleDef>& FBullyVehicleDef::All()
{
	static TArray<FBullyVehicleDef> Cache;
	static bool bLoaded = false;
	if (bLoaded) { return Cache; }
	bLoaded = true;
	FString Text;
	const FString Path = FPaths::ProjectConfigDir() / TEXT("Vehicles.json");
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root)
		|| !Root.IsValid() || !Root->TryGetArrayField(TEXT("vehicles"), Arr))
	{
		UE_LOG(LogBullyVehicle, Warning, TEXT("BR_VEHICLE cannot read %s"), *Path);
		return Cache;
	}
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
		FString S;
		if (!O.IsValid() || !O->TryGetStringField(TEXT("id"), S)) { continue; }
		FBullyVehicleDef D;
		D.Id = FName(*S);
		D.Name = O->TryGetStringField(TEXT("name"), S) ? S : D.Id.ToString();
		ReadVec(O, TEXT("half_extent"), D.HalfExtent);
		D.RideHeight = Num(O, TEXT("ride_height"), D.RideHeight);
		D.MaxSpeed = Num(O, TEXT("max_speed"), D.MaxSpeed);
		D.Accel = Num(O, TEXT("accel"), D.Accel);
		D.Brake = Num(O, TEXT("brake"), D.Brake);
		D.MaxReverse = Num(O, TEXT("max_reverse"), D.MaxReverse);
		D.Coast = Num(O, TEXT("coast"), D.Coast);
		D.TurnRate = Num(O, TEXT("turn_rate"), D.TurnRate);
		D.TurnFullSpeed = FMath::Max(1.f, Num(O, TEXT("turn_full_speed"), D.TurnFullSpeed));
		ReadVec(O, TEXT("seat_offset"), D.SeatOffset);
		O->TryGetBoolField(TEXT("rider_stands"), D.bRiderStands);
		D.CameraDistance = Num(O, TEXT("camera_distance"), D.CameraDistance);
		D.CameraHeight = Num(O, TEXT("camera_height"), D.CameraHeight);
		FVector C;
		if (ReadVec(O, TEXT("color"), C)) { D.Color = FLinearColor(C.X, C.Y, C.Z); }
		D.Wheels = FMath::Clamp(int32(Num(O, TEXT("wheels"), D.Wheels)), 0, 4);
		D.WheelRadius = Num(O, TEXT("wheel_radius"), D.WheelRadius);
		O->TryGetBoolField(TEXT("cabin"), D.bCabin);
		O->TryGetStringField(TEXT("mesh"), D.MeshPath);
		ReadVec(O, TEXT("mesh_scale"), D.MeshScale);
		ReadVec(O, TEXT("mesh_offset_cm"), D.MeshOffset);
		D.MeshYaw = Num(O, TEXT("mesh_yaw"), 0.f);
		O->TryGetStringField(TEXT("model"), D.Model);
		const TSharedPtr<FJsonObject>* RC = nullptr;
		if (O->TryGetObjectField(TEXT("rider"), RC))
		{
			for (const auto& KV : (*RC)->Values)
			{
				FString ClipName;
				if (KV.Value.IsValid() && KV.Value->TryGetString(ClipName)) { D.RiderClips.Add(FName(FString(KV.Key)), FName(*ClipName)); }
			}
		}
		D.RiderClipSpeed = FMath::Max(1.f, Num(O, TEXT("rider_clip_speed"), D.RiderClipSpeed));
		if (O->TryGetStringField(TEXT("sfx_loop"), S)) { D.SfxLoop = FName(*S); }
		if (O->TryGetStringField(TEXT("sfx_brake"), S)) { D.SfxBrake = FName(*S); }
		if (O->TryGetStringField(TEXT("sfx_horn"), S)) { D.SfxHorn = FName(*S); }
		O->TryGetBoolField(TEXT("sfx_loop_idle"), D.bSfxLoopIdle);
		Cache.Add(D);
	}
	UE_LOG(LogBullyVehicle, Display, TEXT("BR_VEHICLE %d ride type(s) loaded from %s"), Cache.Num(), *Path);
	return Cache;
}

const FBullyVehicleDef* FBullyVehicleDef::Find(FName Id)
{
	for (const FBullyVehicleDef& D : All()) { if (D.Id == Id) { return &D; } }
	return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// actor

ABullyVehicle::ABullyVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetCollisionProfileName(TEXT("Vehicle"));
	Box->SetCanEverAffectNavigation(false);
	RootComponent = Box;

	auto Part = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* M = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		M->SetupAttachment(Box);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCanEverAffectNavigation(false);
		return M;
	};
	Body = Part(TEXT("Body"));
	Cabin = Part(TEXT("Cabin"));
	WheelMeshes.Add(Part(TEXT("Wheel0")));
	WheelMeshes.Add(Part(TEXT("Wheel1")));
	WheelMeshes.Add(Part(TEXT("Wheel2")));
	WheelMeshes.Add(Part(TEXT("Wheel3")));

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(Box);
	Boom->bUsePawnControlRotation = false;
	Boom->bInheritPitch = false;
	Boom->bInheritRoll = false;
	Boom->bInheritYaw = true;
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 8.f;
	Boom->bEnableCameraRotationLag = true;
	Boom->CameraRotationLagSpeed = 6.f;
	Boom->bDoCollisionTest = true;

	Cam = CreateDefaultSubobject<UCameraComponent>(TEXT("Cam"));
	Cam->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Cam->bUsePawnControlRotation = false;
}

ABullyVehicle* ABullyVehicle::SpawnVehicle(UWorld* World, FName Id, const FVector& Location, float Yaw)
{
	const FBullyVehicleDef* D = FBullyVehicleDef::Find(Id);
	if (!World || !D) { UE_LOG(LogBullyVehicle, Warning, TEXT("BR_VEHICLE unknown ride '%s'"), *Id.ToString()); return nullptr; }
	FVector At = Location;
	FHitResult Hit;
	FCollisionObjectQueryParams Obj(ECC_WorldStatic);
	if (World->LineTraceSingleByObjectType(Hit, Location + FVector(0, 0, 500), Location - FVector(0, 0, 3000), Obj))
	{
		At.Z = Hit.ImpactPoint.Z + D->RideHeight;
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ABullyVehicle* V = World->SpawnActor<ABullyVehicle>(ABullyVehicle::StaticClass(), At, FRotator(0.f, Yaw, 0.f), SP);
	if (V)
	{
		V->Configure(*D);
		UE_LOG(LogBullyVehicle, Display, TEXT("BR_VEHICLE spawned %s at %s yaw %.0f"), *D->Name, *V->GetActorLocation().ToCompactString(), Yaw);
	}
	return V;
}

void ABullyVehicle::Configure(const FBullyVehicleDef& D)
{
	Def = D;
	Box->SetBoxExtent(D.HalfExtent);
	Boom->TargetArmLength = D.CameraDistance;
	Boom->SocketOffset = FVector(0.f, 0.f, D.CameraHeight);
	Boom->SetRelativeRotation(FRotator(-10.f, 0.f, 0.f));

	// The imported game model (rig) replaces the primitive look entirely; else a plain local mesh.
	if (!D.Model.IsEmpty() && ConfigureFromRig(D)) { return; }
	if (!D.MeshPath.IsEmpty())
	{
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *D.MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Body->SetStaticMesh(M);
			Body->SetRelativeScale3D(D.MeshScale);
			Body->SetRelativeLocationAndRotation(D.MeshOffset, FRotator(0.f, D.MeshYaw, 0.f));
			Cabin->SetVisibility(false);
			for (UStaticMeshComponent* W : WheelMeshes) { W->SetVisibility(false); }
			return;
		}
		UE_LOG(LogBullyVehicle, Warning, TEXT("BR_VEHICLE %s: mesh %s not found, using the primitive look"), *D.Name, *D.MeshPath);
	}

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	auto Tint = [this, Base](UStaticMeshComponent* C, const FLinearColor& Col)
	{
		if (!Base) { return; }
		UMaterialInstanceDynamic* MI = UMaterialInstanceDynamic::Create(Base, this);
		MI->SetVectorParameterValue(TEXT("Color"), Col);
		C->SetMaterial(0, MI);
	};

	const FVector E = D.HalfExtent;
	const float R = D.WheelRadius;
	const float Ground = -D.RideHeight;          // ground plane in actor space
	// body: a slab riding on the axles
	const float BodyHalfZ = FMath::Max(3.f, E.Z * (D.bRiderStands ? 0.25f : 0.45f));
	Body->SetStaticMesh(Cube);
	Body->SetRelativeScale3D(FVector(E.X * 2.f / 100.f, E.Y * (D.Wheels == 2 ? 1.f : 2.f) / 100.f, BodyHalfZ * 2.f / 100.f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, Ground + R * (D.Wheels == 2 ? 1.6f : 1.f) + BodyHalfZ));
	Tint(Body, D.Color);

	Cabin->SetVisibility(D.bCabin);
	if (D.bCabin)
	{
		Cabin->SetStaticMesh(Cube);
		Cabin->SetRelativeScale3D(FVector(E.X * 1.1f / 100.f, E.Y * 1.8f / 100.f, E.Z * 1.0f / 100.f));
		Cabin->SetRelativeLocation(FVector(-E.X * 0.12f, 0.f, Ground + R + BodyHalfZ * 2.f + E.Z * 0.5f));
		Tint(Cabin, D.Color * 0.6f + FLinearColor(0.25f, 0.28f, 0.32f) * 0.4f);
	}

	const float Width = D.Wheels == 2 ? 8.f : FMath::Clamp(R * 0.6f, 4.f, 26.f);
	for (int32 i = 0; i < WheelMeshes.Num(); ++i)
	{
		UStaticMeshComponent* W = WheelMeshes[i];
		const bool bUsed = i < D.Wheels;
		W->SetVisibility(bUsed);
		if (!bUsed) { continue; }
		W->SetStaticMesh(Cyl);
		W->SetRelativeScale3D(FVector(R * 2.f / 100.f, R * 2.f / 100.f, Width / 100.f));
		const float X = (i % 2 == 0 ? 1.f : -1.f) * E.X * (D.Wheels == 2 ? 0.78f : 0.7f);
		const float Y = D.Wheels == 2 ? 0.f : (i < 2 ? 1.f : -1.f) * (E.Y * (D.bRiderStands ? 0.7f : 1.f) + Width * 0.5f);
		W->SetRelativeLocationAndRotation(FVector(X, Y, Ground + R), FRotator(0.f, 0.f, 90.f));   // cylinder axis -> side
		Tint(W, FLinearColor(0.04f, 0.04f, 0.05f));
	}
}

bool ABullyVehicle::ConfigureFromRig(const FBullyVehicleDef& D)
{
	// Content/Imported/VehicleRigs/<model>.json (git-ignored, from the importer): mesh paths, wheel hubs, ground offset,
	// bounds, all in the model's own UE space (cm; the model faces -Y because the game's forward is +y).
	const FString Path = FPaths::ProjectContentDir() / TEXT("Imported/VehicleRigs") / (D.Model + TEXT(".json"));
	FString Text;
	TSharedPtr<FJsonObject> Rig;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Rig) || !Rig.IsValid())
	{
		UE_LOG(LogBullyVehicle, Warning, TEXT("BR_VEHICLE %s: no rig %s (run the vehicle import), using the fallback look"), *D.Name, *Path);
		return false;
	}
	FString MeshPath, WheelPath;
	Rig->TryGetStringField(TEXT("mesh"), MeshPath);
	UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!M)
	{
		UE_LOG(LogBullyVehicle, Warning, TEXT("BR_VEHICLE %s: rig mesh %s not imported, using the fallback look"), *D.Name, *MeshPath);
		return false;
	}
	auto Vec = [](const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		return A.Num() == 3 ? FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()) : FVector::ZeroVector;
	};
	const FVector Scale = D.MeshScale;
	RigBaseYaw = 90.f + D.MeshYaw;   // model -Y -> actor +X
	const FRotator R(0.f, RigBaseYaw, 0.f);
	FVector Lo = FVector::ZeroVector, Hi = FVector::ZeroVector;
	const TArray<TSharedPtr<FJsonValue>>* B = nullptr;
	if (Rig->TryGetArrayField(TEXT("body_bounds_cm"), B) && B->Num() == 2) { Lo = Vec((*B)[0]); Hi = Vec((*B)[1]); }
	double GroundZ = Lo.Z;
	Rig->TryGetNumberField(TEXT("ground_z_cm"), GroundZ);
	// centre the body's footprint on the actor, its wheels on the ground plane (actor z = -RideHeight)
	const FVector Centre = FVector((Lo.X + Hi.X) * 0.5f, (Lo.Y + Hi.Y) * 0.5f, 0.f) * Scale;
	const FVector Offset = -R.RotateVector(Centre) + FVector(0.f, 0.f, -D.RideHeight - float(GroundZ) * Scale.Z) + D.MeshOffset;

	Body->SetStaticMesh(M);
	Body->SetMaterial(0, nullptr);
	Body->SetRelativeScale3D(Scale);
	Body->SetRelativeLocationAndRotation(Offset, R);
	Body->SetVisibility(true);
	Cabin->SetVisibility(false);

	RigWheels.Reset();
	UStaticMesh* WM = Rig->TryGetStringField(TEXT("wheel_mesh"), WheelPath) ? LoadObject<UStaticMesh>(nullptr, *WheelPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Wheels = nullptr;
	double WScale = 1.0, WRadius = 30.0;
	Rig->TryGetNumberField(TEXT("wheel_scale"), WScale);
	Rig->TryGetNumberField(TEXT("wheel_radius_cm"), WRadius);
	RigWheelRadius = FMath::Max(1.f, float(WRadius) * Scale.X);
	if (WM && Rig->TryGetArrayField(TEXT("wheels"), Wheels))
	{
		for (const TSharedPtr<FJsonValue>& WV : *Wheels)
		{
			const TSharedPtr<FJsonObject> WO = WV->AsObject();
			const TArray<TSharedPtr<FJsonValue>>* Hub = nullptr;
			if (!WO.IsValid() || !WO->TryGetArrayField(TEXT("hub_cm"), Hub) || Hub->Num() != 3) { continue; }
			FWheelRig W;
			W.Hub = R.RotateVector(FVector((*Hub)[0]->AsNumber(), (*Hub)[1]->AsNumber(), (*Hub)[2]->AsNumber()) * Scale) + Offset;
			WO->TryGetBoolField(TEXT("front"), W.bFront);
			WO->TryGetBoolField(TEXT("left"), W.bLeft);
			RigWheels.Add(W);
		}
	}
	for (int32 i = 0; i < WheelMeshes.Num(); ++i)
	{
		UStaticMeshComponent* WC = WheelMeshes[i];
		const bool bUsed = RigWheels.IsValidIndex(i);
		WC->SetVisibility(bUsed);
		if (!bUsed) { continue; }
		WC->SetStaticMesh(WM);
		WC->SetMaterial(0, nullptr);
		WC->SetRelativeScale3D(FVector(float(WScale)) * Scale);
		WC->SetRelativeLocation(RigWheels[i].Hub);
	}
	bRigLook = true;
	UE_LOG(LogBullyVehicle, Display, TEXT("BR_VEHICLE %s: game model %s, %d wheel(s) (%s), ground %.1f cm"),
		*D.Name, *MeshPath, RigWheels.Num(), WM ? *WheelPath : TEXT("in the body"), GroundZ);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
// get on / off

void ABullyVehicle::TryMountNearby()
{
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	ABullyCharacter* C = PC ? Cast<ABullyCharacter>(PC->GetPawn()) : nullptr;
	if (!C) { return; }
	const float Reach = Def.HalfExtent.X + 180.f;
	const FVector Me = GetActorLocation();
	if (FVector::DistSquared2D(C->GetActorLocation(), Me) > FMath::Square(Reach) || FMath::Abs(C->GetActorLocation().Z - Me.Z) > 250.f) { return; }
	// only the nearest empty ride answers
	for (TActorIterator<ABullyVehicle> It(W); It; ++It)
	{
		if (*It != this && !It->IsOccupied()
			&& FVector::DistSquared2D(C->GetActorLocation(), It->GetActorLocation()) < FVector::DistSquared2D(C->GetActorLocation(), Me)) { return; }
	}
	if (GEngine) { GEngine->AddOnScreenDebugMessage(uint64(GetUniqueID()) + 7000, 0.12f, FColor(250, 200, 60), FString::Printf(TEXT("E  ride the %s"), *Def.Name)); }
	if ((PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top))
		&& W->GetRealTimeSeconds() - GLastRideToggle > 0.4)
	{
		Mount(C);
	}
}

bool ABullyVehicle::Mount(ABullyCharacter* NewRider)
{
	if (IsOccupied() || !NewRider) { return false; }
	APlayerController* PC = Cast<APlayerController>(NewRider->GetController());
	if (!PC) { return false; }
	GLastRideToggle = GetWorld()->GetRealTimeSeconds();

	if (UCharacterMovementComponent* CM = NewRider->GetCharacterMovement())
	{
		CM->StopMovementImmediately();
		CM->DisableMovement();
	}
	NewRider->SetActorEnableCollision(false);
	NewRider->AttachToComponent(Box, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	NewRider->SetActorRelativeLocation(Def.SeatOffset);
	NewRider->SetActorRelativeRotation(FRotator::ZeroRotator);

	Rider = NewRider;
	DriverPC = PC;
	UpdateRiderPose(0.f);
	Throttle = Steer = 0.f;
	bHandbrake = false;
	LookOffset = FRotator::ZeroRotator;
	PC->Possess(this);
	++TotalMounts;
	UE_LOG(LogBullyVehicle, Display, TEXT("BR_VEHICLE mount %s"), *Def.Name);
	return true;
}

bool ABullyVehicle::Dismount()
{
	ABullyCharacter* C = Rider.Get();
	if (!C) { return false; }
	UWorld* W = GetWorld();
	GLastRideToggle = W->GetRealTimeSeconds();
	APlayerController* PC = DriverPC.IsValid() ? DriverPC.Get() : Cast<APlayerController>(GetController());

	C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (UBullyAnimInstance* AI = C->GetMesh() ? Cast<UBullyAnimInstance>(C->GetMesh()->GetAnimInstance()) : nullptr) { AI->SetRideClip(NAME_None); }
	// step off to the left, else right / behind / ahead, wherever a standing capsule fits
	const float HalfH = C->GetCapsuleComponent() ? C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	const float Rad = C->GetCapsuleComponent() ? C->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.f;
	const FVector Base = GetActorLocation();
	const FVector Side = GetActorRightVector(), Fwd = GetActorForwardVector();
	const FVector Tries[4] = { -Side * (Def.HalfExtent.Y + Rad + 30.f), Side * (Def.HalfExtent.Y + Rad + 30.f),
		-Fwd * (Def.HalfExtent.X + Rad + 30.f), Fwd * (Def.HalfExtent.X + Rad + 30.f) };
	FVector Land = Base + Tries[0];
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BullyDismount), false, this);
	Q.AddIgnoredActor(C);
	for (const FVector& T : Tries)
	{
		FVector P = Base + T;
		FHitResult Hit;
		if (W->LineTraceSingleByObjectType(Hit, P + FVector(0, 0, 200), P - FVector(0, 0, 600), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
		{
			P.Z = Hit.ImpactPoint.Z + HalfH + 3.f;
		}
		if (!W->OverlapBlockingTestByChannel(P, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Rad, HalfH), Q)) { Land = P; break; }
	}
	C->SetActorLocationAndRotation(Land, FRotator(0.f, GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	C->SetActorEnableCollision(true);
	if (UCharacterMovementComponent* CM = C->GetCharacterMovement()) { CM->SetMovementMode(MOVE_Walking); }

	Rider.Reset();
	Throttle = Steer = 0.f;
	bHandbrake = false;
	if (PC) { PC->Possess(C); }
	++TotalDismounts;
	UE_LOG(LogBullyVehicle, Display, TEXT("BR_VEHICLE dismount %s at %.0f km/h"), *Def.Name, FMath::Abs(Speed) * 0.036f);
	return true;
}

void ABullyVehicle::SetScriptedInput(bool bOn, float InThrottle, float InSteer, bool bInHandbrake)
{
	bScripted = bOn;
	ScriptThrottle = FMath::Clamp(InThrottle, -1.f, 1.f);
	ScriptSteer = FMath::Clamp(InSteer, -1.f, 1.f);
	bScriptHandbrake = bInHandbrake;
}

// ---------------------------------------------------------------------------------------------------------------
// input

void ABullyVehicle::BuildInput()
{
	if (Imc) { return; }
	Imc = NewObject<UInputMappingContext>(this, TEXT("IMC_BullyVehicle"));
	auto Make = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(this, Name);
		A->ValueType = Type;
		return A;
	};
	ThrottleAction = Make(TEXT("IA_VehThrottle"), EInputActionValueType::Axis1D);
	SteerAction = Make(TEXT("IA_VehSteer"), EInputActionValueType::Axis1D);
	HandbrakeAction = Make(TEXT("IA_VehHandbrake"), EInputActionValueType::Boolean);
	LookAction = Make(TEXT("IA_VehLook"), EInputActionValueType::Axis2D);
	ExitAction = Make(TEXT("IA_VehExit"), EInputActionValueType::Boolean);
	HornAction = Make(TEXT("IA_VehHorn"), EInputActionValueType::Boolean);

	auto Negate = [this]() { UInputModifierNegate* M = NewObject<UInputModifierNegate>(this); return M; };
	auto Dead = [this]() { UInputModifierDeadZone* M = NewObject<UInputModifierDeadZone>(this); M->LowerThreshold = 0.15f; return M; };

	Imc->MapKey(ThrottleAction, EKeys::W);
	Imc->MapKey(ThrottleAction, EKeys::S).Modifiers.Add(Negate());
	Imc->MapKey(ThrottleAction, EKeys::Gamepad_RightTriggerAxis);
	Imc->MapKey(ThrottleAction, EKeys::Gamepad_LeftTriggerAxis).Modifiers.Add(Negate());
	Imc->MapKey(SteerAction, EKeys::D);
	Imc->MapKey(SteerAction, EKeys::A).Modifiers.Add(Negate());
	Imc->MapKey(SteerAction, EKeys::Gamepad_LeftX).Modifiers.Add(Dead());
	Imc->MapKey(HandbrakeAction, EKeys::SpaceBar);
	Imc->MapKey(HandbrakeAction, EKeys::Gamepad_FaceButton_Left);
	Imc->MapKey(LookAction, EKeys::Mouse2D);
	Imc->MapKey(LookAction, EKeys::Gamepad_Right2D).Modifiers.Add(Dead());
	Imc->MapKey(ExitAction, EKeys::E);
	Imc->MapKey(ExitAction, EKeys::Gamepad_FaceButton_Top);
	Imc->MapKey(HornAction, EKeys::H);
	Imc->MapKey(HornAction, EKeys::Gamepad_LeftThumbstick);
}

void ABullyVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ABullyVehicle::OnThrottle);
		EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ABullyVehicle::OnThrottleEnd);
		EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &ABullyVehicle::OnSteer);
		EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &ABullyVehicle::OnSteerEnd);
		EIC->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &ABullyVehicle::OnHandbrake);
		EIC->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ABullyVehicle::OnHandbrakeEnd);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABullyVehicle::OnLook);
		EIC->BindAction(ExitAction, ETriggerEvent::Started, this, &ABullyVehicle::OnExit);
		EIC->BindAction(HornAction, ETriggerEvent::Started, this, &ABullyVehicle::OnHorn);
		EIC->BindAction(HornAction, ETriggerEvent::Completed, this, &ABullyVehicle::OnHornEnd);
	}
}

void ABullyVehicle::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	DriverPC = Cast<APlayerController>(NewController);
}

void ABullyVehicle::PawnClientRestart()
{
	Super::PawnClientRestart();
	BuildInput();
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (UEnhancedInputLocalPlayerSubsystem* Sub = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr)
	{
		Sub->AddMappingContext(Imc, 1);   // above the on-foot controls: W/S/A/D, mouse and E drive the ride
	}
}

void ABullyVehicle::UnPossessed()
{
	if (APlayerController* PC = DriverPC.Get())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->RemoveMappingContext(Imc);
		}
	}
	Super::UnPossessed();
}

void ABullyVehicle::OnLook(const FInputActionValue& V)
{
	const FVector2D D = V.Get<FVector2D>();
	LookOffset.Yaw = FRotator::NormalizeAxis(LookOffset.Yaw + D.X * 0.2f);
	LookOffset.Pitch = FMath::Clamp(LookOffset.Pitch + D.Y * 0.15f, -35.f, 25.f);
	LastLookTime = GetWorld()->GetRealTimeSeconds();
}

void ABullyVehicle::OnExit(const FInputActionValue& V)
{
	if (GetWorld()->GetRealTimeSeconds() - GLastRideToggle > 0.4) { Dismount(); }
}

void ABullyVehicle::OnHorn(const FInputActionValue& V)
{
	if (Def.SfxHorn.IsNone()) { return; }
	if (!HornAudio) { HornAudio = FBullySfx::Loop(this, Box, Def.SfxHorn); }
	else { HornAudio->Play(); }
}

void ABullyVehicle::OnHornEnd(const FInputActionValue& V)
{
	if (HornAudio) { HornAudio->Stop(); }
}

void ABullyVehicle::UpdateSound(float Dt)
{
	// ride loop: engines idle while someone is on; pedals / wheels only sound when rolling. Pitch climbs with speed.
	if (Def.SfxLoop.IsNone()) { return; }
	const float Frac = FMath::Clamp(FMath::Abs(Speed) / FMath::Max(1.f, Def.MaxSpeed), 0.f, 1.f);
	const float Want = bGrounded || Def.bSfxLoopIdle
		? (Def.bSfxLoopIdle ? (IsOccupied() ? 0.45f + 0.55f * Frac : 0.f) : FMath::Clamp(Frac * 2.5f, 0.f, 1.f))
		: 0.f;
	LoopLevel = FMath::FInterpTo(LoopLevel, Want, Dt, 6.f);
	if (LoopLevel < 0.02f)
	{
		if (LoopAudio && LoopAudio->IsPlaying()) { LoopAudio->Stop(); }
		return;
	}
	if (!LoopAudio) { LoopAudio = FBullySfx::Loop(this, Box, Def.SfxLoop); if (!LoopAudio) { Def.SfxLoop = NAME_None; return; } }
	if (!LoopAudio->IsPlaying()) { LoopAudio->Play(); }
	LoopAudio->SetVolumeMultiplier(LoopLevel);
	LoopAudio->SetPitchMultiplier(0.8f + 0.7f * Frac);
	// a skid when braking hard / handbraking at speed
	const bool HB = bScripted ? bScriptHandbrake : bHandbrake;
	const float T = bScripted ? ScriptThrottle : Throttle;
	if (!Def.SfxBrake.IsNone() && bGrounded && FMath::Abs(Speed) > 600.f && (HB || (T < -0.3f && Speed > 0.f)) && GetWorld()->GetTimeSeconds() >= NextSkidAt)
	{
		FBullySfx::Play(this, Def.SfxBrake, GetActorLocation());
		NextSkidAt = GetWorld()->GetTimeSeconds() + 1.2;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// driving

void ABullyVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);
	if (!IsOccupied() && !bScripted) { TryMountNearby(); }
	Drive(Dt);
	if (IsOccupied())
	{
		UpdateCamera(Dt);
		UpdateRiderPose(Dt);
		ShowStatus();
	}
	UpdateWheels(Dt);
	UpdateSound(Dt);
}

void ABullyVehicle::Drive(float Dt)
{
	const float T = bScripted ? ScriptThrottle : (IsOccupied() ? Throttle : 0.f);
	const float S = bScripted ? ScriptSteer : (IsOccupied() ? Steer : 0.f);
	const bool HB = bScripted ? bScriptHandbrake : (IsOccupied() && bHandbrake);
	const float Sign = Speed >= 0.f ? 1.f : -1.f;

	if (bGrounded)
	{
		if (HB)
		{
			Speed -= Sign * FMath::Min(FMath::Abs(Speed), Def.Brake * 1.2f * Dt);
		}
		else if (T > 0.05f)
		{
			Speed += (Speed < 0.f ? Def.Brake : Def.Accel * (1.f - 0.35f * FMath::Clamp(Speed / Def.MaxSpeed, 0.f, 1.f))) * T * Dt;
		}
		else if (T < -0.05f)
		{
			Speed -= (Speed > 0.f ? Def.Brake : Def.Accel * 0.6f) * -T * Dt;
		}
		else
		{
			Speed -= Sign * FMath::Min(FMath::Abs(Speed), Def.Coast * Dt);
		}
		Speed = FMath::Clamp(Speed, -Def.MaxReverse, Def.MaxSpeed);
	}

	FRotator Rot = GetActorRotation();
	if (bGrounded)
	{
		const float Factor = FMath::Clamp(FMath::Abs(Speed) / Def.TurnFullSpeed, 0.f, 1.f);
		Rot.Yaw += S * Def.TurnRate * (HB ? 1.4f : 1.f) * Factor * (Speed >= 0.f ? 1.f : -1.f) * Dt;
	}
	SetActorRotation(Rot);

	if (FMath::Abs(Speed) > 1.f)
	{
		UWorld* W = GetWorld();
		const FVector Fwd = FRotator(0.f, Rot.Yaw, 0.f).Vector();
		const FVector Delta = Fwd * Speed * Dt;
		const bool bStepped = bGrounded;
		if (bStepped) { AddActorWorldOffset(FVector(0.f, 0.f, StepUp)); }   // lift so the box can roll over kerbs
		FHitResult Hit;
		AddActorWorldOffset(Delta, true, &Hit);
		if (Hit.bBlockingHit)
		{
			AActor* Other = Hit.GetActor();
			UBullyCombatComponent* Victim = Other ? Other->FindComponentByClass<UBullyCombatComponent>() : nullptr;
			const double Now = W->GetTimeSeconds();
			if (Victim && Other != Rider.Get() && FMath::Abs(Speed) > 450.f)
			{
				// knock a pedestrian over (once a second per victim) and keep rolling
				const double* Last = RecentRunOvers.Find(Other);
				if (!Last || Now - *Last > 1.0)
				{
					RecentRunOvers.Add(Other, Now);
					Victim->ApplyWeaponHit(EBullyHitReaction::Knockdown, FMath::Clamp(FMath::Abs(Speed) / 120.f, 4.f, 25.f));
					if (ACharacter* VC = Cast<ACharacter>(Other))
					{
						VC->LaunchCharacter((VC->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * 500.f + FVector(0.f, 0.f, 250.f), true, true);
					}
					++TotalRunOvers;
					UE_LOG(LogBullyVehicle, Display, TEXT("BR_VEHICLE %s ran into %s at %.0f km/h"), *Def.Name, *Other->GetName(), FMath::Abs(Speed) * 0.036f);
				}
				Speed *= 0.75f;
			}
			else
			{
				// wall: lose speed by how head-on it was, bounce off a hard hit, slide along the rest
				const FVector N = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();
				const float HeadOn = FMath::Abs(FVector::DotProduct(Fwd, N));
				if (HeadOn > 0.85f && FMath::Abs(Speed) > 600.f) { Speed = -Speed * 0.2f; }
				else { Speed *= (1.f - 0.8f * HeadOn); }
			}
			FVector Slide = FVector::VectorPlaneProject(Delta * (1.f - Hit.Time), Hit.ImpactNormal);
			Slide.Z = 0.f;
			if (!Slide.IsNearlyZero()) { AddActorWorldOffset(Slide, true); }
		}
		if (bStepped) { AddActorWorldOffset(FVector(0.f, 0.f, -StepUp), true); }   // and back down (stops on a kerb top)
	}
	SettleOnGround(Dt, false);
}

void ABullyVehicle::SettleOnGround(float Dt, bool bSnap)
{
	UWorld* W = GetWorld();
	FVector Loc = GetActorLocation();
	FRotator Rot = GetActorRotation();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BullyVehicleGround), true, this);
	if (Rider.IsValid()) { Q.AddIgnoredActor(Rider.Get()); }

	// Ground under the whole footprint, not just the centre: a seam between road pieces or a kerb face under the middle
	// must not read as "airborne". Probes start above the box (never inside the ground) and the highest walkable hit wins.
	const FVector Fwd = FRotator(0.f, Rot.Yaw, 0.f).Vector();
	const FVector Side = FRotationMatrix(FRotator(0.f, Rot.Yaw, 0.f)).GetUnitAxis(EAxis::Y);
	const FVector2D E(Def.HalfExtent.X * 0.8f, Def.HalfExtent.Y * 0.8f);
	const FVector Probes[5] = { FVector::ZeroVector, Fwd * E.X + Side * E.Y, Fwd * E.X - Side * E.Y, -Fwd * E.X + Side * E.Y, -Fwd * E.X - Side * E.Y };
	const float Top = Loc.Z + Def.HalfExtent.Z + StepUp + 80.f;
	const float Bottom = Loc.Z - Def.RideHeight - 600.f;
	bool bHit = false;
	float GroundZ = -BIG_NUMBER;
	FVector Normal = FVector::ZeroVector;
	for (const FVector& O : Probes)
	{
		FHitResult H;
		const FVector XY(Loc.X + O.X, Loc.Y + O.Y, 0.f);
		if (W->LineTraceSingleByObjectType(H, FVector(XY.X, XY.Y, Top), FVector(XY.X, XY.Y, Bottom), FCollisionObjectQueryParams(ECC_WorldStatic), Q)
			&& H.ImpactNormal.Z > 0.6f)
		{
			bHit = true;
			GroundZ = FMath::Max(GroundZ, H.ImpactPoint.Z);
			Normal += H.ImpactNormal;
		}
	}

	FRotator Target = FRotator(0.f, Rot.Yaw, 0.f);
	if (bHit)
	{
		Normal = Normal.GetSafeNormal();
		const float TargetZ = GroundZ + Def.RideHeight;
		if (bSnap || Loc.Z - TargetZ < 45.f)
		{
			Loc.Z = Loc.Z < TargetZ ? TargetZ : FMath::FInterpTo(Loc.Z, TargetZ, Dt, 18.f);
			VerticalSpeed = 0.f;
			bGrounded = true;
			Target = FRotationMatrix::MakeFromXZ(FRotator(0.f, Rot.Yaw, 0.f).Vector(), Normal).Rotator();
			Target.Yaw = Rot.Yaw;
		}
		else
		{
			VerticalSpeed -= 980.f * Dt;   // off a ledge: fall to the ground below
			Loc.Z = FMath::Max(TargetZ, Loc.Z + VerticalSpeed * Dt);
			bGrounded = Loc.Z <= TargetZ + 1.f;
			if (bGrounded) { VerticalSpeed = 0.f; }
		}
	}
	else
	{
		VerticalSpeed -= 980.f * Dt;
		bGrounded = false;
		// fall with collision so a gap in one probe can never carry the box through solid ground
		FHitResult Stop;
		SetActorLocation(FVector(Loc.X, Loc.Y, Loc.Z + VerticalSpeed * Dt), true, &Stop);
		if (Stop.bBlockingHit && Stop.ImpactNormal.Z > 0.5f) { VerticalSpeed = 0.f; bGrounded = true; }
		Loc = GetActorLocation();
	}
	Rot.Pitch = FMath::FInterpTo(Rot.Pitch, Target.Pitch, Dt, 10.f);
	Rot.Roll = FMath::FInterpTo(Rot.Roll, Target.Roll, Dt, 10.f);
	SetActorLocationAndRotation(Loc, Rot);

	// safety net: remember where it last stood on solid ground; if it drops under the map, put it back there
	const double Now = W->GetTimeSeconds();
	if (bGrounded)
	{
		AirSince = -1.0;
		if (Now - LastGoodAt > 0.25) { LastGood = FTransform(FRotator(0.f, Rot.Yaw, 0.f), Loc); LastGoodAt = Now; bHasLastGood = true; }
	}
	else if (AirSince < 0.0)
	{
		AirSince = Now;
	}
	const bool bLost = !bGrounded && AirSince >= 0.0 && ((Now - AirSince > 2.5 && bHasLastGood && Loc.Z < LastGood.GetLocation().Z - 300.f)
		|| (bHasLastGood && Loc.Z < LastGood.GetLocation().Z - 2500.f));
	if (bLost)
	{
		UE_LOG(LogBullyVehicle, Warning, TEXT("BR_VEHICLE %s fell under the map at %s: back to %s"), *Def.Name, *Loc.ToCompactString(), *LastGood.GetLocation().ToCompactString());
		SetActorTransform(LastGood, false, nullptr, ETeleportType::TeleportPhysics);
		ResetMotion();
		AirSince = -1.0;
		++TotalRescues;
	}
}

void ABullyVehicle::UpdateCamera(float Dt)
{
	// look-around springs back behind the ride once you stop steering the camera and are moving
	if (GetWorld()->GetRealTimeSeconds() - LastLookTime > 1.2 && FMath::Abs(Speed) > 200.f)
	{
		LookOffset.Yaw = FMath::FInterpTo(LookOffset.Yaw, 0.f, Dt, 2.5f);
		LookOffset.Pitch = FMath::FInterpTo(LookOffset.Pitch, 0.f, Dt, 2.5f);
	}
	Boom->SetRelativeRotation(FRotator(-10.f + LookOffset.Pitch, LookOffset.Yaw, 0.f));
}

void ABullyVehicle::UpdateRiderPose(float Dt)
{
	ABullyCharacter* C = Rider.Get();
	UBullyAnimInstance* AI = C && C->GetMesh() ? Cast<UBullyAnimInstance>(C->GetMesh()->GetAnimInstance()) : nullptr;
	if (!AI || Def.RiderClips.Num() == 0) { return; }
	auto Pick = [this](const TCHAR* Key, FName Fallback) { const FName* F = Def.RiderClips.Find(FName(Key)); return F ? *F : Fallback; };
	const FName RideC = Pick(TEXT("ride"), NAME_None);
	const float S = bScripted ? ScriptSteer : Steer;
	const float T = bScripted ? ScriptThrottle : Throttle;
	const float AbsSpeed = FMath::Abs(Speed);
	FName Want = RideC;
	float Rate = FMath::Clamp(AbsSpeed / Def.RiderClipSpeed, 0.35f, 1.6f);
	if (!bGrounded) { Want = Pick(TEXT("air"), RideC); Rate = 1.f; }
	else if (AbsSpeed < 40.f && FMath::Abs(T) < 0.05f) { Want = Pick(TEXT("idle"), RideC); Rate = 1.f; }
	else if (S < -0.35f) { Want = Pick(TEXT("turn_l"), RideC); Rate = 1.f; }
	else if (S > 0.35f) { Want = Pick(TEXT("turn_r"), RideC); Rate = 1.f; }
	else if (T < 0.05f && AbsSpeed > 150.f) { Want = Pick(TEXT("coast"), RideC); Rate = 1.f; }
	else if (AbsSpeed > Def.MaxSpeed * 0.7f) { Want = Pick(TEXT("fast"), RideC); }
	AI->SetRideClip(Want, Rate);
}

void ABullyVehicle::UpdateWheels(float Dt)
{
	if (bRigLook)
	{
		// roll with the ground speed; the front pair also turns with the steering
		WheelSpin = FMath::Fmod(WheelSpin + FMath::RadiansToDegrees(Speed * Dt / RigWheelRadius), 360.f);
		const float SteerVis = (bScripted ? ScriptSteer : Steer) * 28.f;
		for (int32 i = 0; i < RigWheels.Num() && i < WheelMeshes.Num(); ++i)
		{
			const FWheelRig& W = RigWheels[i];
			const float Yaw = RigBaseYaw + (W.bLeft ? 180.f : 0.f) + (W.bFront ? SteerVis : 0.f);
			WheelMeshes[i]->SetRelativeRotation(FRotator(0.f, Yaw, W.bLeft ? WheelSpin : -WheelSpin));
		}
		return;
	}
	if (!Def.MeshPath.IsEmpty() || Def.Wheels == 0) { return; }
	const float SteerVis = (bScripted ? ScriptSteer : Steer) * 25.f;
	for (int32 i = 0; i < Def.Wheels && i < WheelMeshes.Num(); ++i)
	{
		const bool bFront = i % 2 == 0;
		WheelMeshes[i]->SetRelativeRotation(FRotator(0.f, bFront ? SteerVis : 0.f, 90.f));   // front wheels turn with the steering
	}
}

void ABullyVehicle::ShowStatus() const
{
	if (GEngine && IsLocallyControlled())
	{
		GEngine->AddOnScreenDebugMessage(uint64(GetUniqueID()) + 8000, 0.12f, FColor(250, 200, 60),
			FString::Printf(TEXT("%s  %3.0f km/h    W/S drive  A/D steer  SPACE handbrake  E get off"), *Def.Name, FMath::Abs(Speed) * 0.036f));
	}
}
