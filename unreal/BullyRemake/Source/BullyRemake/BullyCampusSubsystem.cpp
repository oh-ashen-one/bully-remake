// OWNER: sm-campus
#include "BullyCampusSubsystem.h"

#include "AmbientSubsystem.h"
#include "AmbientWalker.h"
#include "BullyCampaignSubsystem.h"
#include "BullyCharacter.h"
#include "BullyClockSubsystem.h"
#include "BullyCombatComponent.h"
#include "BullyCutscene.h"
#include "BullyHUD.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MissionSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyCampus, Log, All);

namespace
{
	TSharedPtr<FJsonObject> ReadJsonFile(const FString& Path)
	{
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { return nullptr; }
		return Root;
	}

	FString RepoPath(const FString& Rel)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../.."), Rel));
	}

	bool ReadVec3(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, FVector& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(Key, A) || A->Num() < 3) { return false; }
		Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		return true;
	}

	float Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, float Def)
	{
		double V = 0.0;
		return (O.IsValid() && O->TryGetNumberField(Key, V)) ? float(V) : Def;
	}

	TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* R = nullptr;
		return (O.IsValid() && O->TryGetObjectField(Key, R) && R) ? *R : nullptr;
	}

	bool PointInPoly(const TArray<FVector2D>& P, const FVector2D& Q)
	{
		bool bIn = false;
		for (int32 i = 0, j = P.Num() - 1; i < P.Num(); j = i++)
		{
			if (((P[i].Y > Q.Y) != (P[j].Y > Q.Y)) && (Q.X < (P[j].X - P[i].X) * (Q.Y - P[i].Y) / (P[j].Y - P[i].Y + 1e-6) + P[i].X)) { bIn = !bIn; }
		}
		return bIn;
	}

	const TCHAR* AttitudeName(int32 A)
	{
		static const TCHAR* N[] = { TEXT("Abhor"), TEXT("Averse"), TEXT("Dispassionate"), TEXT("Dig"), TEXT("Adore") };
		return N[FMath::Clamp(A, 0, 4)];
	}

	FString ModelOfMesh(const FString& MeshPath)
	{
		// /Game/Imported/Characters/<Model>/SK_<Model>.SK_<Model>
		TArray<FString> Parts;
		MeshPath.ParseIntoArray(Parts, TEXT("/"));
		return Parts.Num() >= 4 ? Parts[3] : FString();
	}
}

UBullyCampusSubsystem* UBullyCampusSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UBullyCampusSubsystem>() : nullptr;
}

bool UBullyCampusSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer)) { return false; }
	const UWorld* W = Cast<UWorld>(Outer);
	return W && W->IsGameWorld();
}

TStatId UBullyCampusSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBullyCampusSubsystem, STATGROUP_Tickables);
}

void UBullyCampusSubsystem::Deinitialize()
{
	for (UAudioComponent* A : LiveAudio) { if (A) { A->Stop(); } }
	LiveAudio.Reset();
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------------------------------------------
// setup

void UBullyCampusSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	TArray<FString> Tokens, Switches;
	FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);
	bool bForced = false, bOff = false, bHarness = false;
	for (const FString& S : Switches)
	{
		if (S.Equals(TEXT("BRCampusLife"), ESearchCase::IgnoreCase)) { bForced = true; }
		else if (S.Equals(TEXT("BRCampusSmoke"), ESearchCase::IgnoreCase)) { bForced = bSmoke = true; }
		else if (S.Equals(TEXT("BRNoCampusLife"), ESearchCase::IgnoreCase)) { bOff = true; }
		else if (S.StartsWith(TEXT("BRStory"), ESearchCase::IgnoreCase) && !S.StartsWith(TEXT("BRStorySmoke"), ESearchCase::IgnoreCase)) { }
		else if (S.Equals(TEXT("BRTownLife"), ESearchCase::IgnoreCase) || S.Equals(TEXT("BRNoTownLife"), ESearchCase::IgnoreCase)) { }
		else if (S.StartsWith(TEXT("BR"), ESearchCase::IgnoreCase)) { bHarness = true; }
	}
	const bool bEnabled = LoadConfig();
	bLife = !bOff && bEnabled && (bForced || !bHarness) && InWorld.GetName().Contains(TEXT("Bullworth"));
	if (bLife)
	{
		LoadImported();
		LoadSpeech();
	}
	SkippedClasses.Init(false, FMath::Max(1, int32(Num(Obj(Cfg, TEXT("schedule")), TEXT("skipped_window"), 5.f))));
	bBegun = true;
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS %s (trouble core on; life %s%s)"), bLife ? TEXT("life on") : TEXT("life off"),
		bOff ? TEXT("disabled by -BRNoCampusLife") : (!bEnabled ? TEXT("disabled in Config/Campus.json") : (bLife ? (bForced ? TEXT("forced") : TEXT("story play")) : TEXT("harness run"))),
		bSmoke ? TEXT(", campus smoke") : TEXT(""));
}

bool UBullyCampusSubsystem::LoadConfig()
{
	Cfg = ReadJsonFile(FPaths::ProjectConfigDir() / TEXT("Campus.json"));
	if (!Cfg.IsValid()) { UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS no Config/Campus.json: campus life off")); return false; }
	bool bEnabled = true;
	Cfg->TryGetBoolField(TEXT("enabled"), bEnabled);

	const TSharedPtr<FJsonObject> Sch = Obj(Cfg, TEXT("schedule"));
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (Sch.IsValid() && Sch->TryGetArrayField(TEXT("periods"), A))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) { continue; }
			FPeriod P;
			O->TryGetStringField(TEXT("id"), P.Id);
			P.Start = Num(O, TEXT("start"), 0.f);
			P.End = Num(O, TEXT("end"), 0.f);
			O->TryGetBoolField(TEXT("class"), P.bClass);
			Periods.Add(P);
		}
	}
	HeadToClassLead = Num(Sch, TEXT("head_to_class_lead_minutes"), 15.f) / 60.f;
	LateAfter = Num(Sch, TEXT("late_after_minutes"), 2.f) / 60.f;
	StreamOutMinutes = Num(Sch, TEXT("stream_out_minutes"), 30.f);
	DormStart = Num(Sch, TEXT("dorm_hour"), 22.5f);
	SchoolClosedStart = Num(Sch, TEXT("school_closed_hour"), 18.75f);
	if (Sch.IsValid() && Sch->TryGetArrayField(TEXT("curfew"), A) && A->Num() == 2) { CurfewStart = (*A)[0]->AsNumber(); CurfewEnd = (*A)[1]->AsNumber(); }
	if (Sch.IsValid()) { Sch->TryGetStringField(TEXT("truancy_after_mission"), TruancyAfterMission); }
	if (Sch.IsValid() && Sch->TryGetArrayField(TEXT("skipping_punishment"), A))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (O.IsValid()) { SkippingThresholds.Add(FIntPoint(int32(Num(O, TEXT("skipped"), 1.f)), int32(Num(O, TEXT("points"), 100.f)))); }
		}
	}

	const TSharedPtr<FJsonObject> Tr = Obj(Cfg, TEXT("trouble"));
	MaxPoints = Num(Tr, TEXT("max"), MaxPoints);
	ChaseThreshold = Num(Tr, TEXT("chase_threshold"), ChaseThreshold);
	DecayUnseen = Num(Tr, TEXT("decay_unseen_per_s"), DecayUnseen);
	DecayHidden = Num(Tr, TEXT("decay_hidden_per_s"), DecayHidden);
	TruancyFloor = Num(Tr, TEXT("truancy_floor"), TruancyFloor);
	TrespassPerSecond = Num(Tr, TEXT("trespass_per_s"), TrespassPerSecond);
	HitStudentPoints = Num(Tr, TEXT("hit_student"), HitStudentPoints);
	HitPrefectPoints = Num(Tr, TEXT("hit_prefect"), HitPrefectPoints);

	const TSharedPtr<FJsonObject> Pf = Obj(Cfg, TEXT("prefects"));
	VisionHalfCos = FMath::Cos(FMath::DegreesToRadians(Num(Pf, TEXT("vision_yaw_deg"), 100.f) * 0.5f));
	VisionRange = Num(Pf, TEXT("vision_range_m"), 12.f) * 100.f;
	BustRadius = Num(Pf, TEXT("bust_radius_cm"), BustRadius);
	GiveUpSeconds = Num(Pf, TEXT("give_up_seconds"), GiveUpSeconds);
	GiveUpRange = Num(Pf, TEXT("give_up_range_m"), 30.f) * 100.f;

	// cast fallbacks (the importer's cast.json overrides faction / voice / aggression per model)
	if (Cfg->TryGetArrayField(TEXT("cast"), A))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) { continue; }
			FCastRow R;
			O->TryGetStringField(TEXT("model"), R.Model);
			O->TryGetStringField(TEXT("faction"), R.Faction);
			O->TryGetStringField(TEXT("voice"), R.Voice);
			O->TryGetBoolField(TEXT("female"), R.bFemale);
			R.Aggression = Num(O, TEXT("aggression"), 0.f);
			R.bPrefect = R.Faction == TEXT("prefect");
			if (!R.Model.IsEmpty()) { CastRows.Add(R.Model, R); }
		}
	}

	// barks: context -> codes; conversations: [opener, reply]
	if (const TSharedPtr<FJsonObject> B = Obj(Cfg, TEXT("barks")))
	{
		for (const auto& KV : B->Values)
		{
			if (KV.Value->Type != EJson::Array) { continue; }
			TArray<FString>& Codes = BarkCodes.FindOrAdd(FString(KV.Key));
			for (const TSharedPtr<FJsonValue>& C : KV.Value->AsArray())
			{
				if (C->Type == EJson::String) { Codes.Add(C->AsString()); }
			}
		}
		if (B->TryGetArrayField(TEXT("conversations"), A))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TArray<TSharedPtr<FJsonValue>>& P = V->AsArray();
				if (P.Num() >= 2) { ConversationPairs.Add(TPair<FString, FString>(P[0]->AsString(), P[1]->AsString())); }
			}
		}
	}
	return bEnabled;
}

bool UBullyCampusSubsystem::LoadTriggerFile(const FString& Stem)
{
	if (Triggers.Contains(Stem)) { return Triggers[Stem].IsValid(); }
	TSharedPtr<FJsonObject> R = ReadJsonFile(RepoPath(FString::Printf(TEXT("imported/triggers/%s.json"), *Stem)));
	Triggers.Add(Stem, R);
	return R.IsValid();
}

bool UBullyCampusSubsystem::FindPoint(const FString& Ref, FVector& Out) const
{
	// "<file>/<point name>" (a trigger-file point, or a POI point "<file>/<poi>#<i>"), or a fallback in places.fallback_cm
	FString Stem, Name;
	if (Ref.Split(TEXT("/"), &Stem, &Name))
	{
		const_cast<UBullyCampusSubsystem*>(this)->LoadTriggerFile(Stem);
		const TSharedPtr<FJsonObject>* F = Triggers.Find(Stem);
		const TArray<TSharedPtr<FJsonValue>>* Es = nullptr;
		FString Poi;
		int32 Idx = 0;
		FString N = Name;
		if (Name.Split(TEXT("#"), &Poi, nullptr)) { Idx = FCString::Atoi(*Name.RightChop(Poi.Len() + 1)); N = Poi; }
		if (F && F->IsValid() && (*F)->TryGetArrayField(TEXT("entities"), Es))
		{
			for (const TSharedPtr<FJsonValue>& V : *Es)
			{
				const TSharedPtr<FJsonObject> E = V->AsObject();
				FString EN, Ty;
				if (!E.IsValid() || !E->TryGetStringField(TEXT("name"), EN) || !EN.Equals(N, ESearchCase::IgnoreCase)) { continue; }
				E->TryGetStringField(TEXT("type"), Ty);
				if (Ty == TEXT("point") && ReadVec3(E, TEXT("location_cm"), Out)) { return true; }
				const TArray<TSharedPtr<FJsonValue>>* Pts = nullptr;
				if (Ty == TEXT("poi") && E->TryGetArrayField(TEXT("points"), Pts) && Pts->IsValidIndex(Idx) && ReadVec3((*Pts)[Idx]->AsObject(), TEXT("location_cm"), Out)) { return true; }
			}
		}
	}
	const TSharedPtr<FJsonObject> Fb = Obj(Obj(Cfg, TEXT("places")), TEXT("fallback_cm"));
	return ReadVec3(Fb, *Ref, Out);
}

