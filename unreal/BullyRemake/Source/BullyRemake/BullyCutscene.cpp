// OWNER: sm-cutscenes
#include "BullyCutscene.h"

#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "BullyHUD.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HighResScreenshot.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundWaveProcedural.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBullyCutscene, Log, All);

namespace
{
	// Actor yaw correction (degrees) between the importer's skeleton frame and the world; 0 = the glTF/UE placement
	// convention of tools/importers/README.md holds for the cutscene skeleton too (checked on the 1-1-1 render).
	TAutoConsoleVariable<float> CVarActorYaw(TEXT("br.Cutscene.ActorYaw"), 0.f, TEXT("Yaw added to every cutscene actor (deg)."));
	TAutoConsoleVariable<float> CVarSkipGrace(TEXT("br.Cutscene.SkipGrace"), 0.4f, TEXT("Seconds after start before Enter/B can skip."));

	/** Folders the importer output may be in: the project's Content/Imported (packaged copy) or the repo's imported/. */
	TArray<FString> CandidateDirs(const FString& Id)
	{
		return {
			FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Imported/Cutscenes"), Id),
			FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../../imported/cutscenes"), Id)),
		};
	}

	bool ParseWav(const TArray<uint8>& Bytes, int32& OutRate, int32& OutChannels, TArray<uint8>& OutPcm)
	{
		if (Bytes.Num() < 44 || FMemory::Memcmp(Bytes.GetData(), "RIFF", 4) != 0 || FMemory::Memcmp(Bytes.GetData() + 8, "WAVE", 4) != 0) { return false; }
		int64 Pos = 12;
		const int64 Total = Bytes.Num();
		int32 Bits = 0;
		OutRate = OutChannels = 0;
		while (Pos + 8 <= Total)
		{
			const uint8* P = Bytes.GetData() + Pos;
			const int64 Size = int64(P[4]) | (int64(P[5]) << 8) | (int64(P[6]) << 16) | (int64(P[7]) << 24);
			if (FMemory::Memcmp(P, "fmt ", 4) == 0 && Size >= 16 && Pos + 8 + 16 <= Total)
			{
				OutChannels = P[10] | (P[11] << 8);
				OutRate = P[12] | (P[13] << 8) | (P[14] << 16) | (P[15] << 24);
				Bits = P[22] | (P[23] << 8);
			}
			else if (FMemory::Memcmp(P, "data", 4) == 0)
			{
				const int64 Have = FMath::Min<int64>(Size, Total - Pos - 8);
				if (Have > 0) { OutPcm.Append(P + 8, Have); }
				break;
			}
			Pos += 8 + Size + (Size & 1);
		}
		return OutRate > 0 && OutChannels > 0 && Bits == 16 && OutPcm.Num() > 0;
	}

	void ReadFloats(const FJsonObject& O, const TCHAR* Field, TArray<float>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O.TryGetArrayField(Field, A)) { return; }
		Out.Reserve(A->Num());
		for (const TSharedPtr<FJsonValue>& V : *A) { Out.Add(float(V->AsNumber())); }
	}

	void ReadVecs(const FJsonObject& O, const TCHAR* Field, TArray<FVector>& Out)
	{
		TArray<float> F;
		ReadFloats(O, Field, F);
		Out.Reserve(F.Num() / 3);
		for (int32 i = 0; i + 2 < F.Num(); i += 3) { Out.Add(FVector(F[i], F[i + 1], F[i + 2])); }
	}
}

bool UBullyCutsceneSubsystem::ParseWavBytes(const TArray<uint8>& Bytes, int32& OutRate, int32& OutChannels, TArray<uint8>& OutPcm)
{
	return ParseWav(Bytes, OutRate, OutChannels, OutPcm);
}

UBullyCutsceneSubsystem* UBullyCutsceneSubsystem::Get(UWorld* World)
{
	return World ? World->GetSubsystem<UBullyCutsceneSubsystem>() : nullptr;
}

bool UBullyCutsceneSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer)) { return false; }
	const UWorld* W = Cast<UWorld>(Outer);
	if (!W || !W->IsGameWorld() || W->GetNetMode() == NM_DedicatedServer) { return false; }
	return true;
}

void UBullyCutsceneSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (FParse::Value(FCommandLine::Get(), TEXT("-BRCutsceneShots="), Shots.Id) && !Shots.Id.IsEmpty())
	{
		Shots.bEnabled = true;
		if (!FParse::Value(FCommandLine::Get(), TEXT("-BRShotDir="), Shots.Dir) || Shots.Dir.IsEmpty()) { Shots.Dir = FPaths::ProjectSavedDir() / TEXT("CutsceneShots"); }
		int32 V = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("-BRShotResX="), V) && V > 0) { Shots.ResX = V; }
		if (FParse::Value(FCommandLine::Get(), TEXT("-BRShotResY="), V) && V > 0) { Shots.ResY = V; }
		FString Times;
		if (FParse::Value(FCommandLine::Get(), TEXT("-BRCutsceneShotTimes="), Times, /*bShouldStopOnSeparator=*/false))
		{
			TArray<FString> Parts;
			Times.ParseIntoArray(Parts, TEXT(","));
			for (const FString& P : Parts) { Shots.Times.Add(FCString::Atof(*P)); }
		}
		if (Shots.Times.Num() == 0) { Shots.Times = { 2.f, 8.f, 15.f, 25.f, 40.f }; }
		Shots.Times.Sort();
		IFileManager::Get().MakeDirectory(*Shots.Dir, true);
		UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENESHOT armed: %s -> %s (%d time(s))"), *Shots.Id, *Shots.Dir, Shots.Times.Num());
	}
}

TStatId UBullyCutsceneSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBullyCutsceneSubsystem, STATGROUP_Tickables);
}

void UBullyCutsceneSubsystem::Deinitialize()
{
	if (bPlaying) { Finish(true, false); }
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------------------------------------------
// content

bool UBullyCutsceneSubsystem::LoadData(const FString& Id, FBullyCutsceneData& Out) const
{
	for (const FString& Dir : CandidateDirs(Id))
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, TEXT("cutscene.json")))) { continue; }
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOG(LogBullyCutscene, Warning, TEXT("BR_CUTSCENE %s: unreadable manifest in %s"), *Id, *Dir);
			continue;
		}
		Out = FBullyCutsceneData();
		Out.Id = Id;
		Out.Dir = Dir;
		double D = 0.0;
		if (Root->TryGetNumberField(TEXT("duration_s"), D)) { Out.Duration = float(D); }
		const TSharedPtr<FJsonObject>* Audio = nullptr;
		FString Wav;
		if (Root->TryGetObjectField(TEXT("audio"), Audio) && (*Audio)->TryGetStringField(TEXT("wav"), Wav))
		{
			const FString P = FPaths::Combine(Dir, Wav);
			if (IFileManager::Get().FileExists(*P)) { Out.WavPath = P; }
		}
		const TArray<TSharedPtr<FJsonValue>>* Subs = nullptr;
		if (Root->TryGetArrayField(TEXT("subtitles"), Subs))
		{
			for (const TSharedPtr<FJsonValue>& V : *Subs)
			{
				const TSharedPtr<FJsonObject> S = V->AsObject();
				FString T;
				if (!S.IsValid() || !S->TryGetStringField(TEXT("text"), T) || T.IsEmpty()) { continue; }
				FBullyCutsceneData::FSub Sub;
				Sub.Start = float(S->GetNumberField(TEXT("start")));
				Sub.End = float(S->GetNumberField(TEXT("end")));
				Sub.Text = T;
				Out.Subs.Add(MoveTemp(Sub));
			}
		}
		const TSharedPtr<FJsonObject>* CamO = nullptr;
		if (Root->TryGetObjectField(TEXT("camera"), CamO))
		{
			const FJsonObject& C = **CamO;
			if (C.TryGetNumberField(TEXT("hz"), D) && D > 0.0) { Out.CamHz = float(D); }
			ReadVecs(C, TEXT("pos_cm"), Out.CamPos);
			ReadVecs(C, TEXT("target_cm"), Out.CamTarget);
			ReadFloats(C, TEXT("fov_deg"), Out.CamFov);
			ReadFloats(C, TEXT("roll_deg"), Out.CamRoll);
			TArray<float> Cuts;
			ReadFloats(C, TEXT("cuts"), Cuts);
			for (float F : Cuts) { Out.CamCuts.Add(int32(F)); }
		}
		const TArray<TSharedPtr<FJsonValue>>* Acts = nullptr;
		if (Root->TryGetArrayField(TEXT("actors"), Acts))
		{
			for (const TSharedPtr<FJsonValue>& V : *Acts)
			{
				const TSharedPtr<FJsonObject> A = V->AsObject();
				if (!A.IsValid()) { continue; }
				FBullyCutsceneData::FActor Act;
				A->TryGetStringField(TEXT("name"), Act.Name);
				A->TryGetStringField(TEXT("kind"), Act.Kind);
				A->TryGetStringField(TEXT("ue_mesh"), Act.MeshPath);
				A->TryGetStringField(TEXT("ue_anim"), Act.AnimPath);
				Act.bCharacter = Act.Kind != TEXT("prop");
				const TArray<TSharedPtr<FJsonValue>>* Off = nullptr;
				if (A->TryGetArrayField(TEXT("offset_cm"), Off) && Off->Num() >= 3)
				{
					Act.Offset = FVector((*Off)[0]->AsNumber(), (*Off)[1]->AsNumber(), (*Off)[2]->AsNumber());
				}
				Out.Actors.Add(MoveTemp(Act));
			}
		}
		if (Out.Duration <= 0.f && Out.HasCamera()) { Out.Duration = float(Out.CamPos.Num() - 1) / Out.CamHz; }
		return true;
	}
	return false;
}