bool UBullyCampusSubsystem::FindPath(const FString& Ref, TArray<FVector>& Out) const
{
	FString Stem, Name;
	if (!Ref.Split(TEXT("/"), &Stem, &Name)) { return false; }
	const_cast<UBullyCampusSubsystem*>(this)->LoadTriggerFile(Stem);
	const TSharedPtr<FJsonObject>* F = Triggers.Find(Stem);
	const TArray<TSharedPtr<FJsonValue>>* Es = nullptr;
	if (!F || !F->IsValid() || !(*F)->TryGetArrayField(TEXT("entities"), Es)) { return false; }
	for (const TSharedPtr<FJsonValue>& V : *Es)
	{
		const TSharedPtr<FJsonObject> E = V->AsObject();
		FString EN, Ty;
		if (!E.IsValid() || !E->TryGetStringField(TEXT("name"), EN) || !EN.Equals(Name, ESearchCase::IgnoreCase)) { continue; }
		const TArray<TSharedPtr<FJsonValue>>* Wps = nullptr;
		if (E->TryGetStringField(TEXT("type"), Ty) && Ty == TEXT("path") && E->TryGetArrayField(TEXT("waypoints"), Wps))
		{
			for (const TSharedPtr<FJsonValue>& W : *Wps)
			{
				FVector P;
				if (ReadVec3(W->AsObject(), TEXT("location_cm"), P)) { Out.Add(P); }
			}
			return Out.Num() >= 2;
		}
	}
	return false;
}

bool UBullyCampusSubsystem::FindVolume(const FString& Ref, FVolume& Out) const
{
	FString Stem, Name;
	if (Ref.Split(TEXT("/"), &Stem, &Name))
	{
		const_cast<UBullyCampusSubsystem*>(this)->LoadTriggerFile(Stem);
		const TSharedPtr<FJsonObject>* F = Triggers.Find(Stem);
		const TArray<TSharedPtr<FJsonValue>>* Es = nullptr;
		if (F && F->IsValid() && (*F)->TryGetArrayField(TEXT("entities"), Es))
		{
			for (const TSharedPtr<FJsonValue>& V : *Es)
			{
				const TSharedPtr<FJsonObject> E = V->AsObject();
				FString EN;
				if (!E.IsValid() || !E->TryGetStringField(TEXT("name"), EN) || !EN.Equals(Name, ESearchCase::IgnoreCase)) { continue; }
				const TSharedPtr<FJsonObject> Bb = Obj(E, TEXT("aabb_cm"));
				if (!ReadVec3(Bb, TEXT("min"), Out.Min) || !ReadVec3(Bb, TEXT("max"), Out.Max)) { continue; }
				Out.Name = Name;
				const TArray<TSharedPtr<FJsonValue>>* Poly = nullptr;
				if (E->TryGetArrayField(TEXT("polygon_cm"), Poly))
				{
					for (const TSharedPtr<FJsonValue>& P : *Poly)
					{
						const TArray<TSharedPtr<FJsonValue>>& XY = P->AsArray();
						if (XY.Num() >= 2) { Out.Poly.Add(FVector2D(XY[0]->AsNumber(), XY[1]->AsNumber())); }
					}
					Out.bPoly = Out.Poly.Num() >= 3;
				}
				return true;
			}
		}
	}
	// fallback box: places.fallback_boxes_cm.<ref> = [minx, miny, minz, maxx, maxy, maxz]
	const TSharedPtr<FJsonObject> Fb = Obj(Obj(Cfg, TEXT("places")), TEXT("fallback_boxes_cm"));
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (Fb.IsValid() && Fb->TryGetArrayField(*Ref, A) && A->Num() == 6)
	{
		Out.Name = Ref;
		Out.Min = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		Out.Max = FVector((*A)[3]->AsNumber(), (*A)[4]->AsNumber(), (*A)[5]->AsNumber());
		return true;
	}
	return false;
}

void UBullyCampusSubsystem::LoadImported()
{
	const TSharedPtr<FJsonObject> Pl = Obj(Cfg, TEXT("places"));
	auto ReadPoints = [&](const TCHAR* Key, TArray<FVector>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!Pl.IsValid() || !Pl->TryGetArrayField(Key, A)) { return; }
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			FVector P;
			if (FindPoint(V->AsString(), P)) { Out.Add(P); }
		}
	};
	ReadPoints(TEXT("grounds_doors"), GroundsDoors);
	ReadPoints(TEXT("halls_doors"), HallsDoors);
	ReadPoints(TEXT("dorm_doors"), DormDoors);
	FString S;
	if (Pl.IsValid() && Pl->TryGetStringField(TEXT("class_exit"), S)) { FindPoint(S, ClassExitPoint); }
	if (Pl.IsValid() && Pl->TryGetStringField(TEXT("principal_spawn"), S)) { FindPoint(S, PrincipalSpawn); }
	if (Pl.IsValid() && Pl->TryGetStringField(TEXT("principal_outside"), S)) { FindPoint(S, PrincipalOutside); }
	if (Pl.IsValid() && Pl->TryGetStringField(TEXT("school_front"), S)) { FindPoint(S, SchoolFront); }
	if (Pl.IsValid() && Pl->TryGetStringField(TEXT("bell"), S)) { FindPoint(S, BellPoint); }
	if (Pl.IsValid() && Pl->TryGetStringField(TEXT("grounds_volume"), S)) { FindVolume(S, Grounds); }
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (Pl.IsValid() && Pl->TryGetArrayField(TEXT("school_volumes"), A))
	{
		for (const TSharedPtr<FJsonValue>& V : *A) { FVolume Vol; if (FindVolume(V->AsString(), Vol)) { SchoolVolumes.Add(Vol); } }
	}
	if (Pl.IsValid() && Pl->TryGetArrayField(TEXT("trespass_volumes"), A))
	{
		for (const TSharedPtr<FJsonValue>& V : *A) { FVolume Vol; if (FindVolume(V->AsString(), Vol)) { TrespassVolumes.Add(Vol); } }
	}

	// install-derived cast table (faction / voice / aggression / vision) overrides the config fallbacks
	if (const TSharedPtr<FJsonObject> Imp = ReadJsonFile(RepoPath(TEXT("imported/campus/cast.json"))))
	{
		if (const TSharedPtr<FJsonObject> C = Obj(Imp, TEXT("cast")))
		{
			for (const auto& KV : C->Values)
			{
				const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
				if (!O.IsValid()) { continue; }
				FCastRow& R = CastRows.FindOrAdd(FString(KV.Key));
				R.Model = FString(KV.Key);
				O->TryGetStringField(TEXT("faction"), R.Faction);
				FString Voice;
				if (O->TryGetStringField(TEXT("voice"), Voice) && !Voice.IsEmpty()) { R.Voice = Voice; }
				O->TryGetBoolField(TEXT("female"), R.bFemale);
				if (const TSharedPtr<FJsonObject> St = Obj(O, TEXT("stats"))) { R.Aggression = Num(St, TEXT("aggression"), R.Aggression); R.Chicken = Num(St, TEXT("chicken"), 0.f); }
				R.bPrefect = R.Faction == TEXT("prefect");
			}
		}
		if (const TSharedPtr<FJsonObject> Ps = Obj(Imp, TEXT("prefect_stats")))
		{
			VisionHalfCos = FMath::Cos(FMath::DegreesToRadians(Num(Ps, TEXT("vision_yaw_deg"), 100.f) * 0.5f));
			VisionRange = Num(Ps, TEXT("vision_range_m"), VisionRange / 100.f) * 100.f;
		}
	}

	// the bell: loop x N + end, one PCM buffer (F_RingSchoolBell's "School Bell 2D" / "Half Bell 2D" play these waves)
	TArray<uint8> LoopB, EndB, LoopPcm, EndPcm;
	int32 R1 = 0, C1 = 0, R2 = 0, C2 = 0;
	if (FFileHelper::LoadFileToArray(LoopB, *RepoPath(TEXT("imported/campus/bell/loop.wav"))) && UBullyCutsceneSubsystem::ParseWavBytes(LoopB, R1, C1, LoopPcm)
		&& FFileHelper::LoadFileToArray(EndB, *RepoPath(TEXT("imported/campus/bell/end.wav"))) && UBullyCutsceneSubsystem::ParseWavBytes(EndB, R2, C2, EndPcm) && R1 == R2 && C1 == C2)
	{
		const int32 Loops = FMath::Max(1, int32(Num(Obj(Cfg, TEXT("schedule")), TEXT("bell_loops"), 10.f)));
		for (int32 i = 0; i < Loops; ++i) { BellPcm.Append(LoopPcm); }
		BellPcm.Append(EndPcm);
		BellRate = R1;
		BellChannels = C1;
	}
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS places: %d grounds doors, %d hall doors, %d dorm doors, grounds volume %s, %d school volumes, %d trespass volumes, bell %s (%.1f s)"),
		GroundsDoors.Num(), HallsDoors.Num(), DormDoors.Num(), Grounds.Name.IsEmpty() ? TEXT("none") : *Grounds.Name, SchoolVolumes.Num(), TrespassVolumes.Num(),
		BellPcm.Num() ? TEXT("imported") : TEXT("missing (python3 -m tools.importers campus)"), BellRate > 0 ? float(BellPcm.Num()) / float(BellRate * BellChannels * 2) : 0.f);
}

const UBullyCampusSubsystem::FCastRow* UBullyCampusSubsystem::FindCast(const FString& Model) const
{
	return CastRows.Find(Model);
}

void UBullyCampusSubsystem::AdoptWalker(AAmbientWalker* W, const FString& Zone)
{
	if (!W) { return; }
	for (const FMember& M : Members) { if (M.Walker.Get() == W) { return; } }
	FMember M;
	M.Walker = W;
	M.Model = ModelOfMesh(W->MeshPath);
	M.Zone = Zone;
	M.HomeStart = W->GetActorLocation();
	if (const FCastRow* R = FindCast(M.Model))
	{
		M.Faction = R->Faction;
		M.Voice = R->Voice;
		M.bPrefect = R->bPrefect;
		M.Aggression = R->Aggression;
	}
	else { M.Faction = TEXT("students"); }
	W->bFleeWhenHit = !M.bPrefect && M.Aggression < 15.f;
	W->CampusRunSpeed = M.bPrefect ? Num(Obj(Cfg, TEXT("prefects")), TEXT("run_speed_cm_s"), 560.f) : 450.f;
	W->OnFirstHit.AddUObject(this, &UBullyCampusSubsystem::OnWalkerFirstHit);
	Members.Add(M);
}