UBullyCutsceneSubsystem::ETier UBullyCutsceneSubsystem::Probe(const FString& Id)
{
	// -BRNoCutscenes: behave exactly like a clean clone with nothing imported (fallback verification)
	static const bool bDisabled = FParse::Param(FCommandLine::Get(), TEXT("BRNoCutscenes"));
	if (bDisabled) { return ETier::None; }
	FBullyCutsceneData D;
	if (!LoadData(Id, D) || (D.WavPath.IsEmpty() && D.Subs.Num() == 0) || D.Duration <= 0.f) { return ETier::None; }
	if (!D.HasCamera()) { return ETier::Audio; }
	int32 Chars = 0;
	for (const FBullyCutsceneData::FActor& A : D.Actors)
	{
		if (!A.bCharacter) { continue; }
		++Chars;
		if (!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(A.MeshPath))
			|| !FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(A.AnimPath)))
		{
			return ETier::Audio;
		}
	}
	return Chars > 0 ? ETier::Full : ETier::Audio;
}

// ---------------------------------------------------------------------------------------------------------------
// play / stop

UBullyCutsceneSubsystem::ETier UBullyCutsceneSubsystem::Play(const FString& Id, TSharedPtr<FBullyHUD> InHud, TFunction<void(bool)> OnDone, bool bAllowFull)
{
	UWorld* World = GetWorld();
	if (!World) { return ETier::None; }
	if (bPlaying) { Stop(); }
	ETier Tier = Probe(Id);
	if (Tier == ETier::None)
	{
		UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENE %s: not imported (run python3 -m tools.importers cutscene build %s)"), *Id, *Id);
		return ETier::None;
	}
	if (Tier == ETier::Full && !bAllowFull) { Tier = ETier::Audio; }
	if (!LoadData(Id, Cur)) { return ETier::None; }
	CurTier = Tier;
	DoneFn = MoveTemp(OnDone);
	Elapsed = 0.f;
	SubIndex = -1;
	bPlaying = true;

	Hud = InHud;
	bOwnHud = false;
	if (!Hud.IsValid())
	{
		Hud = MakeShared<FBullyHUD>();
		bOwnHud = Hud->Show(World);
		if (!bOwnHud) { Hud.Reset(); }
	}
	if (Hud.IsValid()) { Hud->SetLetterbox(true); }

	APlayerController* PC = World->GetFirstPlayerController();
	if (PC && !bInputIgnored)
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
		bInputIgnored = true;
	}
	if (Tier == ETier::Full)
	{
		if (!SpawnActors())
		{
			UE_LOG(LogBullyCutscene, Warning, TEXT("BR_CUTSCENE %s: actor spawn failed; dropping to the audio tier"), *Id);
			CurTier = ETier::Audio;
		}
	}
	if (CurTier == ETier::Full && PC)
	{
		HidePawns();
		Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(Cur.CamPos[0]));
		if (Cam)
		{
			Cam->GetCameraComponent()->bConstrainAspectRatio = false;
			PrevViewTarget = PC->GetViewTarget();
			ApplyCamera(0.f);
			PC->SetViewTarget(Cam);
		}
	}
	const bool bAudio = StartAudio();
	UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENE play %s tier=%s duration=%.1fs audio=%s subtitles=%d camera_frames=%d actors=%d"),
		*Id, CurTier == ETier::Full ? TEXT("full") : TEXT("audio"), Cur.Duration, bAudio ? TEXT("yes") : TEXT("no"),
		Cur.Subs.Num(), Cur.CamPos.Num(), ActorsSpawned.Num());
	return CurTier;
}