void UBullyCampusSubsystem::SetupCast()
{
	UWorld* World = GetWorld();
	UAmbientSubsystem* Amb = World ? World->GetSubsystem<UAmbientSubsystem>() : nullptr;
	if (!Amb) { return; }
	// the loop walkers of Config/AmbientLoops.json are the outdoor between-classes crowd
	for (const TWeakObjectPtr<AAmbientWalker>& W : Amb->GetWalkers()) { AdoptWalker(W.Get(), TEXT("grounds")); }
	// the rest of the cast: hangout cliques, hall students, prefect patrols (Config/Campus.json "spawns")
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	int32 Spawned = 0, Skipped = 0;
	if (Cfg->TryGetArrayField(TEXT("spawns"), A))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) { continue; }
			FString Model, Id, Zone = TEXT("grounds"), Ref;
			O->TryGetStringField(TEXT("model"), Model);
			O->TryGetStringField(TEXT("id"), Id);
			O->TryGetStringField(TEXT("zone"), Zone);
			TArray<FVector> Loop;
			if (O->TryGetStringField(TEXT("path"), Ref)) { FindPath(Ref, Loop); }
			else if (O->TryGetStringField(TEXT("hangout"), Ref))
			{
				FVector P;
				if (FindPoint(Ref, P))
				{
					// a clique member shuffles on the spot around the hangout (idles long: the conversations happen here)
					const FVector Off = FVector(Num(O, TEXT("dx_cm"), 0.f), Num(O, TEXT("dy_cm"), 0.f), 0.f);
					Loop = { P + Off, P + Off + Off.GetSafeNormal2D().RotateAngleAxis(90.f, FVector::UpVector) * 40.f };
				}
			}
			if (Loop.Num() < 1) { ++Skipped; UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS spawn %s: no route (%s not imported? python3 -m tools.importers campus)"), *Id, *Ref); continue; }
			const FCastRow* R = FindCast(Model);
			const TArray<TSharedPtr<FJsonValue>>* Idle = nullptr;
			FVector2D IdleRange(O->HasField(TEXT("hangout")) ? 8.0 : 1.5, O->HasField(TEXT("hangout")) ? 18.0 : 4.0);
			if (O->TryGetArrayField(TEXT("idle_s"), Idle) && Idle->Num() == 2) { IdleRange = FVector2D((*Idle)[0]->AsNumber(), (*Idle)[1]->AsNumber()); }
			AAmbientWalker* W = Amb->SpawnWalker(Model, Id, Ref, Loop, int32(Num(O, TEXT("start_index"), 0.f)), Num(O, TEXT("speed_cm_s"), 170.f), IdleRange,
				R && R->bFemale ? TEXT("female") : TEXT("shoe"));
			if (!W) { ++Skipped; continue; }
			AdoptWalker(W, Zone);
			++Spawned;
		}
	}
	TMap<FString, int32> ByFaction;
	for (const FMember& M : Members) { ByFaction.FindOrAdd(M.Faction) += 1; }
	FString Summary;
	for (const auto& KV : ByFaction) { Summary += FString::Printf(TEXT(" %s=%d"), *KV.Key, KV.Value); }
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS cast: %d members (%d spawned, %d skipped):%s"), Members.Num(), Spawned, Skipped, *Summary);
	for (const FMember& M : Members)
	{
		UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS member %s model=%s faction=%s voice=%s zone=%s attitude=%s aggression=%.0f%s"),
			M.Walker.IsValid() ? *M.Walker->StudentId : TEXT("?"), *M.Model, *M.Faction, M.Voice.IsEmpty() ? TEXT("-") : *M.Voice, *M.Zone,
			AttitudeName(AttitudeToPlayer(M.Faction)), M.Aggression, M.bPrefect ? TEXT(" PREFECT") : TEXT(""));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// helpers

APawn* UBullyCampusSubsystem::Player() const
{
	const UWorld* W = GetWorld();
	const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	return PC ? PC->GetPawn() : nullptr;
}

bool UBullyCampusSubsystem::MissionActive() const
{
	UMissionSubsystem* MS = UMissionSubsystem::Get(GetWorld());
	return MS && (MS->IsActive() || MS->HasPendingStart());
}

float UBullyCampusSubsystem::ClockHour() const
{
	const UBullyClockSubsystem* C = UBullyClockSubsystem::Get(GetWorld());
	return C ? C->GetHourFloat() : 8.5f;
}

const UBullyCampusSubsystem::FPeriod* UBullyCampusSubsystem::CurrentPeriod() const
{
	if (ScheduleHeld()) { return Periods.Num() ? &Periods[0] : nullptr; }
	const float H = ClockHour();
	for (const FPeriod& P : Periods) { if (InWindow(H, P.Start, P.End)) { return &P; } }
	return nullptr;
}

bool UBullyCampusSubsystem::ScheduleHeld() const
{
	// the opening morning (arrival -> day_one -> 1_02B) stays before the first bell: in the original the first class
	// bell is 1_02C's (it sets 9:00 and rings F_RingSchoolBell); our boot chain does not pause the clock, so the
	// campus holds the school day at MORNING until 1_02C runs or has passed
	if (TruancyAfterMission.IsEmpty()) { return false; }
	const UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(GetWorld());
	if (!Camp || Camp->IsCompleted(TruancyAfterMission)) { return false; }
	const UMissionSubsystem* MS = UMissionSubsystem::Get(GetWorld());
	return !(MS && MS->IsActive() && MS->GetMission() && MS->GetMission()->Id == TruancyAfterMission);
}

bool UBullyCampusSubsystem::IsClassTime() const
{
	const FPeriod* P = CurrentPeriod();
	return P && P->bClass;
}

bool UBullyCampusSubsystem::PlayerOnGrounds() const
{
	const APawn* P = Player();
	if (!P) { return false; }
	const FVector L = P->GetActorLocation();
	if (PlayerInSchool()) { return true; }
	if (Grounds.Name.IsEmpty()) { return false; }
	if (L.X < Grounds.Min.X || L.Y < Grounds.Min.Y || L.X > Grounds.Max.X || L.Y > Grounds.Max.Y) { return false; }
	return !Grounds.bPoly || PointInPoly(Grounds.Poly, FVector2D(L.X, L.Y));
}

bool UBullyCampusSubsystem::PlayerInSchool() const
{
	const APawn* P = Player();
	if (!P) { return false; }
	const FVector L = P->GetActorLocation();
	for (const FVolume& V : SchoolVolumes)
	{
		if (L.X >= V.Min.X - 300.f && L.X <= V.Max.X + 300.f && L.Y >= V.Min.Y - 300.f && L.Y <= V.Max.Y + 300.f && L.Z >= V.Min.Z - 300.f && L.Z <= V.Max.Z + 300.f) { return true; }
	}
	return false;
}

bool UBullyCampusSubsystem::PlayerTrespassing(FString* Where) const
{
	const APawn* P = Player();
	if (!P) { return false; }
	const FVector L = P->GetActorLocation();
	for (const FVolume& V : TrespassVolumes)
	{
		if (L.X >= V.Min.X && L.X <= V.Max.X && L.Y >= V.Min.Y && L.Y <= V.Max.Y && L.Z >= V.Min.Z - 100.f && L.Z <= V.Max.Z + 100.f
			&& (!V.bPoly || PointInPoly(V.Poly, FVector2D(L.X, L.Y))))
		{
			if (Where) { *Where = V.Name; }
			return true;
		}
	}
	return false;
}

bool UBullyCampusSubsystem::CanSee(const AAmbientWalker* Prefect, const APawn* Target) const
{
	if (!Prefect || !Target || Prefect->IsInside() || Prefect->IsDormant() || Prefect->IsCampusSuppressed()) { return false; }
	const FVector Eye = Prefect->GetActorLocation() + FVector(0, 0, 60.f);
	const FVector To = Target->GetActorLocation() - Prefect->GetActorLocation();
	const float D = To.Size2D();
	if (D > VisionRange || FMath::Abs(To.Z) > 400.f) { return false; }
	const bool bClose = D < 300.f;   // right next to him: he notices whatever his facing
	if (!bClose && FVector::DotProduct(Prefect->GetActorForwardVector().GetSafeNormal2D(), To.GetSafeNormal2D()) < VisionHalfCos) { return false; }
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(CampusSight), false, Prefect);
	Q.AddIgnoredActor(Target);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target->GetActorLocation() + FVector(0, 0, 50.f), ECC_Visibility, Q);
}

UBullyCampusSubsystem::FMember* UBullyCampusSubsystem::MemberOf(const AAmbientWalker* W)
{
	for (FMember& M : Members) { if (M.Walker.Get() == W) { return &M; } }
	return nullptr;
}

int32 UBullyCampusSubsystem::AttitudeToPlayer(const FString& Faction) const
{
	// SFaction.lua F_MissionFactionChanges: the first completed mission in the faction's row list sets the default
	// attitude toward the player; campaign respect (SetFactionRespect values) shifts it one step at the extremes.
	const TSharedPtr<FJsonObject> Fs = Obj(Cfg, TEXT("factions"));
	const TSharedPtr<FJsonObject> F = Obj(Fs, *Faction);
	int32 A = int32(Num(F, TEXT("default"), 2.f));
	const UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(GetWorld());
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (F.IsValid() && F->TryGetArrayField(TEXT("rows"), Rows) && Camp)
	{
		for (const TSharedPtr<FJsonValue>& R : *Rows)
		{
			const TArray<TSharedPtr<FJsonValue>>& P = R->AsArray();
			if (P.Num() >= 2 && Camp->IsCompleted(P[0]->AsString())) { A = int32(P[1]->AsNumber()); break; }
		}
	}
	FString Key;
	if (F.IsValid() && F->TryGetStringField(TEXT("respect"), Key) && Camp)
	{
		const int32 R = Camp->GetRespect(FName(*Key));
		if (R >= int32(Num(Fs, TEXT("respect_high"), 80.f))) { A += 1; }
		else if (R < int32(Num(Fs, TEXT("respect_low"), 20.f))) { A -= 1; }
	}
	return FMath::Clamp(A, 0, 4);
}

// ---------------------------------------------------------------------------------------------------------------
// tick

void UBullyCampusSubsystem::Tick(float Dt)
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	const double Now = W->GetTimeSeconds();
	if (bLife && Members.Num() == 0 && (SetupTried < 0.0 || Now - SetupTried > 1.0) && Now > 0.5)
	{
		SetupTried = Now;
		SetupCast();
		if (Members.Num() == 0 && Now > 5.0) { UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS no ambient walkers in this world (-BRNoAmbient?): campus life off")); bLife = false; }
	}
	LiveAudio.RemoveAll([](const TObjectPtr<UAudioComponent>& A) { return !A || !A->IsPlaying(); });
	TickMissions();
	TickTrouble(Dt);
	if (bLife && Members.Num() > 0)
	{
		TickSchedule();
		TickPrefects(Dt);
		if (Now >= NextSlow)
		{
			NextSlow = Now + 0.25;
			TickReactions();
		}
		TickConversations();
		TickDormancy();
		TickAttend(Dt);
		if (!bRespectSeeded) { SeedRespect(); }
	}
	if (BustPhase != 0) { FinishBust(); }
	TickHud();
	if (bSmoke) { TickSmoke(Dt); }
}

void UBullyCampusSubsystem::TickMissions()
{
	const bool bActive = MissionActive();
	UMissionSubsystem* MS = UMissionSubsystem::Get(GetWorld());
	const FString Id = MS && MS->GetMission() ? MS->GetMission()->Id : FString();
	if (bActive && Id != MissionModelsFor)
	{
		// the mission's spawned models become unique: hide their ambient doubles (F_SetCharacterModelsUnique)
		MissionModelsFor = Id;
		MissionModels.Reset();
		FString Text;
		if (!Id.IsEmpty() && FFileHelper::LoadFileToString(Text, *(FPaths::ProjectConfigDir() / TEXT("Missions") / (Id + TEXT(".json")))))
		{
			// any cast model the mission names (spawn mesh, spawn_wave meshes, stand-ins) is the mission's now
			for (const FMember& M : Members)
			{
				if (!M.Model.IsEmpty() && Text.Contains(FString::Printf(TEXT("\"%s\""), *M.Model))) { MissionModels.Add(M.Model); }
			}
		}
		int32 Hidden = 0;
		for (FMember& M : Members)
		{
			if (AAmbientWalker* W = M.Walker.Get())
			{
				const bool bSupp = MissionModels.Contains(M.Model);
				W->SetCampusSuppressed(bSupp);
				Hidden += bSupp ? 1 : 0;
			}
		}
		UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS mission %s: %d unique model(s), %d ambient double(s) hidden"), *Id, MissionModels.Num(), Hidden);
	}
	if (bActive != bMissionWasActive)
	{
		bMissionWasActive = bActive;
		for (FMember& M : Members)
		{
			if (AAmbientWalker* W = M.Walker.Get())
			{
				W->SetBystander(bActive);
				if (!bActive) { W->SetCampusSuppressed(false); }
				if (bActive && W->GetCampusMode() == AAmbientWalker::ECampusMode::Chase) { W->CampusResume(); }
			}
		}
		if (bActive)
		{
			Chaser.Reset();
			if (!bMissionHold && Points > 0.f) { SetTrouble(0.f, TEXT("mission start")); }
		}
		else
		{
			MissionModelsFor.Reset();
			MissionModels.Reset();
			bMissionHold = false;
			bHidden = false;
			if (bInClass) { bAttendedThis = true; }   // a mission that ran through class time (1_02C is the first class) counts as being there
		}
		UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS mission %s: campus rules %s, %d walker(s) %s"), bActive ? TEXT("started") : TEXT("ended"),
			bActive ? TEXT("stand down") : TEXT("resume"), Members.Num(), bActive ? TEXT("now bystanders") : TEXT("back to normal"));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// trouble meter

void UBullyCampusSubsystem::MissionHeatSet(float NewPoints, float Decay)
{
	bMissionHold = NewPoints > 0.f;
	bHidden = false;
	if (Decay > 0.f) { DecayHidden = Decay; }
	SetTrouble(NewPoints, TEXT("mission heat_set"));
}

void UBullyCampusSubsystem::SetPlayerHidden(bool bOn)
{
	if (bHidden == bOn) { return; }
	bHidden = bOn;
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS player %s (trouble %.0f)"), bOn ? TEXT("hidden") : TEXT("out of hiding"), Points);
}

void UBullyCampusSubsystem::SetTrouble(float NewPoints, const TCHAR* Why)
{
	const float Old = Points;
	Points = FMath::Clamp(NewPoints, 0.f, MaxPoints);
	if (FMath::Abs(Old - Points) >= 1.f || (Points == 0.f && Old > 0.f))
	{
		UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS trouble %.0f -> %.0f (%s)"), Old, Points, Why);
	}
}

void UBullyCampusSubsystem::AddTrouble(float Delta, const TCHAR* Why)
{
	SetTrouble(Points + Delta, Why);
}

void UBullyCampusSubsystem::TickTrouble(float Dt)
{
	if (Points <= 0.f) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	if (bHidden)
	{
		Points = FMath::Max(0.f, Points - DecayHidden * Dt);
		if (Points <= 0.f) { UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS trouble drained to 0 while hidden")); bMissionHold = false; }
		return;
	}
	if (bMissionHold || MissionActive()) { return; }   // mission-forced trouble only drains from a hiding spot
	if (Chaser.IsValid() || Now - LastSeenByAuthority < 3.0) { return; }
	float Floor = 0.f;
	if (bTruant && PlayerOnGrounds()) { Floor = TruancyFloor; }
	if (bNight && PlayerOnGrounds()) { Floor = FMath::Max(Floor, TruancyFloor); }
	if (Points > Floor) { Points = FMath::Max(Floor, Points - DecayUnseen * Dt); }
}

void UBullyCampusSubsystem::OnWalkerFirstHit(AAmbientWalker* W, bool bFled)
{
	FMember* M = MemberOf(W);
	if (!M) { return; }
	const bool bMission = MissionActive();
	if (!bMission && bLife)
	{
		AddTrouble(M->bPrefect ? HitPrefectPoints : HitStudentPoints, M->bPrefect ? TEXT("hit a prefect") : TEXT("hit a student"));
		// any prefect who saw it comes running (an offence in view)
		for (FMember& P : Members)
		{
			if (P.bPrefect && P.Walker.IsValid() && !Chaser.IsValid() && CanSee(P.Walker.Get(), Player()))
			{
				Bark(P, TEXT("chase_start"), true);
				Chaser = P.Walker;
				ChaseLostSince = -1.0;
				P.Walker->CampusChase(Player());
				UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS prefect %s saw the hit: chase"), *P.Walker->StudentId);
			}
		}
	}
	Bark(*M, bFled ? TEXT("scared") : TEXT("hit"), true);
	// faction mates nearby back him up (one helper; fighters only), the rest of the crowd gathers to watch
	if (!bFled && !bMission)
	{
		int32 Helpers = 0;
		for (FMember& O : Members)
		{
			AAmbientWalker* OW = O.Walker.Get();
			if (!OW || OW == W || O.bPrefect || OW->IsInside() || OW->IsDormant()) { continue; }
			const float D = FVector::Dist(OW->GetActorLocation(), W->GetActorLocation());
			if (D > 1000.f) { continue; }
			if (O.Faction == M->Faction && O.Aggression >= 50.f && Helpers < 1 && O.Faction != TEXT("students"))
			{
				OW->JoinFightAgainstPlayer();
				Bark(O, TEXT("fight_start"), true);
				++Helpers;
				UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS %s (%s) joins the fight for %s"), *OW->StudentId, *O.Faction, *W->StudentId);
			}
			else if (FMath::FRand() < 0.5f) { Bark(O, TEXT("watch_fight")); }
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// schedule: bells, docking into class, streaming out, truancy

void UBullyCampusSubsystem::RingBell(const TCHAR* Why)
{
	++BellsRung;
	const bool bInside = PlayerInSchool();
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS bell (%s) at %02d:%02d %s"), Why, FMath::FloorToInt(ClockHour()), FMath::FloorToInt(FMath::Frac(ClockHour()) * 60.f + 0.01f),
		bInside ? TEXT("School Bell 2D (inside)") : TEXT("Half Bell 2D (outside)"));
	if (BellPcm.Num() == 0 || !GetWorld()) { return; }
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(BellRate);
	Wave->NumChannels = BellChannels;
	Wave->Duration = float(BellPcm.Num()) / float(BellRate * BellChannels * 2);
	Wave->SoundGroup = SOUNDGROUP_Effects;
	Wave->bLooping = false;
	Wave->QueueAudio(BellPcm.GetData(), BellPcm.Num());
	if (UAudioComponent* A = UGameplayStatics::CreateSound2D(GetWorld(), Wave, bInside ? 0.9f : 0.55f, 1.f, 0.f, nullptr, false, false))
	{
		A->Play();
		LiveAudio.Add(A);
	}
}

void UBullyCampusSubsystem::TickSchedule()
{
	const UBullyClockSubsystem* Clock = UBullyClockSubsystem::Get(GetWorld());
	if (!Clock) { return; }
	const int32 Key = Clock->GetDay() * 1440 + Clock->GetHour() * 60 + Clock->GetMinute();
	if (Key == LastMinuteKey) { return; }
	const bool bFirst = LastMinuteKey < 0;
	LastMinuteKey = Key;
	const float H = Clock->GetHourFloat();

	// which class (if any) is next / running
	const FPeriod* Cur = CurrentPeriod();
	const FPeriod* Next = nullptr;
	for (const FPeriod& P : Periods) { if (P.bClass && !ScheduleHeld() && InWindow(H, P.Start - HeadToClassLead, P.Start)) { Next = &P; } }
	const bool bNowClass = Cur && Cur->bClass;

	// night: students head for the dorms, the school empties (MainMap.lua hangout spawners stop ~22:30;
	// SchoolHallways.lua clears the halls at 18:45); curfew 23:00-07:00 (STimeCycle.lua F_CurfewDefaultRules)
	const bool bDorm = InWindow(H, DormStart, CurfewEnd);
	const bool bHallsShut = InWindow(H, SchoolClosedStart, CurfewEnd);
	bNight = InWindow(H, CurfewStart, CurfewEnd);

	if (bFirst)
	{
		// world just started mid-period: put everybody where the schedule says without a parade
		bInClass = bNowClass;
		ClassId = bNowClass ? Cur->Id : FString();
		bAttendedThis = bNowClass;   // no truancy for the class the game (or a load) dropped the player into
		for (FMember& M : Members)
		{
			AAmbientWalker* W = M.Walker.Get();
			if (!W || M.bPrefect) { continue; }
			const bool bOut = !(bNowClass || (M.Zone == TEXT("grounds") ? bDorm : bHallsShut));
			M.bScheduledOut = bOut;
			if (!bOut) { W->CampusEnter(); }
		}
		UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS schedule start %02d:%02d period=%s"), Clock->GetHour(), Clock->GetMinute(), Cur ? *Cur->Id : TEXT("?"));
		return;
	}

	// head to class (dockers 8:45 / 12:45, 80 % run once late)
	if (Next && !bDocking && !bInClass)
	{
		bDocking = true;
		UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS %02d:%02d heading to class (%s at %.2f)"), Clock->GetHour(), Clock->GetMinute(), *Next->Id, Next->Start);
		BeginClassDocking(false);
		for (FMember& M : Members)
		{
			if (M.bPrefect && M.Walker.IsValid() && Player() && PlayerOnGrounds() && !MissionActive() && FVector::Dist(M.Walker->GetActorLocation(), Player()->GetActorLocation()) < 1500.f)
			{
				Bark(M, TEXT("warn_go_to_class"));
				break;
			}
		}
	}
	if (bNowClass && !bInClass) { StartClass(); }
	if (bInClass && !bNowClass) { EndClass(bAttendedThis); }
	if (bInClass && !bAttendedThis && !bTruant && Cur && InWindow(H, Cur->Start + LateAfter, Cur->End) && !MissionActive())
	{
		const UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(GetWorld());
		if ((TruancyAfterMission.IsEmpty() || (Camp && Camp->IsCompleted(TruancyAfterMission))) && PlayerOnGrounds())
		{
			bTruant = true;
			if (Points < TruancyFloor) { SetTrouble(TruancyFloor, TEXT("truancy: late for class")); }
			if (UMissionSubsystem* MS = UMissionSubsystem::Get(GetWorld()))
			{
				if (FBullyHUD* Hud = MS->AcquireHud()) { Hud->ShowBanner(TEXT("LATE FOR CLASS"), TEXT("Class has started. Prefects will bust truants on sight."), 4.f, true); }
			}
			UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS truancy: late for %s (trouble %.0f)"), *ClassId, Points);
		}
	}
	if (bTruant && !bInClass) { bTruant = false; }

	// evening / morning movements of the cast (dorms, the school building)
	for (FMember& M : Members)
	{
		AAmbientWalker* W = M.Walker.Get();
		if (!W || M.bPrefect || W->IsBrawling()) { continue; }
		const bool bShut = M.Zone == TEXT("grounds") ? bDorm : bHallsShut;
		if (bShut && M.bScheduledOut && !bInClass && !bDocking)
		{
			M.bScheduledOut = false;
			const TArray<FVector>& Doors = M.Zone == TEXT("grounds") ? (DormDoors.Num() ? DormDoors : GroundsDoors) : HallsDoors;
			if (Doors.Num() == 0) { W->CampusEnter(); continue; }
			const FVector P = W->GetActorLocation();
			FVector Best = Doors[0];
			for (const FVector& D : Doors) { if (FVector::DistSquared(D, P) < FVector::DistSquared(Best, P)) { Best = D; } }
			W->CampusGoTo(Best, false, true);
		}
		else if (!bShut && !M.bScheduledOut && !bInClass && !bDocking && W->IsInside())
		{
			M.bScheduledOut = true;
			const TArray<FVector>& Doors = M.Zone == TEXT("grounds") ? (DormDoors.Num() ? DormDoors : GroundsDoors) : HallsDoors;
			W->CampusEmerge(Doors.Num() ? Doors[FMath::RandRange(0, Doors.Num() - 1)] : M.HomeStart);
		}
	}
	// staggered stream-out after class: a few students per game minute (spawners 11:30 / 15:30 for 30 minutes)
	if (!bInClass && !bDocking && !(bDorm || bNight)) { StreamOut(); }
}

void UBullyCampusSubsystem::BeginClassDocking(bool bRun)
{
	for (FMember& M : Members)
	{
		AAmbientWalker* W = M.Walker.Get();
		if (!W || M.bPrefect || W->IsInside() || W->IsBrawling()) { continue; }
		const TArray<FVector>& Doors = M.Zone == TEXT("grounds") ? GroundsDoors : HallsDoors;
		M.bScheduledOut = false;
		if (Doors.Num() == 0 || W->IsDormant()) { W->CampusEnter(); continue; }
		const FVector P = W->GetActorLocation();
		FVector Best = Doors[0];
		for (const FVector& D : Doors)
		{
			// same floor first (hall doors exist on both storeys)
			const float Dz = FMath::Abs(D.Z - P.Z) > 250.f ? 1e8f : 0.f;
			if (FVector::DistSquared(D, P) + Dz < FVector::DistSquared(Best, P) + (FMath::Abs(Best.Z - P.Z) > 250.f ? 1e8f : 0.f)) { Best = D; }
		}
		W->CampusGoTo(Best, bRun || FMath::FRand() < 0.2f, true);
	}
}

void UBullyCampusSubsystem::StartClass()
{
	const FPeriod* Cur = CurrentPeriod();
	bInClass = true;
	bDocking = false;
	bAttendedThis = false;
	bLateShown = false;
	ClassId = Cur ? Cur->Id : TEXT("CLASS");
	// F_StartClass: the bell, and the skipping punishment from the classes skipped so far
	RingBell(TEXT("class starts"));
	int32 Skipped = 0;
	for (bool b : SkippedClasses) { Skipped += b ? 1 : 0; }
	int32 Extra = 0;
	for (const FIntPoint& T : SkippingThresholds) { if (Skipped >= T.X) { Extra = T.Y; } }
	if (Extra > 0 && !MissionActive()) { AddTrouble(float(Extra), TEXT("skipping punishment (GetSkippingPunishment)")); }
	// stragglers run (80 % run percentage after the bell)
	for (FMember& M : Members)
	{
		AAmbientWalker* W = M.Walker.Get();
		if (!W || M.bPrefect || W->IsInside() || W->IsBrawling()) { continue; }
		if (W->GetCampusMode() == AAmbientWalker::ECampusMode::Errand) { W->CampusGoTo(W->GetErrandDest(), FMath::FRand() < 0.8f, true); }
		else { M.bScheduledOut = true; }
	}
	BeginClassDocking(true);
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS class %s started (skipped so far %d, skipping punishment %d)"), *ClassId, Skipped, Extra);
}

void UBullyCampusSubsystem::EndClass(bool bAttended)
{
	// F_EndClass rings the bell; F_AttendedClass / F_MissedClass update the skipped-class window
	bInClass = false;
	bTruant = false;
	RingBell(TEXT("class ends"));
	if (SkippedClasses.Num() > 0) { SkippedClasses.RemoveAt(0); }
	SkippedClasses.Add(!bAttended);
	if (bAttended) { if (!bMissionHold) { SetTrouble(0.f, TEXT("attended class (F_AttendedClass)")); } }
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS class %s ended: %s"), *ClassId, bAttended ? TEXT("attended") : TEXT("SKIPPED (F_MissedClass)"));
	ClassId.Reset();
	for (FMember& M : Members) { if (!M.bPrefect) { M.bScheduledOut = true; } }
}

void UBullyCampusSubsystem::StreamOut()
{
	// the docked students come back out a few at a time through the doors they went in by
	int32 Budget = FMath::Max(1, int32(FMath::CeilToInt(float(Members.Num()) / FMath::Max(1.f, StreamOutMinutes * 0.25f))));
	for (FMember& M : Members)
	{
		AAmbientWalker* W = M.Walker.Get();
		if (!W || M.bPrefect || !W->IsInside() || !M.bScheduledOut || W->IsCampusSuppressed()) { continue; }
		const TArray<FVector>& Doors = M.Zone == TEXT("grounds") ? GroundsDoors : HallsDoors;
		FVector At = M.HomeStart;
		if (Doors.Num())
		{
			At = Doors[0];
			for (const FVector& D : Doors) { if (FVector::DistSquared(D, M.HomeStart) < FVector::DistSquared(At, M.HomeStart)) { At = D; } }
		}
		W->CampusEmerge(At);
		if (--Budget <= 0) { break; }
	}
}

void UBullyCampusSubsystem::AttendClass(const TCHAR* Why)
{
	// v1 class = a fade, the clock to the end of the period, then out into the hall (C_* class minigames are sm-minigames')
	const FPeriod* Cur = CurrentPeriod();
	if (!Cur || !Cur->bClass)
	{
		Cur = nullptr;
		const float H = ClockHour();
		for (const FPeriod& P : Periods) { if (P.bClass && InWindow(H, P.Start - HeadToClassLead, P.Start)) { Cur = &P; } }
		if (!Cur) { return; }
		if (!bInClass) { StartClass(); }   // walked in just before the bell: the class starts with him in it
	}
	bAttendedThis = true;
	bTruant = false;
	SetTrouble(0.f, TEXT("in class"));
	if (UBullyClockSubsystem* Clock = UBullyClockSubsystem::Get(GetWorld()))
	{
		const float End = Cur->End;
		Clock->SetClock(FMath::FloorToInt(End), FMath::RoundToInt(FMath::Frac(End) * 60.f));
	}
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS attended %s (%s): clock -> end of class"), *Cur->Id, Why);
}

void UBullyCampusSubsystem::TickAttend(float Dt)
{
	// a classroom door during class time (or the docking quarter-hour before it): Jimmy goes in and attends
	APawn* P = Player();
	if (!P || BustPhase != 0 || MissionActive() || bAttendedThis || HallsDoors.Num() == 0) { AttendDwell = 0.f; return; }
	const float H = ClockHour();
	bool bWindow = IsClassTime();
	for (const FPeriod& Pd : Periods) { if (Pd.bClass && !ScheduleHeld() && InWindow(H, Pd.Start - HeadToClassLead, Pd.Start)) { bWindow = true; } }
	if (!bWindow || !PlayerInSchool()) { AttendDwell = 0.f; return; }
	const FVector L = P->GetActorLocation() - FVector(0, 0, P->GetSimpleCollisionHalfHeight());
	bool bAtDoor = false;
	for (const FVector& D : HallsDoors) { if (FVector::Dist2D(D, L) < 160.f && FMath::Abs(D.Z - L.Z) < 200.f) { bAtDoor = true; break; } }
	AttendDwell = bAtDoor ? AttendDwell + Dt : 0.f;
	if (AttendDwell < 1.0f) { return; }
	AttendDwell = 0.f;
	BustOutcome = TEXT("attend");
	if (APlayerController* PC = Cast<APlayerController>(P->GetController())) { PC->SetIgnoreMoveInput(true); }
	BustPhase = 1;
	BustPhaseAt = GetWorld()->GetTimeSeconds();
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS Jimmy walks into class at %s"), *L.ToString());
}

void UBullyCampusSubsystem::SeedRespect()
{
	// 1_01.lua sets the starting respect (bullies 25, nerds / preps / greasers / jocks 50, dropouts 0) during the
	// opening; our day_one covers 1_01 but the campaign table starts every faction at 0. Seed once, on a fresh game only.
	UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(GetWorld());
	const TSharedPtr<FJsonObject> Start = Obj(Obj(Cfg, TEXT("factions")), TEXT("start_respect"));
	if (!Camp || !Start.IsValid()) { bRespectSeeded = true; return; }
	if (!Camp->IsCompleted(TEXT("day_one"))) { return; }
	bRespectSeeded = true;
	bool bAllZero = true;
	for (const auto& KV : Start->Values) { bAllZero &= Camp->GetRespect(FName(*FString(KV.Key))) == 0; }
	if (!bAllZero) { return; }
	for (const auto& KV : Start->Values) { Camp->SetRespect(FName(*FString(KV.Key)), int32(KV.Value->AsNumber())); }
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS starting faction respect seeded from 1_01.lua (%d factions)"), Start->Values.Num());
}

// ---------------------------------------------------------------------------------------------------------------
// prefects

void UBullyCampusSubsystem::TickPrefects(float Dt)
{
	APawn* P = Player();
	if (!P || BustPhase != 0) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bMission = MissionActive();
	FString Where;
	const bool bTrespass = !bMission && PlayerTrespassing(&Where);
	if (bTrespass) { AddTrouble(TrespassPerSecond * Dt, TEXT("trespassing")); }
	const bool bCurfewOut = !bMission && bNight && PlayerOnGrounds();
	if (bCurfewOut && Points < TruancyFloor) { SetTrouble(TruancyFloor, TEXT("out after curfew")); }

	if (AAmbientWalker* C = Chaser.Get())
	{
		if (bMission) { C->CampusResume(); Chaser.Reset(); return; }
		const float D = FVector::Dist2D(C->GetActorLocation(), P->GetActorLocation());
		const bool bSee = CanSee(C, P) || D < 400.f;
		if (bSee) { ChaseLostSince = -1.0; LastSeenByAuthority = Now; } else if (ChaseLostSince < 0.0) { ChaseLostSince = Now; }
		if (D <= BustRadius && FMath::Abs(C->GetActorLocation().Z - P->GetActorLocation().Z) < 150.f) { Bust(C); return; }
		if (Now >= NextChaseBark) { if (FMember* M = MemberOf(C)) { Bark(*M, TEXT("chase")); } NextChaseBark = Now + 5.0; }
		if ((ChaseLostSince >= 0.0 && Now - ChaseLostSince > GiveUpSeconds) || D > GiveUpRange)
		{
			if (FMember* M = MemberOf(C)) { Bark(*M, TEXT("chase_lost"), true); }
			UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS prefect %s lost Jimmy (%.0f m) - chase over"), *C->StudentId, D / 100.f);
			C->CampusResume();
			Chaser.Reset();
		}
		return;
	}
	if (bMission) { return; }
	for (FMember& M : Members)
	{
		AAmbientWalker* W = M.Walker.Get();
		if (!M.bPrefect || !W || W->IsBrawling() || !CanSee(W, P)) { continue; }
		LastSeenByAuthority = Now;
		if (Now < M.NextNotice) { continue; }
		M.NextNotice = Now + 8.0;
		const bool bOffence = bTruant || bTrespass || bCurfewOut || Points >= ChaseThreshold;
		if (bOffence)
		{
			Bark(M, bTruant ? TEXT("warn_truancy") : bTrespass ? TEXT("warn_trespass") : bCurfewOut ? TEXT("warn_curfew") : TEXT("chase_start"), true);
			Chaser = W;
			ChaseLostSince = -1.0;
			NextChaseBark = Now + 3.0;
			W->CampusChase(P);
			UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS prefect %s spotted Jimmy (%s, trouble %.0f): chase"), *W->StudentId,
				bTruant ? TEXT("truant") : bTrespass ? *Where : bCurfewOut ? TEXT("curfew") : TEXT("trouble"), Points);
			break;
		}
		if (Points > 0.f && FVector::Dist(W->GetActorLocation(), P->GetActorLocation()) < 800.f)
		{
			Bark(M, TEXT("warn_minor"));   // trouble on the meter but under the chase line: a warning
			UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS prefect %s warns Jimmy (trouble %.0f < %.0f)"), *W->StudentId, Points, ChaseThreshold);
		}
	}
}

void UBullyCampusSubsystem::Bust(AAmbientWalker* By)
{
	++BustCount;
	if (FMember* M = MemberOf(By)) { Bark(*M, TEXT("bust"), true); }
	if (By) { By->CampusResume(); }
	Chaser.Reset();
	// main.lua F_PlayerBusted: on school grounds after 1_02C -> class time: sent to class; else 7-19h and (first bust or
	// more than 2 busts since the last visit): the principal's office (PriOff); otherwise no scene. Before 1_02C the
	// bust only resets (the mission set-ups place the arrest point).
	const UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(GetWorld());
	const bool bAfter102C = !Camp || Camp->IsCompleted(TEXT("1_02C"));
	const float H = ClockHour();
	if (bAfter102C && IsClassTime()) { BustOutcome = TEXT("class"); }
	else if (bAfter102C && H >= 7.f && H < 19.f && (BustCount == 1 || BustCount - BustsAtLastPrincipal > 2)) { BustOutcome = TEXT("principal"); BustsAtLastPrincipal = BustCount; }
	else { BustOutcome = TEXT("released"); }
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS BUSTED by %s (bust #%d, trouble %.0f, %02d:%02d) -> %s"), By ? *By->StudentId : TEXT("?"), BustCount, Points,
		FMath::FloorToInt(H), FMath::FloorToInt(FMath::Frac(H) * 60.f + 0.01f), *BustOutcome);
	if (UMissionSubsystem* MS = UMissionSubsystem::Get(GetWorld()))
	{
		if (FBullyHUD* Hud = MS->AcquireHud())
		{
			Hud->ShowBanner(TEXT("BUSTED"), BustOutcome == TEXT("class") ? TEXT("Off to class.") : BustOutcome == TEXT("principal") ? TEXT("The principal wants a word.") : TEXT("Behave."), 3.f, true);
		}
	}
	if (APawn* P = Player()) { if (APlayerController* PC = Cast<APlayerController>(P->GetController())) { PC->SetIgnoreMoveInput(true); } }
	SetTrouble(0.f, TEXT("busted"));
	bTruant = false;
	BustPhase = 1;
	BustPhaseAt = GetWorld()->GetTimeSeconds();
}

void UBullyCampusSubsystem::SetFade(float Alpha)
{
	APawn* P = Player();
	APlayerController* PC = P ? Cast<APlayerController>(P->GetController()) : nullptr;
	if (!PC || !PC->PlayerCameraManager) { return; }
	if (Alpha <= 0.f) { PC->PlayerCameraManager->StopCameraFade(); }
	else { PC->PlayerCameraManager->SetManualCameraFade(FMath::Min(Alpha, 1.f), FLinearColor::Black, false); }
}

void UBullyCampusSubsystem::PlacePlayer(const FVector& At, float Yaw)
{
	APawn* P = Player();
	if (!P || At.IsZero()) { return; }
	FVector Spot = At;
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(CampusPlace), true, P);
	if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 150.f), At - FVector(0, 0, 300.f), ECC_Visibility, Q)) { Spot = Hit.ImpactPoint; }
	const float Half = P->GetSimpleCollisionHalfHeight();
	P->SetActorLocation(Spot + FVector(0, 0, Half + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
	P->SetActorRotation(FRotator(0.f, Yaw, 0.f));
	if (AController* C = P->GetController()) { C->SetControlRotation(FRotator(0.f, Yaw, 0.f)); }
	if (ACharacter* Ch = Cast<ACharacter>(P)) { Ch->GetCharacterMovement()->StopMovementImmediately(); }
	if (ABullyCharacter* B = Cast<ABullyCharacter>(P)) { B->SnapCamera(); }
}

void UBullyCampusSubsystem::FinishBust()
{
	const double Now = GetWorld()->GetTimeSeconds();
	const double T = Now - BustPhaseAt;
	FBullyHUD* Hud = nullptr;
	if (UMissionSubsystem* MS = UMissionSubsystem::Get(GetWorld())) { Hud = MS->AcquireHud(); }
	if (BustPhase == 1)
	{
		SetFade(float(T / 1.0));
		if (T < 1.2) { return; }
		if (BustOutcome == TEXT("principal") && !PrincipalSpawn.IsZero())
		{
			PlacePlayer(PrincipalSpawn, 0.f);
			// PriOff.lua F_NISCore: first visit lecture ids 10, 51, 52; later visits 1, 2, one bad deed (9 = busted by
			// prefects), 50, 52
			const TSharedPtr<FJsonObject> L = Obj(Obj(Cfg, TEXT("speech")), TEXT("lecture"));
			const TArray<TSharedPtr<FJsonValue>>* Ids = nullptr;
			LectureQueue.Reset();
			if (L.IsValid() && L->TryGetArrayField(BustsAtLastPrincipal <= 1 && BustCount == 1 ? TEXT("first") : TEXT("repeat"), Ids))
			{
				for (const TSharedPtr<FJsonValue>& V : *Ids)
				{
					const FString Key = FString::Printf(TEXT("%03d"), int32(V->AsNumber()));
					if (const TArray<FTake>* Tk = Lecture.Find(Key)) { if (Tk->Num()) { LectureQueue.Add((*Tk)[FMath::RandRange(0, Tk->Num() - 1)]); } }
				}
			}
			UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS principal's office: %d lecture line(s)"), LectureQueue.Num());
			BustReturnAt = PrincipalOutside;
		}
		else if (BustOutcome == TEXT("class") || BustOutcome == TEXT("attend"))
		{
			const bool bSent = BustOutcome == TEXT("class");
			AttendClass(bSent ? TEXT("sent to class by a prefect") : TEXT("walked into class"));
			BustReturnAt = bSent ? (!ClassExitPoint.IsZero() ? ClassExitPoint : SchoolFront) : (Player() ? Player()->GetActorLocation() - FVector(0, 0, Player()->GetSimpleCollisionHalfHeight()) : FVector::ZeroVector);
			if (Hud) { Hud->ShowBanner(TEXT("CLASS"), TEXT("You sat through the rest of the lesson."), 3.f, false); }
		}
		else { BustReturnAt = !SchoolFront.IsZero() ? SchoolFront : FVector::ZeroVector; }
		BustPhase = 2;
		BustPhaseAt = Now;
		LectureNextAt = Now + 0.6;
		SetFade(LectureQueue.Num() ? 0.f : 1.f);
		return;
	}
	if (BustPhase == 2)
	{
		if (LectureQueue.Num() > 0)
		{
			if (Now >= LectureNextAt)
			{
				const FTake Tk = LectureQueue[0];
				LectureQueue.RemoveAt(0);
				const float Len = PlayTake(Tk, Player(), 1.f, TEXT("principal lecture"));
				if (Hud) { Hud->Say(TEXT("Dr. Crabblesnitch"), Tk.Text.IsEmpty() ? TEXT("...") : Tk.Text, FMath::Max(1.5f, Len)); }
				LectureNextAt = Now + FMath::Max(1.5f, Len) + 0.4f;
			}
			return;
		}
		if (Now < LectureNextAt || T < 1.0) { return; }
		SetFade(1.f);
		if (!BustReturnAt.IsZero()) { PlacePlayer(BustReturnAt, 90.f); }
		BustPhase = 3;
		BustPhaseAt = Now;
		return;
	}
	if (BustPhase == 3)
	{
		SetFade(1.f - float(T / 0.8));
		if (T < 0.8) { return; }
		SetFade(0.f);
		if (APawn* P = Player()) { if (APlayerController* PC = Cast<APlayerController>(P->GetController())) { PC->SetIgnoreMoveInput(false); } }
		UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS bust done (%s), player at %s"), *BustOutcome, Player() ? *Player()->GetActorLocation().ToString() : TEXT("?"));
		BustPhase = 0;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// reactions: greetings / taunts / bumps, fights, picked fights

void UBullyCampusSubsystem::TickReactions()
{
	APawn* P = Player();
	if (!P || BustPhase != 0) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bMission = MissionActive();
	const FVector PL = P->GetActorLocation();
	const FVector PV = P->GetVelocity();
	for (FMember& M : Members)
	{
		AAmbientWalker* W = M.Walker.Get();
		if (!W || W->IsInside() || W->IsDormant() || W->IsCampusSuppressed() || W->bChatHold) { continue; }
		// fight state edges: the KO'd / beaten line
		if (UBullyCombatComponent* C = W->GetCombat())
		{
			const bool bDown = C->IsDown();
			if (bDown && !M.bWasDown) { Bark(M, TEXT("beaten"), true); }
			M.bWasDown = bDown;
		}
		if (W->IsBrawling()) { continue; }
		const FVector To = PL - W->GetActorLocation();
		const float D = To.Size2D();
		if (D > 700.f || FMath::Abs(To.Z) > 250.f) { continue; }
		const int32 Att = M.bPrefect ? 1 : AttitudeToPlayer(M.Faction);
		// a bump: Jimmy runs into him
		if (D < 85.f && PV.Size2D() > 150.f && Now >= M.NextBark)
		{
			Bark(M, Att >= 3 ? TEXT("bump_sorry") : TEXT("bump_rude"));
			W->CampusHold(1.2f, P);
			continue;
		}
		if (Now < M.NextNotice) { continue; }
		const bool bFacing = FVector::DotProduct(W->GetActorForwardVector().GetSafeNormal2D(), To.GetSafeNormal2D()) > 0.3f;
		if (D < 450.f && bFacing)
		{
			M.NextNotice = Now + FMath::FRandRange(25.f, 45.f);
			if (M.bPrefect) { continue; }   // prefects: TickPrefects
			if (Att >= 3) { Bark(M, TEXT("greet")); W->CampusHold(1.5f, P); }
			else if (Att <= 1 && FMath::FRand() < 0.6f) { Bark(M, TEXT("taunt")); W->CampusHold(1.5f, P); }
			else if (Att == 2 && FMath::FRand() < 0.25f) { Bark(M, TEXT("chatter")); }
			// Abhor + a fighter: he picks a fight (never during missions; at most one a minute)
			if (Att == 0 && !bMission && M.Aggression >= 50.f && D < 300.f && Now >= NextFightPick)
			{
				NextFightPick = Now + 60.0;
				Bark(M, TEXT("fight_start"), true);
				W->JoinFightAgainstPlayer();
				UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS %s (%s, Abhor) picks a fight with Jimmy"), *W->StudentId, *M.Faction);
			}
		}
	}
}

void UBullyCampusSubsystem::TickConversations()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextConversation || ConversationPairs.Num() == 0) { return; }
	NextConversation = Now + FMath::FRandRange(4.f, 9.f);
	const APawn* P = Player();
	if (!P) { return; }
	// two idle students close together within earshot of Jimmy: opener + reply (+ a parting line)
	for (int32 i = 0; i < Members.Num(); ++i)
	{
		FMember& A = Members[i];
		AAmbientWalker* WA = A.Walker.Get();
		if (!WA || A.bPrefect || !WA->IsIdling() || WA->IsDormant() || WA->IsInside() || WA->IsCampusSuppressed() || Now < A.NextBark) { continue; }
		if (FVector::Dist(WA->GetActorLocation(), P->GetActorLocation()) > 2500.f) { continue; }
		for (int32 j = 0; j < Members.Num(); ++j)
		{
			FMember& B = Members[j];
			AAmbientWalker* WB = B.Walker.Get();
			if (i == j || !WB || B.bPrefect || WB->IsInside() || WB->IsDormant() || WB->IsCampusSuppressed() || WB->IsBrawling() || Now < B.NextBark) { continue; }
			if (FVector::Dist(WA->GetActorLocation(), WB->GetActorLocation()) > 400.f) { continue; }
			const TPair<FString, FString>& Pair = ConversationPairs[FMath::RandRange(0, ConversationPairs.Num() - 1)];
			const TArray<FTake>* Open = Takes(A.Voice, Pair.Key);
			const TArray<FTake>* Reply = Takes(B.Voice, Pair.Value);
			if (!Open || !Reply || Open->Num() == 0 || Reply->Num() == 0) { continue; }
			const FTake& TA = (*Open)[FMath::RandRange(0, Open->Num() - 1)];
			const FTake& TB = (*Reply)[FMath::RandRange(0, Reply->Num() - 1)];
			const float Total = TA.Seconds + 0.35f + TB.Seconds + 0.5f;
			WA->CampusHold(Total, WB);
			WB->CampusHold(Total, WA);
			PlayTake(TA, WA, 0.9f, FString::Printf(TEXT("conversation %s %s_%s"), *WA->StudentId, *A.Voice, *Pair.Key));
			// the reply starts when the opener ends (timer on the world, keeps the take by value)
			FTimerHandle H;
			TWeakObjectPtr<AAmbientWalker> WeakB = WB;
			const FTake Copy = TB;
			const FString Label = FString::Printf(TEXT("conversation %s %s_%s (reply)"), *WB->StudentId, *B.Voice, *Pair.Value);
			GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, WeakB, Copy, Label]()
			{
				if (WeakB.IsValid() && !WeakB->IsInside()) { PlayTake(Copy, WeakB.Get(), 0.9f, Label); }
			}), TA.Seconds + 0.35f, false);
			A.NextBark = B.NextBark = Now + Total + 20.0;
			A.NextNotice = B.NextNotice = Now + Total + 2.0;
			++ConversationsPlayed;
			UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS conversation %s (%s) -> %s (%s): %s / %s, %.1f s"), *WA->StudentId, *A.Faction, *WB->StudentId, *B.Faction, *Pair.Key, *Pair.Value, Total);
			return;
		}
	}
}