bool UBullyCutsceneSubsystem::StartAudio()
{
	if (Cur.WavPath.IsEmpty()) { return false; }
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Cur.WavPath)) { return false; }
	int32 Rate = 0, Channels = 0;
	TArray<uint8> Pcm;
	if (!ParseWav(Bytes, Rate, Channels, Pcm)) { UE_LOG(LogBullyCutscene, Warning, TEXT("BR_CUTSCENE %s: bad wav %s"), *Cur.Id, *Cur.WavPath); return false; }
	Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(Rate);
	Wave->NumChannels = Channels;
	Wave->Duration = float(Pcm.Num()) / float(Rate * Channels * 2);
	Wave->SoundGroup = SOUNDGROUP_Voice;
	Wave->bLooping = false;
	Wave->QueueAudio(Pcm.GetData(), Pcm.Num());
	AudioComp = UGameplayStatics::CreateSound2D(GetWorld(), Wave, 1.f, 1.f, 0.f, nullptr, false, false);
	if (!AudioComp) { return false; }   // -NoSound / no audio device: the clock still runs
	AudioComp->Play();
	return true;
}

bool UBullyCutsceneSubsystem::SpawnActors()
{
	UWorld* World = GetWorld();
	const float Yaw = CVarActorYaw.GetValueOnGameThread();
	for (const FBullyCutsceneData::FActor& A : Cur.Actors)
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *A.MeshPath);
		UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, *A.AnimPath);
		if (!Mesh || !Anim)
		{
			if (A.bCharacter) { UE_LOG(LogBullyCutscene, Warning, TEXT("BR_CUTSCENE %s: actor %s assets missing (%s)"), *Cur.Id, *A.Name, *A.MeshPath); return false; }
			UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENE %s: prop %s not imported, skipped"), *Cur.Id, *A.Name);
			continue;
		}
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASkeletalMeshActor* Act = World->SpawnActor<ASkeletalMeshActor>(A.Offset, FRotator(0.f, Yaw, 0.f), P);
		if (!Act) { return false; }
		USkeletalMeshComponent* C = Act->GetSkeletalMeshComponent();
		C->SetMobility(EComponentMobility::Movable);
		C->SetSkeletalMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		C->SetBoundsScale(4.f);   // the Root bone travels metres from the actor origin over a scene
		C->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		C->PlayAnimation(Anim, false);
		C->SetPlayRate(0.f);      // scrubbed to the cutscene clock every tick (ApplyActors)
		C->SetPosition(0.f, false);
		Act->Tags.Add(FName(*FString::Printf(TEXT("br_cutscene:%s"), *A.Name)));
		ActorsSpawned.Add(Act);
		ActorAnims.Add(Anim);
	}
	return ActorsSpawned.Num() > 0;
}

void UBullyCutsceneSubsystem::HidePawns()
{
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (!It->IsHidden())
		{
			It->SetActorHiddenInGame(true);
			Hidden.Add(*It);
		}
	}
}

void UBullyCutsceneSubsystem::Skip()
{
	if (!bPlaying) { return; }
	UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENE skip %s at %.1fs"), *Cur.Id, Elapsed);
	Finish(true, true);
}

void UBullyCutsceneSubsystem::Stop()
{
	if (!bPlaying) { return; }
	UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENE stop %s at %.1fs (no callback)"), *Cur.Id, Elapsed);
	Finish(true, false);
}