void UBullyCampusSubsystem::TickDormancy()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextDormancy) { return; }
	NextDormancy = Now + 1.0;
	const APawn* P = Player();
	if (!P) { return; }
	const FVector PL = P->GetActorLocation();
	const float Sleep = Num(Obj(Cfg, TEXT("performance")), TEXT("dormant_beyond_m"), 110.f) * 100.f;
	const float Wake = Num(Obj(Cfg, TEXT("performance")), TEXT("wake_within_m"), 95.f) * 100.f;
	for (FMember& M : Members)
	{
		AAmbientWalker* W = M.Walker.Get();
		if (!W || W->IsBrawling() || W == Chaser.Get()) { continue; }
		const float D = FVector::Dist(W->GetActorLocation(), PL);
		if (!W->IsDormant() && D > Sleep) { W->SetDormant(true); }
		else if (W->IsDormant() && D < Wake) { W->SetDormant(false); }
	}
}

// ---------------------------------------------------------------------------------------------------------------
// HUD: the trouble meter + the clock / period line

void UBullyCampusSubsystem::TickHud()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextHud) { return; }
	NextHud = Now + 0.2;
	UMissionSubsystem* MS = UMissionSubsystem::Get(GetWorld());
	FBullyHUD* Hud = MS ? MS->GetHUD() : nullptr;
	if (!Hud && (Points > 0.f || bLife) && MS && Player()) { Hud = MS->AcquireHud(); }
	if (!Hud) { return; }
	FString Line;
	if (bLife)
	{
		const UBullyClockSubsystem* C = UBullyClockSubsystem::Get(GetWorld());
		const FPeriod* Pd = CurrentPeriod();
		if (C)
		{
			const int32 H12 = (C->GetHour() + 11) % 12 + 1;
			Line = FString::Printf(TEXT("%d:%02d %s"), H12, C->GetMinute(), C->GetHour() < 12 ? TEXT("AM") : TEXT("PM"));
			if (Pd) { Line += FString::Printf(TEXT("  ·  %s"), Pd->bClass ? (bAttendedThis ? TEXT("CLASS (attended)") : TEXT("CLASS")) : *Pd->Id.Replace(TEXT("_"), TEXT(" "))); }
		}
	}
	Hud->SetCampus(Line, Points > 0.f ? Points / MaxPoints : -1.f, Chaser.IsValid(), ChaseThreshold / MaxPoints);
}

// ---------------------------------------------------------------------------------------------------------------
// speech

void UBullyCampusSubsystem::LoadSpeech()
{
	const FString Root = RepoPath(TEXT("imported/campus/speech"));
	const TSharedPtr<FJsonObject> Man = ReadJsonFile(Root / TEXT("manifest.json"));
	if (!Man.IsValid()) { UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS no ambient speech imported (python3 -m tools.importers campus): barks are logged only")); return; }
	auto ReadTakes = [&Root](const TArray<TSharedPtr<FJsonValue>>& A, TArray<FTake>& Out)
	{
		for (const TSharedPtr<FJsonValue>& V : A)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FTake T;
			if (O.IsValid() && O->TryGetStringField(TEXT("wav"), T.Wav))
			{
				T.Wav = Root / T.Wav;
				T.Seconds = Num(O, TEXT("seconds"), 1.5f);
				O->TryGetStringField(TEXT("text"), T.Text);
				Out.Add(T);
			}
		}
	};
	int32 N = 0;
	if (const TSharedPtr<FJsonObject> Sp = Obj(Man, TEXT("speakers")))
	{
		for (const auto& S : Sp->Values)
		{
			const TSharedPtr<FJsonObject> Codes = S.Value->AsObject();
			if (!Codes.IsValid()) { continue; }
			for (const auto& C : Codes->Values)
			{
				TArray<FTake>& T = Speech.FindOrAdd(FString(S.Key)).FindOrAdd(FString(C.Key));
				ReadTakes(C.Value->AsArray(), T);
				N += T.Num();
			}
		}
	}
	if (const TSharedPtr<FJsonObject> L = Obj(Man, TEXT("lecture")))
	{
		for (const auto& KV : L->Values) { ReadTakes(KV.Value->AsArray(), Lecture.FindOrAdd(FString(KV.Key))); }
	}
	Attenuation = NewObject<USoundAttenuation>(this);
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(500.f, 0.f, 0.f);
	Attenuation->Attenuation.FalloffDistance = 2200.f;
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS ambient speech: %d voices, %d takes, %d lecture lines"), Speech.Num(), N, Lecture.Num());
}