void UBullyCutsceneSubsystem::Finish(bool bSkipped, bool bCallDone)
{
	bPlaying = false;
	if (AudioComp) { AudioComp->Stop(); AudioComp->DestroyComponent(); }
	AudioComp = nullptr;
	Wave = nullptr;
	for (ASkeletalMeshActor* A : ActorsSpawned) { if (IsValid(A)) { A->Destroy(); } }
	ActorsSpawned.Reset();
	ActorAnims.Reset();
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (Cam)
	{
		if (PC && PC->GetViewTarget() == Cam)
		{
			AActor* Back = PrevViewTarget.Get();
			if (!Back || Back == Cam) { Back = PC->GetPawn(); }
			if (Back) { PC->SetViewTarget(Back); }
		}
		Cam->Destroy();
		Cam = nullptr;
	}
	for (const TWeakObjectPtr<AActor>& A : Hidden) { if (A.IsValid()) { A->SetActorHiddenInGame(false); } }
	Hidden.Reset();
	if (PC && bInputIgnored)
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}
	bInputIgnored = false;
	if (Hud.IsValid())
	{
		Hud->Say(TEXT(""), TEXT(""), 0.f);
		Hud->SetLetterbox(false);
		if (bOwnHud) { Hud->Hide(); }
	}
	Hud.Reset();
	bOwnHud = false;
	const FString Id = Cur.Id;
	UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENE done %s (%s at %.1f/%.1fs)"), *Id, bSkipped ? TEXT("skipped") : TEXT("finished"), Elapsed, Cur.Duration);
	CurTier = ETier::None;
	TFunction<void(bool)> Fn = MoveTemp(DoneFn);
	DoneFn = nullptr;
	if (bCallDone && Fn) { Fn(bSkipped); }
}

// ---------------------------------------------------------------------------------------------------------------
// per frame

void UBullyCutsceneSubsystem::ApplyCamera(float T)
{
	if (!Cam || !Cur.HasCamera()) { return; }
	const int32 N = Cur.CamPos.Num();
	const float F = FMath::Clamp(T * Cur.CamHz, 0.f, float(N - 1));
	const int32 I = FMath::Min(int32(F), N - 1);
	const int32 J = FMath::Min(I + 1, N - 1);
	// a cut segment holds frame I until the next frame time (never blend through a hard cut)
	const float A = Cur.CamCuts.Contains(I) ? 0.f : F - float(I);
	const FVector Pos = FMath::Lerp(Cur.CamPos[I], Cur.CamPos[J], A);
	const FVector Tgt = FMath::Lerp(Cur.CamTarget[I], Cur.CamTarget[J], A);
	FRotator Rot = (Tgt - Pos).Rotation();
	if (Cur.CamRoll.IsValidIndex(J)) { Rot.Roll = FMath::Lerp(Cur.CamRoll[I], Cur.CamRoll[J], A); }
	Cam->SetActorLocationAndRotation(Pos, Rot);
	if (Cur.CamFov.IsValidIndex(J))
	{
		// INFERRED: the stored FOV is horizontal for a 4:3 frame; keep the vertical extent on wider screens (Hor+)
		const float F43 = FMath::Lerp(Cur.CamFov[I], Cur.CamFov[J], A);
		float Aspect = 16.f / 9.f;
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
		{
			const FIntPoint Sz = GEngine->GameViewport->Viewport->GetSizeXY();
			if (Sz.X > 0 && Sz.Y > 0) { Aspect = float(Sz.X) / float(Sz.Y); }
		}
		const float Half = FMath::DegreesToRadians(FMath::Clamp(F43, 5.f, 150.f) * 0.5f);
		const float H = 2.f * FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(Half) * Aspect / (4.f / 3.f)));
		Cam->GetCameraComponent()->SetFieldOfView(FMath::Clamp(H, 5.f, 170.f));
	}
}

void UBullyCutsceneSubsystem::ApplyActors(float T)
{
	for (int32 i = 0; i < ActorsSpawned.Num(); ++i)
	{
		ASkeletalMeshActor* A = ActorsSpawned[i];
		if (!IsValid(A)) { continue; }
		const float Len = ActorAnims.IsValidIndex(i) && ActorAnims[i] ? ActorAnims[i]->GetPlayLength() : T;
		A->GetSkeletalMeshComponent()->SetPosition(FMath::Clamp(T, 0.f, Len), false);
	}
}

void UBullyCutsceneSubsystem::ApplySubtitle(float T)
{
	int32 Want = -1;
	for (int32 i = 0; i < Cur.Subs.Num(); ++i)
	{
		if (T >= Cur.Subs[i].Start && T < Cur.Subs[i].End) { Want = i; break; }
	}
	if (Want == SubIndex) { return; }
	SubIndex = Want;
	if (!Hud.IsValid()) { return; }
	if (Want < 0) { Hud->Say(TEXT(""), TEXT(""), 0.f); return; }
	const FBullyCutsceneData::FSub& S = Cur.Subs[Want];
	Hud->Say(TEXT(""), S.Text, FMath::Max(0.2f, S.End - T));
}

void UBullyCutsceneSubsystem::Tick(float Dt)
{
	if (Shots.bEnabled) { TickShots(Dt); }
	if (!bPlaying) { return; }
	UWorld* World = GetWorld();
	if (!World) { return; }
	// shots mode holds the clock at each shot time until the frame is captured (TickShots releases it)
	const bool bHold = Shots.bEnabled && Shots.Next < Shots.Times.Num() && Elapsed >= Shots.Times[Shots.Next];
	if (!bHold) { Elapsed += Dt; }
	if (Shots.bEnabled && Shots.Next < Shots.Times.Num()) { Elapsed = FMath::Min(Elapsed, Shots.Times[Shots.Next]); }
	if (APlayerController* PC = World->GetFirstPlayerController(); PC && Elapsed > CVarSkipGrace.GetValueOnGameThread())
	{
		if (PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right)) { Skip(); return; }
	}
	ApplyCamera(Elapsed);
	ApplyActors(Elapsed);
	ApplySubtitle(Elapsed);
	if (bOwnHud && Hud.IsValid()) { Hud->Tick(Dt); }
	if (Elapsed >= Cur.Duration) { Finish(false, true); }
}

// ---------------------------------------------------------------------------------------------------------------
// offscreen evidence (-BRCutsceneShots=<id>)