const TArray<UBullyCampusSubsystem::FTake>* UBullyCampusSubsystem::Takes(const FString& Voice, const FString& Code) const
{
	const TMap<FString, TArray<FTake>>* V = Speech.Find(Voice);
	return V ? V->Find(Code) : nullptr;
}

TArray<FString> UBullyCampusSubsystem::CodesFor(const TCHAR* Context) const
{
	const TArray<FString>* C = BarkCodes.Find(Context);
	return C ? *C : TArray<FString>();
}

float UBullyCampusSubsystem::Bark(FMember& M, const TCHAR* Context, bool bForce)
{
	AAmbientWalker* W = M.Walker.Get();
	UWorld* World = GetWorld();
	if (!W || !World) { return 0.f; }
	const double Now = World->GetTimeSeconds();
	if (!bForce && (Now < M.NextBark || Now - LastBarkAt < 0.6 || LiveAudio.Num() >= 3)) { return 0.f; }
	const TArray<FString> Codes = CodesFor(Context);
	TArray<const FTake*> Pool;
	FString Picked;
	for (const FString& C : Codes)
	{
		if (const TArray<FTake>* T = Takes(M.Voice, C)) { for (const FTake& Tk : *T) { Pool.Add(&Tk); } if (T->Num() && Picked.IsEmpty()) { Picked = C; } }
	}
	M.NextBark = Now + FMath::FRandRange(6.f, 10.f);
	LastBarkAt = Now;
	++BarksPlayed;
	if (Pool.Num() == 0)
	{
		UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS bark %s (%s) %s: no take for voice %s [%s] (logged only)"), *W->StudentId, *M.Faction, Context,
			M.Voice.IsEmpty() ? TEXT("-") : *M.Voice, *FString::Join(Codes, TEXT(",")));
		return 0.f;
	}
	const FTake& T = *Pool[FMath::RandRange(0, Pool.Num() - 1)];
	return PlayTake(T, W, 1.f, FString::Printf(TEXT("bark %s (%s) %s %s"), *W->StudentId, *M.Faction, Context, *FPaths::GetBaseFilename(T.Wav)));
}

float UBullyCampusSubsystem::PlayTake(const FTake& T, AActor* At, float Volume, const FString& Label)
{
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS %s %.2fs"), *Label, T.Seconds);
	TArray<uint8> Bytes, Pcm;
	int32 Rate = 0, Ch = 0;
	if (!FFileHelper::LoadFileToArray(Bytes, *T.Wav) || !UBullyCutsceneSubsystem::ParseWavBytes(Bytes, Rate, Ch, Pcm) || Rate <= 0) { return T.Seconds; }
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(Rate);
	Wave->NumChannels = Ch;
	Wave->Duration = float(Pcm.Num()) / float(Rate * Ch * 2);
	Wave->SoundGroup = SOUNDGROUP_Voice;
	Wave->bLooping = false;
	Wave->QueueAudio(Pcm.GetData(), Pcm.Num());
	UAudioComponent* A = nullptr;
	if (At && At != Player()) { A = UGameplayStatics::SpawnSoundAttached(Wave, At->GetRootComponent(), NAME_None, FVector(0, 0, 60.f), EAttachLocation::KeepRelativeOffset, true, Volume, 1.f, 0.f, Attenuation); }
	else { A = UGameplayStatics::CreateSound2D(GetWorld(), Wave, Volume, 1.f, 0.f, nullptr, false, false); if (A) { A->Play(); } }
	if (A) { LiveAudio.Add(A); }
	return Wave->Duration;
}

// ---------------------------------------------------------------------------------------------------------------
// status / console

void UBullyCampusSubsystem::LogStatus() const
{
	const FPeriod* P = CurrentPeriod();
	int32 Out = 0, Inside = 0, Dormant = 0;
	for (const FMember& M : Members)
	{
		if (!M.Walker.IsValid()) { continue; }
		if (M.Walker->IsInside()) { ++Inside; } else { ++Out; }
		if (M.Walker->IsDormant()) { ++Dormant; }
	}
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS status: %.2fh period=%s class=%d attended=%d truant=%d trouble=%.0f chase=%s busts=%d members=%d (out %d, inside %d, dormant %d) barks=%d conversations=%d bells=%d"),
		ClockHour(), P ? *P->Id : TEXT("?"), bInClass ? 1 : 0, bAttendedThis ? 1 : 0, bTruant ? 1 : 0, Points, Chaser.IsValid() ? *Chaser->StudentId : TEXT("-"), BustCount,
		Members.Num(), Out, Inside, Dormant, BarksPlayed, ConversationsPlayed, BellsRung);
	for (const TCHAR* F : { TEXT("bullies"), TEXT("nerds"), TEXT("preppies"), TEXT("greasers"), TEXT("jocks"), TEXT("students"), TEXT("townies") })
	{
		UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS attitude %s -> Jimmy: %s"), F, AttitudeName(AttitudeToPlayer(F)));
	}
}

namespace
{
	FAutoConsoleCommandWithWorld GCampusStatus(TEXT("Campus.Status"), TEXT("Log campus life state"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (UBullyCampusSubsystem* S = UBullyCampusSubsystem::Get(W)) { S->LogStatus(); } }));
	FAutoConsoleCommandWithWorldAndArgs GCampusTrouble(TEXT("Campus.Trouble"), TEXT("Campus.Trouble <points>: set the trouble meter"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W) { if (UBullyCampusSubsystem* S = UBullyCampusSubsystem::Get(W)) { S->SetTrouble(A.Num() ? FCString::Atof(*A[0]) : 0.f, TEXT("console")); } }));
	FAutoConsoleCommandWithWorld GCampusBell(TEXT("Campus.Bell"), TEXT("Ring the school bell"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (UBullyCampusSubsystem* S = UBullyCampusSubsystem::Get(W)) { S->RingBell(TEXT("console")); } }));
}

// ---------------------------------------------------------------------------------------------------------------
// -BRCampusSmoke: a headless school-day check (tools/ue/run_campus_smoke.sh). The clock is driven through the real
// schedule: 8:43 on the front lawn -> heading to class -> the 9:00 bell -> the grounds clear -> late for class ->
// a prefect spots, chases and busts Jimmy -> sent to class (clock to the end of the period, the class-end bell) ->
// students stream back out -> hitting a student raises trouble -> barks / a conversation -> the 1_02B heat contract.

void UBullyCampusSubsystem::SmokeCheck(const TCHAR* Name, bool bOk, const FString& Detail)
{
	if (bOk) { ++SmokePass; } else { ++SmokeFail; }
	UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS_SMOKE %s %s: %s"), Name, bOk ? TEXT("PASS") : TEXT("FAIL"), *Detail);
}