void UBullyCutsceneSubsystem::TickShots(float Dt)
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	const double Now = World->GetTimeSeconds();
	if (!Shots.bStarted && !Shots.bQuitting)
	{
		if (!World->GetFirstPlayerController() || !World->GetFirstPlayerController()->GetPawn()) { return; }
		int32 Assets = 0, ShaderJobs = 0;
#if WITH_EDITOR
		Assets = FAssetCompilingManager::Get().GetNumRemainingAssets();
		ShaderJobs = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
#endif
		if (Assets == 0 && ShaderJobs == 0) { if (Shots.IdleSince < 0.0) { Shots.IdleSince = Now; } }
		else { Shots.IdleSince = -1.0; }
		if (Now - Shots.LastLog >= 10.0)
		{
			Shots.LastLog = Now;
			UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENESHOT waiting for compilation: %d assets, %d shader jobs (t=%.0f)"), Assets, ShaderJobs, Now);
		}
		if (Shots.IdleSince < 0.0 || Now - Shots.IdleSince < 3.0) { return; }
		if (Shots.ReadyAt < 0.0) { Shots.ReadyAt = Now; }
		if (Now - Shots.ReadyAt < Shots.Warmup) { return; }
		Shots.bStarted = true;
		// stand the (hidden) player at the scene so streaming/LOD/lighting are those of the cutscene's location
		FBullyCutsceneData Peek;
		if (LoadData(Shots.Id, Peek) && Peek.Actors.Num() > 0)
		{
			if (APawn* P = World->GetFirstPlayerController()->GetPawn())
			{
				const FVector At = Peek.Actors[0].Offset + FVector(0, 0, 120.0);
				P->TeleportTo(At, P->GetActorRotation(), false, true);
				UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENESHOT player moved to %s (scene origin of %s)"), *At.ToCompactString(), *Peek.Actors[0].Name);
			}
		}
		const bool bAudioOnly = FParse::Param(FCommandLine::Get(), TEXT("BRCutsceneAudioTier"));
		if (Play(Shots.Id, nullptr, [this](bool) { Shots.QuitAt = GetWorld()->GetTimeSeconds() + 1.0; }, !bAudioOnly) == ETier::None)
		{
			UE_LOG(LogBullyCutscene, Error, TEXT("BR_CUTSCENESHOT %s: nothing to play; quitting"), *Shots.Id);
			Shots.QuitAt = Now;
		}
		Shots.Hold = 0;
		return;
	}
	if (Shots.QuitAt >= 0.0 && Now >= Shots.QuitAt && !Shots.bQuitting)
	{
		Shots.bQuitting = true;
		UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENESHOT done: %d shot(s); quitting"), Shots.Next);
		GEngine->Exec(World, TEXT("quit"));
		return;
	}
	if (!bPlaying || Shots.Next >= Shots.Times.Num()) { return; }
	if (Elapsed < Shots.Times[Shots.Next]) { return; }
	// held at the shot time: let a few frames render (temporal AA, streaming) before the capture
	if (++Shots.Hold < 24) { return; }
	const FString File = Shots.Dir / FString::Printf(TEXT("%s_%02d_t%05.1f.png"), *Shots.Id, Shots.Next, Shots.Times[Shots.Next]);
	FHighResScreenshotConfig& HR = GetHighResScreenshotConfig();
	HR.SetResolution(uint32(Shots.ResX), uint32(Shots.ResY), 1.0f);
	HR.SetFilename(File);
	HR.bMaskEnabled = false;
	HR.bDumpBufferVisualizationTargets = false;
	const bool bOk = GEngine->GameViewport && GEngine->GameViewport->Viewport && GEngine->GameViewport->Viewport->TakeHighResScreenShot();
	UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENESHOT %s t=%.2f -> %s (sub %d)"), bOk ? TEXT("shot") : TEXT("REFUSED"), Elapsed, *File, SubIndex);
	// placement evidence: where each actor's Root bone and the camera are, in world cm
	for (ASkeletalMeshActor* A : ActorsSpawned)
	{
		if (!IsValid(A)) { continue; }
		USkeletalMeshComponent* C = A->GetSkeletalMeshComponent();
		const FName RootBone = C->GetNumBones() > 2 ? C->GetBoneName(2) : NAME_None;
		const FVector R = RootBone.IsNone() ? A->GetActorLocation() : C->GetBoneLocation(RootBone);
		const FVector Fwd = RootBone.IsNone() ? A->GetActorForwardVector() : C->GetBoneQuaternion(RootBone).GetAxisX();
		UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENESHOT   actor %s root=%s fwd=%s"), *A->Tags[0].ToString(), *R.ToCompactString(), *Fwd.ToCompactString());
	}
	if (Cam) { UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENESHOT   camera at %s rot %s fov %.1f"), *Cam->GetActorLocation().ToCompactString(), *Cam->GetActorRotation().ToCompactString(), Cam->GetCameraComponent()->FieldOfView); }
	++Shots.Next;
	Shots.Hold = 0;
	if (Shots.Next >= Shots.Times.Num()) { Shots.QuitAt = Now + 2.0; }
}

// ---------------------------------------------------------------------------------------------------------------
// console

namespace
{
	UBullyCutsceneSubsystem* CutsceneSub(UWorld* W)
	{
		UBullyCutsceneSubsystem* S = UBullyCutsceneSubsystem::Get(W);
		if (!S) { UE_LOG(LogBullyCutscene, Warning, TEXT("BR_CUTSCENE: no cutscene subsystem in this world")); }
		return S;
	}

	FAutoConsoleCommandWithWorldAndArgs GCsPlay(TEXT("Cutscene.Play"), TEXT("Cutscene.Play <id> [audio]: play an imported original cutscene (audio = skip the actor/camera tier)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W)
		{
			if (A.Num() < 1) { return; }
			if (UBullyCutsceneSubsystem* S = CutsceneSub(W)) { S->Play(A[0], nullptr, nullptr, !(A.Num() > 1 && A[1] == TEXT("audio"))); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GCsSkip(TEXT("Cutscene.Skip"), TEXT("Skip the playing cutscene"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W) { if (UBullyCutsceneSubsystem* S = CutsceneSub(W)) { S->Skip(); } }));
	FAutoConsoleCommandWithWorldAndArgs GCsStatus(TEXT("Cutscene.Status"), TEXT("Log the cutscene player state"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W)
		{
			if (UBullyCutsceneSubsystem* S = CutsceneSub(W))
			{
				UE_LOG(LogBullyCutscene, Display, TEXT("BR_CUTSCENE status: %s %s t=%.1f"), S->IsPlaying() ? TEXT("playing") : TEXT("idle"), *S->GetCurrentId(), S->GetElapsed());
			}
		}));
}