void UBullyCampusSubsystem::TickSmoke(float Dt)
{
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	UBullyClockSubsystem* Clock = UBullyClockSubsystem::Get(World);
	UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(World);
	APawn* P = Player();
	if (!Clock || !P) { return; }
	const double InStep = Now - SmokeStepAt;
	auto Next = [&]() { ++SmokeStep; SmokeStepAt = Now; };
	auto CountGrounds = [&](int32& Out, int32& In)
	{
		Out = In = 0;
		for (const FMember& M : Members)
		{
			if (M.bPrefect || M.Zone != TEXT("grounds") || !M.Walker.IsValid() || M.Walker->IsCampusSuppressed()) { continue; }
			(M.Walker->IsInside() ? In : Out) += 1;
		}
	};
	auto Lawn = FVector(25500.f, 7150.f, 700.f);
	switch (SmokeStep)
	{
	case 0:
		if (Now < 3.0 || Members.Num() == 0) { if (Now > 30.0) { SmokeCheck(TEXT("cast"), false, TEXT("no campus cast after 30 s")); SmokeStep = 99; } return; }
		{
			SmokeStart = Now;
			for (const TCHAR* Id : { TEXT("arrival"), TEXT("day_one"), TEXT("1_02B"), TEXT("1_02C") }) { if (Camp && !Camp->IsCompleted(Id)) { Camp->NotifyPassed(Id); } }
			SeedRespect();
			int32 Pref = 0, Halls = 0, Factions = 0;
			TSet<FString> F;
			for (const FMember& M : Members) { Pref += M.bPrefect ? 1 : 0; Halls += M.Zone == TEXT("halls") ? 1 : 0; F.Add(M.Faction); }
			Factions = F.Num();
			SmokeCheck(TEXT("cast"), Members.Num() >= 10 && Pref >= 1 && Halls >= 1 && Factions >= 4,
				FString::Printf(TEXT("%d members, %d prefects, %d in the halls, %d factions"), Members.Num(), Pref, Halls, Factions));
			const int32 Before = AttitudeToPlayer(TEXT("nerds"));
			const int32 Saved = Camp ? Camp->GetRespect(FName(TEXT("nerds"))) : 0;
			if (Camp) { Camp->SetRespect(FName(TEXT("nerds")), 85); }
			const int32 After = AttitudeToPlayer(TEXT("nerds"));
			if (Camp) { Camp->SetRespect(FName(TEXT("nerds")), Saved); }
			SmokeCheck(TEXT("faction_respect"), After == FMath::Min(4, Before + 1) && AttitudeToPlayer(TEXT("bullies")) <= 1,
				FString::Printf(TEXT("nerds %s -> %s at respect 85; bullies %s"), AttitudeName(Before), AttitudeName(After), AttitudeName(AttitudeToPlayer(TEXT("bullies")))));
			PlacePlayer(Lawn, 180.f);
			Clock->UnpauseClock();
			Clock->SetClock(8, 43);
			SmokeBellsAtStart = BellsRung;
			Next();
		}
		return;
	case 1:   // dockers start 8:45
		if (bDocking || bInClass) { int32 O = 0, I = 0; CountGrounds(O, I); SmokeCheck(TEXT("head_to_class"), true, FString::Printf(TEXT("docking at %.2fh, %d out / %d in"), ClockHour(), O, I)); Next(); }
		else if (InStep > 40.0) { SmokeCheck(TEXT("head_to_class"), false, TEXT("no docking by 8:45")); Next(); }
		return;
	case 2:   // the 9:00 bell
		if (BellsRung > SmokeBellsAtStart && bInClass) { SmokeCheck(TEXT("bell_class_start"), true, FString::Printf(TEXT("bell at %.2fh (%s)"), ClockHour(), *ClassId)); Next(); }
		else if (InStep > 40.0) { SmokeCheck(TEXT("bell_class_start"), false, TEXT("no bell by 9:00")); Next(); }
		return;
	case 3:   // late for class (9:02) while still on the lawn
		if (bTruant) { SmokeCheck(TEXT("truancy"), Points >= TruancyFloor, FString::Printf(TEXT("late for class at %.2fh, trouble %.0f"), ClockHour(), Points)); Next(); }
		else if (InStep > 30.0) { SmokeCheck(TEXT("truancy"), false, FString::Printf(TEXT("not truant at %.2fh (on grounds %d)"), ClockHour(), PlayerOnGrounds() ? 1 : 0)); Next(); }
		return;
	case 4:   // the grounds clear (by ~9:15)
	{
		int32 O = 0, I = 0;
		CountGrounds(O, I);
		if (O == 0 || (InStep > 20.0 && O * 10 <= (O + I))) { SmokeCheck(TEXT("grounds_cleared"), true, FString::Printf(TEXT("%d of %d students inside at %.2fh"), I, O + I, ClockHour())); Next(); }
		else if (InStep > 45.0) { SmokeCheck(TEXT("grounds_cleared"), false, FString::Printf(TEXT("%d students still out at %.2fh"), O, ClockHour())); Next(); }
		return;
	}
	case 5:   // put Jimmy in front of a prefect
	{
		FMember* Best = nullptr;
		for (FMember& M : Members) { if (M.bPrefect && M.Zone == TEXT("grounds") && M.Walker.IsValid() && !M.Walker->IsDormant()) { Best = &M; break; } }
		if (!Best) { for (FMember& M : Members) { if (M.bPrefect && M.Zone == TEXT("grounds") && M.Walker.IsValid()) { Best = &M; break; } } }
		if (!Best) { SmokeCheck(TEXT("prefect_chase"), false, TEXT("no grounds prefect")); SmokeStep = 8; return; }
		AAmbientWalker* W = Best->Walker.Get();
		W->SetDormant(false);
		SmokeTarget = W;
		Best->NextNotice = 0.0;
		const FVector Fwd = W->GetActorForwardVector().GetSafeNormal2D();
		PlacePlayer(W->GetActorLocation() + Fwd * 550.f, (W->GetActorLocation() - (W->GetActorLocation() + Fwd * 550.f)).Rotation().Yaw);
		UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS_SMOKE player placed 5.5 m in front of prefect %s"), *W->StudentId);
		Next();
		return;
	}
	case 6:   // chase + bust
		if (Chaser.IsValid() && SmokeTarget.IsValid() && SmokeStepAt >= 0.0 && !bSmokeChaseSeen) { bSmokeChaseSeen = true; SmokeCheck(TEXT("prefect_chase"), true, FString::Printf(TEXT("%s chasing (trouble %.0f)"), *Chaser->StudentId, Points)); }
		if (BustCount > 0)
		{
			if (!bSmokeChaseSeen) { SmokeCheck(TEXT("prefect_chase"), true, TEXT("bust without a logged chase frame")); }
			SmokeCheck(TEXT("busted"), BustOutcome == TEXT("class"), FString::Printf(TEXT("bust #%d -> %s"), BustCount, *BustOutcome));
			Next();
		}
		else if (InStep > 40.0) { SmokeCheck(TEXT("busted"), false, FString::Printf(TEXT("no bust (chaser %s)"), Chaser.IsValid() ? *Chaser->StudentId : TEXT("none"))); Next(); }
		return;
	case 7:   // sent to class: clock at the end of the period, the class-end bell, meter cleared
		if (BustPhase == 0 && !bInClass)
		{
			SmokeCheck(TEXT("sent_to_class"), Points <= 0.f && BellsRung >= SmokeBellsAtStart + 2 && ClockHour() >= 11.49f,
				FString::Printf(TEXT("clock %.2fh, bells %d, trouble %.0f, player at %s"), ClockHour(), BellsRung - SmokeBellsAtStart, Points, *P->GetActorLocation().ToString()));
			Next();
		}
		else if (InStep > 30.0) { SmokeCheck(TEXT("sent_to_class"), false, FString::Printf(TEXT("bust phase %d, in class %d"), BustPhase, bInClass ? 1 : 0)); Next(); }
		return;
	case 8:   // students stream back out
	{
		int32 O = 0, I = 0;
		CountGrounds(O, I);
		if (O >= 3) { SmokeCheck(TEXT("stream_out"), true, FString::Printf(TEXT("%d out, %d still inside at %.2fh"), O, I, ClockHour())); PlacePlayer(Lawn, 180.f); Next(); }
		else if (InStep > 40.0) { SmokeCheck(TEXT("stream_out"), false, FString::Printf(TEXT("only %d out at %.2fh"), O, ClockHour())); PlacePlayer(Lawn, 180.f); Next(); }
		return;
	}
	case 9:   // hit a student (the real health path; the walker reacts by fighting or fleeing)
	{
		if (InStep < 3.0) { return; }
		AAmbientWalker* Victim = nullptr;
		float BestD = 1e9f;
		for (FMember& M : Members)
		{
			AAmbientWalker* W = M.Walker.Get();
			if (!W || M.bPrefect || W->IsInside() || W->IsBrawling()) { continue; }
			const float D = FVector::Dist(W->GetActorLocation(), P->GetActorLocation());
			if (D < BestD) { BestD = D; Victim = W; }
		}
		if (!Victim || !Victim->GetCombat()) { SmokeCheck(TEXT("hit_trouble"), false, TEXT("no student to hit")); SmokeStep = 11; return; }
		Victim->SetDormant(false);
		SmokePointsBefore = Points;
		SmokeTarget = Victim;
		Victim->GetCombat()->ApplyWeaponHit(EBullyHitReaction::Flinch, 5.f);
		UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS_SMOKE hit %s (%.0f m away)"), *Victim->StudentId, BestD / 100.f);
		Next();
		return;
	}
	case 10:
		if (Points >= SmokePointsBefore + HitStudentPoints - 1.f)
		{
			const bool bReact = SmokeTarget.IsValid() && (SmokeTarget->IsBrawling() || SmokeTarget->GetCampusMode() == AAmbientWalker::ECampusMode::Flee);
			SmokeCheck(TEXT("hit_trouble"), bReact, FString::Printf(TEXT("trouble %.0f -> %.0f; %s %s"), SmokePointsBefore, Points,
				SmokeTarget.IsValid() ? *SmokeTarget->StudentId : TEXT("?"), SmokeTarget.IsValid() && SmokeTarget->IsBrawling() ? TEXT("fights back") : TEXT("runs")));
			Next();
		}
		else if (InStep > 5.0) { SmokeCheck(TEXT("hit_trouble"), false, FString::Printf(TEXT("trouble %.0f -> %.0f"), SmokePointsBefore, Points)); Next(); }
		return;
	case 11:   // barks + a conversation between two idle students (needs the speech import)
		if (ConversationsPlayed > 0 || (Speech.Num() == 0 && InStep > 2.0))
		{
			SmokeCheck(TEXT("barks"), BarksPlayed >= 3, FString::Printf(TEXT("%d barks, %d conversations, %d voices imported"), BarksPlayed, ConversationsPlayed, Speech.Num()));
			SmokeCheck(TEXT("conversation"), Speech.Num() == 0 || ConversationsPlayed > 0, Speech.Num() == 0 ? TEXT("SKIPPED: no speech import") : FString::Printf(TEXT("%d played"), ConversationsPlayed));
			Next();
		}
		else if (InStep > 75.0)
		{
			SmokeCheck(TEXT("barks"), BarksPlayed >= 3, FString::Printf(TEXT("%d barks"), BarksPlayed));
			SmokeCheck(TEXT("conversation"), false, TEXT("no conversation in 75 s"));
			Next();
		}
		else if (FMath::Fmod(InStep, 15.0) < Dt)
		{
			// walk Jimmy past the hangouts (conversations only start within earshot)
			for (FMember& M : Members)
			{
				if (!M.bPrefect && M.Walker.IsValid() && M.Walker->IsIdling() && M.Zone == TEXT("grounds")) { PlacePlayer(M.Walker->GetActorLocation() + FVector(600.f, 0.f, 0.f), 180.f); break; }
			}
		}
		return;
	case 12:   // the 1_02B heat contract: held until hidden, drains while hidden, reaches 0
		if (InStep < 0.1) { SetTrouble(0.f, TEXT("smoke reset")); MissionHeatSet(200.f, 25.f); return; }
		if (InStep < 2.0) { if (Points < 199.f) { SmokeCheck(TEXT("mission_heat"), false, FString::Printf(TEXT("drained before hiding (%.0f)"), Points)); SmokeStep = 13; } return; }
		if (!bHidden) { SetPlayerHidden(true); return; }
		if (Points <= 0.f) { SmokeCheck(TEXT("mission_heat"), true, FString::Printf(TEXT("200 held, drained to 0 in %.1f s hidden"), InStep - 2.0)); SetPlayerHidden(false); Next(); }
		else if (InStep > 20.0) { SmokeCheck(TEXT("mission_heat"), false, FString::Printf(TEXT("still %.0f"), Points)); SetPlayerHidden(false); Next(); }
		return;
	case 13:
	case 99:
	{
		LogStatus();
		const bool bOk = SmokeFail == 0 && SmokePass > 0;
		UE_LOG(LogBullyCampus, Display, TEXT("BR_CAMPUS SMOKE %s (%d/%d checks, %.0f s)"), bOk ? TEXT("PASS") : TEXT("FAIL"), SmokePass, SmokePass + SmokeFail, Now - SmokeStart);
		SmokeStep = 100;
		FPlatformMisc::RequestExit(false);
		return;
	}
	default:
		return;
	}
}
