// OWNER: mission-fw
#include "MissionSubsystem.h"

#include "BullyCampaignSubsystem.h"
#include "BullyCutscene.h"
#include "BullyCharacter.h"
#include "BullyClockSubsystem.h"
#include "BullyCombatComponent.h"
#include "BullyCombatPawn.h"
#include "BullyCombatTest.h"
#include "BullyHUD.h"
#include "BullyProgressSubsystem.h"
#include "BullyScriptedNPC.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Components/BoxComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "MissionTrigger.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBullyMission, Log, All);

namespace
{
	using FObj = TSharedPtr<FJsonObject>;

	FString MissionDir() { return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Missions")); }

	bool ReadVec3(const FJsonObject& O, const TCHAR* Field, FVector& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.TryGetArrayField(Field, A) && A->Num() >= 3)
		{
			Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
			return true;
		}
		return false;
	}

	FVector GameToUe(const FVector& M) { return FVector(M.X * 100.0, -M.Y * 100.0, M.Z * 100.0); }

	// A reach_location tighter than this is a thin target that a fast mover or a teleport steps over (see the door-link
	// race in docs/re-notes/mission-robustness.md); the parser never lets a reach get narrower than these.
	constexpr float MinReachRadiusCm = 200.f;
	constexpr float MinReachZCm = 150.f;

	/** A location from "point" (named), "location_cm"/"center_cm" or "game_m". `Prefix` allows e.g. "blip". */
	bool ReadLocation(const FJsonObject& O, const FMissionDef& D, FVector& Out)
	{
		FString PointName;
		if (O.TryGetStringField(TEXT("point"), PointName))
		{
			if (const FVector* P = D.Points.Find(FName(*PointName))) { Out = *P; return true; }
			for (const FMissionTriggerDef& T : D.Triggers) { if (T.Id == FName(*PointName)) { Out = T.Center; return true; } }
			return false;
		}
		if (ReadVec3(O, TEXT("location_cm"), Out) || ReadVec3(O, TEXT("center_cm"), Out)) { return true; }
		FVector M;
		if (ReadVec3(O, TEXT("game_m"), M)) { Out = GameToUe(M); return true; }
		return false;
	}

	bool ParseCondition(const FObj& O, const FMissionDef& D, FMissionCondition& C, FString& Err)
	{
		FString Type;
		if (!O.IsValid() || !O->TryGetStringField(TEXT("type"), Type)) { Err = TEXT("condition missing \"type\""); return false; }
		auto Num = [&](const TCHAR* F, float Def) { double V; return O->TryGetNumberField(F, V) ? float(V) : Def; };
		FString S;
		if (Type == TEXT("reach_location") || Type == TEXT("reach"))
		{
			C.Type = FMissionCondition::EType::Reach;
			if (!ReadLocation(*O, D, C.Location)) { Err = TEXT("reach_location: unknown point / no location"); return false; }
			C.Radius = Num(TEXT("radius_cm"), 300.f);
			C.ZTolerance = Num(TEXT("z_tolerance_cm"), 400.f);
			if (C.Radius < MinReachRadiusCm || C.ZTolerance < MinReachZCm)
			{
				UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION reach_location radius %.0f / z %.0f is under the %.0f / %.0f cm minimum: widened"), C.Radius, C.ZTolerance, MinReachRadiusCm, MinReachZCm);
				C.Radius = FMath::Max(C.Radius, MinReachRadiusCm);
				C.ZTolerance = FMath::Max(C.ZTolerance, MinReachZCm);
			}
		}
		else if (Type == TEXT("trigger_enter") || Type == TEXT("trigger_exit"))
		{
			C.Type = Type == TEXT("trigger_enter") ? FMissionCondition::EType::TriggerEnter : FMissionCondition::EType::TriggerExit;
			if (!O->TryGetStringField(TEXT("trigger"), S)) { Err = Type + TEXT(": missing \"trigger\""); return false; }
			C.Name = FName(*S);
		}
		else if (Type == TEXT("interact") || Type == TEXT("social"))
		{
			C.Type = FMissionCondition::EType::Interact;
			if (O->TryGetStringField(TEXT("trigger"), S)) { C.Name = FName(*S); }
			else if (O->TryGetStringField(TEXT("actor_tag"), S)) { C.Name = FName(*S); C.bActorTag = true; }
			else { Err = Type + TEXT(": needs \"trigger\" or \"actor_tag\""); return false; }
			C.Radius = Num(TEXT("radius_cm"), 250.f);
			O->TryGetStringField(TEXT("prompt"), C.Prompt);
			double MC = 0.0;
			if (O->TryGetNumberField(TEXT("money"), MC)) { C.MoneyCost = int32(MC); }   // social: the wallet must cover it
			FString F;
			if (O->TryGetStringField(TEXT("set_flag"), F)) { C.FlagToSet = FName(*F); }
		}
		else if (Type == TEXT("has_item"))
		{
			C.Type = FMissionCondition::EType::HasItem;
			if (!O->TryGetStringField(TEXT("name"), S) && !O->TryGetStringField(TEXT("item"), S)) { Err = TEXT("has_item: missing \"name\""); return false; }
			C.Name = FName(*S);
		}
		else if (Type == TEXT("attempt"))
		{
			C.Type = FMissionCondition::EType::Attempt;
			C.AttemptN = int32(Num(TEXT("n"), 1.f));
			O->TryGetBoolField(TEXT("eq"), C.bAttemptEq);
		}
		else if (Type == TEXT("leave_area"))
		{
			C.Type = FMissionCondition::EType::LeaveArea;
			C.Area = int32(Num(TEXT("area"), -1.f));
			if (C.Area < 0) { Err = TEXT("leave_area: missing \"area\""); return false; }
		}
		else if (Type == TEXT("leash"))
		{
			C.Type = FMissionCondition::EType::Leash;
			if (!O->TryGetStringField(TEXT("npc"), S)) { Err = TEXT("leash: missing \"npc\""); return false; }
			C.Name = FName(*S);
			C.Radius = Num(TEXT("radius_cm"), 3500.f);
			C.Seconds = Num(TEXT("seconds"), 10.f);
		}
		else if (Type == TEXT("zone_timer"))
		{
			C.Type = FMissionCondition::EType::ZoneTimer;
			if (!O->TryGetStringField(TEXT("trigger"), S)) { Err = TEXT("zone_timer: missing \"trigger\""); return false; }
			C.Name = FName(*S);
			C.Seconds = Num(TEXT("seconds"), 15.f);
			O->TryGetBoolField(TEXT("visible"), C.bVisible);
		}
		else if (Type == TEXT("event"))
		{
			C.Type = FMissionCondition::EType::Event;
			if (!O->TryGetStringField(TEXT("name"), S)) { Err = TEXT("event: missing \"name\""); return false; }
			C.Name = FName(*S);
			O->TryGetBoolField(TEXT("latched"), C.bLatched);
		}
		else if (Type == TEXT("timer")) { C.Type = FMissionCondition::EType::Timer; C.Seconds = Num(TEXT("seconds"), 1.f); O->TryGetBoolField(TEXT("visible"), C.bVisible); }
		else if (Type == TEXT("flag"))
		{
			C.Type = FMissionCondition::EType::Flag;
			if (!O->TryGetStringField(TEXT("name"), S)) { Err = TEXT("flag: missing \"name\""); return false; }
			C.Name = FName(*S);
		}
		else if (Type == TEXT("all") || Type == TEXT("any"))
		{
			C.Type = Type == TEXT("all") ? FMissionCondition::EType::All : FMissionCondition::EType::Any;
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (!O->TryGetArrayField(TEXT("conditions"), Arr) || Arr->Num() == 0) { Err = Type + TEXT(": needs \"conditions\""); return false; }
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FMissionCondition Child;
				if (!ParseCondition(V->AsObject(), D, Child, Err)) { return false; }
				C.Children.Add(MoveTemp(Child));
			}
		}
		else { Err = FString::Printf(TEXT("unknown condition type \"%s\""), *Type); return false; }
		return true;
	}

	bool ParseActions(const FObj& Parent, const TCHAR* Field, const FMissionDef& D, TArray<FMissionAction>& Out, FString& Err)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Parent->TryGetArrayField(Field, Arr)) { return true; }
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			FObj O = V->AsObject();
			FString Type, S;
			if (!O.IsValid() || !O->TryGetStringField(TEXT("type"), Type)) { Err = TEXT("action missing \"type\""); return false; }
			FMissionAction A;
			double Sec = 0.0;
			if (O->TryGetNumberField(TEXT("seconds"), Sec)) { A.Seconds = float(Sec); }
			O->TryGetStringField(TEXT("text"), A.Text);
			O->TryGetStringField(TEXT("sub"), A.Sub);
			double Yaw = 0.0; if (O->TryGetNumberField(TEXT("yaw"), Yaw)) { A.Yaw = float(Yaw); }
			if (O->TryGetStringField(TEXT("name"), S)) { A.Name = FName(*S); }
			if (Type == TEXT("hint")) { A.Type = FMissionAction::EType::Hint; }
			else if (Type == TEXT("banner")) { A.Type = FMissionAction::EType::Banner; }
			else if (Type == TEXT("event") || Type == TEXT("fire_event")) { A.Type = FMissionAction::EType::Event; }
			else if (Type == TEXT("set_flag")) { A.Type = FMissionAction::EType::SetFlag; }
			else if (Type == TEXT("clear_flag")) { A.Type = FMissionAction::EType::ClearFlag; }
			else if (Type == TEXT("log")) { A.Type = FMissionAction::EType::Log; }
			else if (Type == TEXT("teleport"))
			{
				A.Type = FMissionAction::EType::Teleport;
				if (!ReadLocation(*O, D, A.Location)) { Err = TEXT("teleport: unknown point / no location"); return false; }
			}
			else if (Type == TEXT("start_mission"))
			{
				A.Type = FMissionAction::EType::StartMission;
				if (!O->TryGetStringField(TEXT("mission"), A.Text) || A.Text.IsEmpty()) { Err = TEXT("start_mission: missing \"mission\" id"); return false; }
				if (A.Seconds >= 5.f) { A.Seconds = 0.5f; }   // default: brief beat after the complete banner, unless overridden
			}
			else if (Type == TEXT("spawn_enemy"))
			{
				A.Type = FMissionAction::EType::SpawnEnemy;
				double D2 = 0.0;
				if (O->TryGetNumberField(TEXT("distance_cm"), D2)) { A.Dist = float(D2); }
				if (O->TryGetNumberField(TEXT("health_floor"), D2)) { A.HealthFloor = float(D2); }
				bool b = false;
				if (O->TryGetBoolField(TEXT("player_invulnerable"), b)) { A.PlayerInvuln = b ? 1 : 0; }
				if (O->TryGetBoolField(TEXT("gate_attack"), b)) { A.GateAttack = b ? 1 : 0; }
				if (O->TryGetBoolField(TEXT("gate_block"), b)) { A.GateBlock = b ? 1 : 0; }
				O->TryGetStringField(TEXT("mesh"), A.EnemyMesh);
			}
			else if (Type == TEXT("fade"))
			{
				A.Type = FMissionAction::EType::Fade;
				double F = 0.0;
				if (O->TryGetNumberField(TEXT("out"), F))  { A.FadeOut  = FMath::Max(0.f, float(F)); }
				if (O->TryGetNumberField(TEXT("hold"), F)) { A.FadeHold = FMath::Max(0.f, float(F)); }
				if (O->TryGetNumberField(TEXT("in"), F))   { A.FadeIn   = FMath::Max(0.f, float(F)); }
				// Optional teleport (resolved like the teleport action) happens at full black, so there is no visible jump.
				A.bFadeTeleport = ReadLocation(*O, D, A.Location);
			}
			else if (Type == TEXT("reward"))
			{
				A.Type = FMissionAction::EType::Reward;
				double N = 0.0;
				if (O->TryGetNumberField(TEXT("money"), N))   { A.Money = int32(N); }
				if (O->TryGetNumberField(TEXT("respect"), N)) { A.Respect = int32(N); }
			}
			else if (Type == TEXT("letterbox"))
			{
				A.Type = FMissionAction::EType::Letterbox;
				bool b = true; O->TryGetBoolField(TEXT("on"), b); A.bOn = b;   // default on
			}
			else if (Type == TEXT("say"))
			{
				A.Type = FMissionAction::EType::Say;
				O->TryGetStringField(TEXT("speaker"), A.Sub);   // Sub = speaker label; Text (line) already read above
				if (A.Text.IsEmpty()) { O->TryGetStringField(TEXT("line"), A.Text); }   // "line" accepted as an alias for "text"
				if (!O->HasField(TEXT("seconds"))) { A.Seconds = 4.f; }
			}
			else if (Type == TEXT("camera"))
			{
				A.Type = FMissionAction::EType::Camera;
				double B = 0.0; if (O->TryGetNumberField(TEXT("blend"), B)) { A.Blend = FMath::Max(0.f, float(B)); }
				bool bRel = false; O->TryGetBoolField(TEXT("release"), bRel); A.bRelease = bRel;
				if (!A.bRelease)
				{
					if (!ReadLocation(*O, D, A.Location)) { Err = TEXT("camera: needs a point / location_cm"); return false; }
					// look target: "look":"player"|"<point>" or a "look_cm":[x,y,z] spot (a point name wins)
					FString Lk;
					if (O->TryGetStringField(TEXT("look"), Lk))
					{
						if (Lk.Equals(TEXT("player"), ESearchCase::IgnoreCase)) { A.bLookPlayer = true; }
						else if (const FVector* P = D.Points.Find(FName(*Lk))) { A.Look = *P; A.bHasLook = true; }
						else { Err = FString::Printf(TEXT("camera: unknown look point \"%s\""), *Lk); return false; }
					}
					else if (ReadVec3(*O, TEXT("look_cm"), A.Look)) { A.bHasLook = true; }
					double F = 0.0; if (O->TryGetNumberField(TEXT("fov"), F)) { A.Fov = float(F); }
				}
			}
			else if (Type == TEXT("shake"))
			{
				A.Type = FMissionAction::EType::Shake;
				double Am = 0.0; if (O->TryGetNumberField(TEXT("amp"), Am)) { A.Amp = FMath::Max(0.f, float(Am)); }
				if (!O->HasField(TEXT("seconds"))) { A.Seconds = 0.3f; }
			}
			else if (Type == TEXT("give_weapon") || Type == TEXT("give_ammo"))
			{
				A.Type = Type == TEXT("give_weapon") ? FMissionAction::EType::GiveWeapon : FMissionAction::EType::GiveAmmo;
				double W = 0.0;
				if (O->TryGetNumberField(TEXT("weapon"), W)) { A.WeaponId = int32(W); }
				else if (O->TryGetStringField(TEXT("weapon"), S)) { A.Text = S; }
				else { Err = Type + TEXT(": missing \"weapon\""); return false; }
				double N2 = 0.0;
				if (O->TryGetNumberField(TEXT("n"), N2)) { A.Amount = int32(N2); }
			}
			else if (Type == TEXT("set_respect"))
			{
				A.Type = FMissionAction::EType::SetRespect;
				if (!O->TryGetStringField(TEXT("faction"), S)) { Err = TEXT("set_respect: missing \"faction\""); return false; }
				A.Name = FName(*S);
				double V2 = 0.0;
				if (O->TryGetNumberField(TEXT("value"), V2)) { A.Amount = int32(V2); }
			}
			else if (Type == TEXT("clock"))
			{
				A.Type = FMissionAction::EType::Clock;
				bool b = false;
				if (O->TryGetStringField(TEXT("set"), S)) { A.Text = S; }
				else if (O->TryGetBoolField(TEXT("pause"), b) && b) { A.Amount = 1; }
				else if (O->TryGetBoolField(TEXT("unpause"), b) && b) { A.Amount = 2; }
				else { Err = TEXT("clock: needs \"set\" | \"pause\" | \"unpause\""); return false; }
			}
			else if (Type == TEXT("chapter_set"))
			{
				A.Type = FMissionAction::EType::ChapterSet;
				double C2 = 0.0;
				if (!O->TryGetNumberField(TEXT("chapter"), C2)) { Err = TEXT("chapter_set: missing \"chapter\""); return false; }
				A.Amount = int32(C2);
			}
			else if (Type == TEXT("force_complete"))
			{
				const TArray<TSharedPtr<FJsonValue>>* Ids = nullptr;
				if (O->TryGetStringField(TEXT("mission"), S))
				{
					A.Type = FMissionAction::EType::ForceComplete;
					A.Name = FName(*S);
					Out.Add(MoveTemp(A));
					continue;
				}
				if (!O->TryGetArrayField(TEXT("missions"), Ids) || Ids->Num() == 0) { Err = TEXT("force_complete: needs \"mission\" or \"missions\""); return false; }
				for (const TSharedPtr<FJsonValue>& Iv : *Ids)
				{
					FMissionAction Fa;
					Fa.Type = FMissionAction::EType::ForceComplete;
					Fa.Name = FName(*Iv->AsString());
					Out.Add(MoveTemp(Fa));
				}
				continue;
			}
			else if (Type == TEXT("spawn_npc"))
			{
				A.Type = FMissionAction::EType::SpawnNpc;
				if (!O->TryGetStringField(TEXT("npc"), S)) { Err = TEXT("spawn_npc: missing \"npc\" id"); return false; }
				A.NpcId = FName(*S);
				if (!ReadLocation(*O, D, A.Location)) { Err = TEXT("spawn_npc: unknown point / no location"); return false; }
				O->TryGetStringField(TEXT("mesh"), A.Mesh);
				bool b = false;
				if (O->TryGetBoolField(TEXT("mission_critical"), b)) { A.bMissionCritical = b; }
				if (O->TryGetBoolField(TEXT("invulnerable"), b)) { A.bInvulnerable = b; }
				if (O->TryGetBoolField(TEXT("untargetable"), b)) { A.bUntargetable = b; }
				FString Face;
				if (O->TryGetStringField(TEXT("face"), Face) && Face.Equals(TEXT("player"), ESearchCase::IgnoreCase)) { A.bFacePlayer = true; }
				double N2 = 0.0;
				if (O->TryGetNumberField(TEXT("min_health"), N2)) { A.MinHealth = float(N2); }
				if (O->TryGetStringField(TEXT("tag"), S)) { A.Name = FName(*S); }   // actor-tag override (default br_npc_<id>)
			}
			else if (Type == TEXT("npc_move_to") || Type == TEXT("npc_lead"))
			{
				A.Type = Type == TEXT("npc_lead") ? FMissionAction::EType::NpcLead : FMissionAction::EType::NpcMove;
				if (!O->TryGetStringField(TEXT("npc"), S)) { Err = Type + TEXT(": missing \"npc\" id"); return false; }
				A.NpcId = FName(*S);
				if (!ReadLocation(*O, D, A.Location)) { Err = Type + TEXT(": unknown point / no location"); return false; }
				bool b = false;
				if (O->TryGetBoolField(TEXT("run"), b)) { A.bRun = b; }
				if (O->TryGetStringField(TEXT("event"), S)) { A.Sub = S; }   // arrived-event override
				double R = 0.0;
				if (O->TryGetNumberField(TEXT("radius_cm"), R)) { A.Dist = float(R); }
				O->TryGetStringArrayField(TEXT("via"), A.Via);             // extra waypoints (point or path names)
				O->TryGetStringField(TEXT("path"), A.Text);                // a trigger-file path name
				O->TryGetStringField(TEXT("gait"), A.Gait);                // "walk" | "run" | "sprint"
			}
			else if (Type == TEXT("npc_face"))
			{
				A.Type = FMissionAction::EType::NpcFace;
				if (!O->TryGetStringField(TEXT("npc"), S)) { Err = TEXT("npc_face: missing \"npc\" id"); return false; }
				A.NpcId = FName(*S);
				FString Face;
				if (O->TryGetStringField(TEXT("face"), Face) && Face.Equals(TEXT("player"), ESearchCase::IgnoreCase)) { A.bFacePlayer = true; }
				else if (!ReadLocation(*O, D, A.Location)) { Err = TEXT("npc_face: needs \"face\" or a point"); return false; }
			}
			else if (Type == TEXT("npc_despawn"))
			{
				A.Type = FMissionAction::EType::NpcDespawn;
				if (!O->TryGetStringField(TEXT("npc"), S)) { Err = TEXT("npc_despawn: missing \"npc\" id"); return false; }
				A.NpcId = FName(*S);
			}
			else if (Type == TEXT("pair_anim"))
			{
				A.Type = FMissionAction::EType::PairAnim;
				if (!O->TryGetStringField(TEXT("npc"), S)) { Err = TEXT("pair_anim: missing \"npc\" id"); return false; }
				A.NpcId = FName(*S);
				O->TryGetStringField(TEXT("clips"), A.Text);   // comma-separated candidates
				if (!O->HasField(TEXT("seconds"))) { A.Seconds = 1.5f; }
			}
			else if (Type == TEXT("speech_sequence"))
			{
				A.Type = FMissionAction::EType::SpeechSequence;
				if (!O->TryGetStringField(TEXT("id"), S)) { Err = TEXT("speech_sequence: missing \"id\""); return false; }
				A.Name = FName(*S);
				const TArray<TSharedPtr<FJsonValue>>* Ls = nullptr;
				if (!O->TryGetArrayField(TEXT("lines"), Ls) || Ls->Num() == 0) { Err = TEXT("speech_sequence: needs \"lines\""); return false; }
				for (const TSharedPtr<FJsonValue>& Lv : *Ls)
				{
					FObj LO = Lv->AsObject();
					if (!LO.IsValid()) { continue; }
					FSpeechLine L;
					LO->TryGetStringField(TEXT("speaker"), L.Speaker);
					if (!LO->TryGetStringField(TEXT("text"), L.Text)) { LO->TryGetStringField(TEXT("line"), L.Text); }
					double Sec2 = 0.0;
					if (LO->TryGetNumberField(TEXT("seconds"), Sec2)) { L.Seconds = float(Sec2); }
					A.Lines.Add(MoveTemp(L));
				}
			}
			else if (Type == TEXT("minigame_launch"))
			{
				A.Type = FMissionAction::EType::MinigameLaunch;
				if (!O->TryGetStringField(TEXT("id"), S)) { Err = TEXT("minigame_launch: missing \"id\""); return false; }
				A.Name = FName(*S);
				FString Trig;
				if (O->TryGetStringField(TEXT("trigger"), Trig))
				{
					bool bFound = false;
					for (const FMissionTriggerDef& T : D.Triggers) { if (T.Id == FName(*Trig)) { A.Location = T.Center; bFound = true; break; } }
					if (!bFound) { Err = FString::Printf(TEXT("minigame_launch: unknown trigger \"%s\""), *Trig); return false; }
				}
				else if (!ReadLocation(*O, D, A.Location)) { Err = TEXT("minigame_launch: needs \"point\" or \"trigger\""); return false; }
				if (O->TryGetStringField(TEXT("success"), S)) { A.SuccessEvent = FName(*S); }
				else { A.SuccessEvent = FName(*FString::Printf(TEXT("minigame:%s_ok"), *A.Name.ToString())); }
				if (O->TryGetStringField(TEXT("fail"), S)) { A.FailEvent = FName(*S); }
				else { A.FailEvent = FName(*FString::Printf(TEXT("minigame:%s_fail"), *A.Name.ToString())); }
				double R = 0.0;
				if (O->TryGetNumberField(TEXT("radius_cm"), R)) { A.Radius = float(R); }
				if (!O->HasField(TEXT("seconds"))) { A.Seconds = 4.f; }
			}
			else if (Type == TEXT("give_item") || Type == TEXT("take_item"))
			{
				A.Type = Type == TEXT("give_item") ? FMissionAction::EType::GiveItem : FMissionAction::EType::TakeItem;
				if (!O->TryGetStringField(TEXT("item"), S)) { Err = Type + TEXT(": missing \"item\""); return false; }
				A.Name = FName(*S);
				double N2 = 0.0;
				if (O->TryGetNumberField(TEXT("n"), N2)) { A.Amount = int32(N2); }
				if (A.Amount <= 0) { A.Amount = 1; }
			}
			else if (Type == TEXT("money_ensure") || Type == TEXT("money_take"))
			{
				A.Type = Type == TEXT("money_ensure") ? FMissionAction::EType::MoneyEnsure : FMissionAction::EType::MoneyTake;
				double M2 = 0.0;
				if (!O->TryGetNumberField(TEXT("money"), M2)) { Err = Type + TEXT(": missing \"money\""); return false; }
				A.Money = int32(M2);
			}
			else if (Type == TEXT("heat_set"))
			{
				A.Type = FMissionAction::EType::HeatSet;
				double P2 = 0.0;
				if (O->TryGetNumberField(TEXT("points"), P2)) { A.Amount = int32(P2); }
				if (O->TryGetNumberField(TEXT("decay"), P2)) { A.Amp = float(P2); }
			}
			else if (Type == TEXT("hide"))
			{
				A.Type = FMissionAction::EType::Hide;
				bool b = true; O->TryGetBoolField(TEXT("on"), b); A.bOn = b;
			}
			else if (Type == TEXT("spawn_wave"))
			{
				A.Type = FMissionAction::EType::SpawnWave;
				if (!O->TryGetStringField(TEXT("group"), S)) { Err = TEXT("spawn_wave: missing \"group\""); return false; }
				A.Name = FName(*S);
				if (!O->TryGetStringArrayField(TEXT("points"), A.PointNames) || A.PointNames.Num() == 0) { Err = TEXT("spawn_wave: needs \"points\""); return false; }
				O->TryGetStringField(TEXT("mesh"), A.Mesh);
				double N2 = 0.0;
				if (O->TryGetNumberField(TEXT("health"), N2)) { A.Health = float(N2); }
				if (O->TryGetNumberField(TEXT("hits_to_beat"), N2)) { A.HitsToBeat = int32(N2); }
				if (O->TryGetNumberField(TEXT("advance_at"), N2)) { A.AdvanceAt = int32(N2); }
				bool b = true;
				if (O->TryGetBoolField(TEXT("hostile"), b)) { A.bHostile = b; }
				if (O->TryGetStringField(TEXT("target"), S)) { A.Sub = S; }   // every other member fights this defender
			}
			else if (Type == TEXT("spotter"))
			{
				A.Type = FMissionAction::EType::Spotter;
				if (!O->TryGetStringField(TEXT("npc"), S)) { Err = TEXT("spotter: missing \"npc\" id"); return false; }
				A.NpcId = FName(*S);
				double N2 = 0.0;
				if (O->TryGetNumberField(TEXT("range_cm"), N2)) { A.Radius = float(N2); }
				if (O->TryGetNumberField(TEXT("cone_deg"), N2)) { A.Amp = float(N2); }
				if (O->TryGetNumberField(TEXT("seconds"), N2)) { A.Seconds = float(N2); }
				if (O->TryGetStringField(TEXT("event"), S)) { A.Sub = S; }   // spotted-event override
			}
			else if (Type == TEXT("npc_follow"))
			{
				A.Type = FMissionAction::EType::NpcFollow;
				if (!O->TryGetStringField(TEXT("npc"), S)) { Err = TEXT("npc_follow: missing \"npc\" id"); return false; }
				A.NpcId = FName(*S);
				double R = 0.0;
				if (O->TryGetNumberField(TEXT("radius_cm"), R)) { A.Dist = float(R); }
				O->TryGetStringField(TEXT("gait"), A.Gait);
			}
			else if (Type == TEXT("countdown_start"))
			{
				A.Type = FMissionAction::EType::CountdownStart;
				if (!O->HasField(TEXT("seconds"))) { Err = TEXT("countdown_start: missing \"seconds\""); return false; }   // generic top already read it
				if (O->TryGetStringField(TEXT("event"), S)) { A.Name = FName(*S); }
				bool b = false;
				if (O->TryGetBoolField(TEXT("resume"), b)) { A.bResume = b; }
				if (O->TryGetBoolField(TEXT("visible"), b)) { A.bCountdownVisible = b; }
			}
			else if (Type == TEXT("countdown_cancel"))
			{
				A.Type = FMissionAction::EType::CountdownCancel;
			}
			else if (Type == TEXT("lesson_start"))
			{
				A.Type = FMissionAction::EType::LessonStart;
				if (!O->TryGetStringField(TEXT("id"), S)) { Err = TEXT("lesson_start: missing \"id\""); return false; }
				A.Name = FName(*S);
				if (!O->TryGetStringField(TEXT("move"), A.Sub)) { Err = TEXT("lesson_start: missing \"move\""); return false; }
				double N2 = 0.0;
				if (O->TryGetNumberField(TEXT("count"), N2)) { A.Amount = int32(N2); }
				if (A.Text.IsEmpty()) { O->TryGetStringField(TEXT("prompt"), A.Text); }
			}
			else if (Type == TEXT("spawn_defender"))
			{
				A.Type = FMissionAction::EType::SpawnDefender;
				if (!O->TryGetStringField(TEXT("npc"), S)) { Err = TEXT("spawn_defender: missing \"npc\" id"); return false; }
				A.NpcId = FName(*S);
				if (!ReadLocation(*O, D, A.Location)) { Err = TEXT("spawn_defender: unknown point / no location"); return false; }
				O->TryGetStringField(TEXT("mesh"), A.Mesh);
				double N2 = 0.0;
				if (O->TryGetNumberField(TEXT("health"), N2)) { A.Health = float(N2); }
				if (O->TryGetNumberField(TEXT("drain_hp"), N2)) { A.Amount = int32(N2); }
				if (O->TryGetNumberField(TEXT("drain_every"), N2)) { A.Seconds = float(N2); }
			}
			else if (Type == TEXT("npc_drain"))
			{
				A.Type = FMissionAction::EType::NpcDrain;
				if (!O->TryGetStringField(TEXT("npc"), S)) { Err = TEXT("npc_drain: missing \"npc\" id"); return false; }
				A.NpcId = FName(*S);
				bool b = true; O->TryGetBoolField(TEXT("on"), b); A.bOn = b;
			}
			else if (Type == TEXT("dialogue_choice"))
			{
				A.Type = FMissionAction::EType::DialogueChoice;
				if (!O->TryGetStringField(TEXT("id"), S)) { Err = TEXT("dialogue_choice: missing \"id\""); return false; }
				A.Name = FName(*S);
				if (A.Text.IsEmpty()) { O->TryGetStringField(TEXT("prompt"), A.Text); }
				const TArray<TSharedPtr<FJsonValue>>* Opts = nullptr;
				if (!O->TryGetArrayField(TEXT("options"), Opts) || Opts->Num() == 0) { Err = TEXT("dialogue_choice: needs \"options\""); return false; }
				for (const TSharedPtr<FJsonValue>& Ov : *Opts)
				{
					FObj OO = Ov->AsObject();
					if (!OO.IsValid()) { continue; }
					FChoiceOption Op;
					OO->TryGetStringField(TEXT("label"), Op.Label);
					if (!OO->TryGetStringField(TEXT("event"), S)) { Err = TEXT("dialogue_choice: option missing \"event\""); return false; }
					Op.Event = FName(*S);
					FString It;
					if (OO->TryGetStringField(TEXT("requires_item"), It)) { Op.RequiresItem = FName(*It); }
					A.Options.Add(MoveTemp(Op));
				}
			}
			else if (Type == TEXT("boss_bar"))
			{
				A.Type = FMissionAction::EType::BossBar;
				if (!O->TryGetStringField(TEXT("group"), S)) { Err = TEXT("boss_bar: missing \"group\""); return false; }
				A.Name = FName(*S);
				if (A.Text.IsEmpty()) { O->TryGetStringField(TEXT("label"), A.Text); }
				O->TryGetStringField(TEXT("half_line"), A.Sub);
			}
			else if (Type == TEXT("race_start"))
			{
				A.Type = FMissionAction::EType::RaceStart;
				if (!O->TryGetStringField(TEXT("id"), S)) { Err = TEXT("race_start: missing \"id\""); return false; }
				A.Name = FName(*S);
				if (!O->TryGetStringField(TEXT("course"), S)) { Err = TEXT("race_start: missing \"course\""); return false; }
				A.Sub = S;   // trigger-file path name
				double N2 = 0.0;
				if (O->TryGetNumberField(TEXT("laps"), N2)) { A.Amount = FMath::Max(1, int32(N2)); }
				if (O->TryGetNumberField(TEXT("checkpoint_radius_cm"), N2)) { A.Radius = float(N2); }
			}
			else if (Type == TEXT("clock_fail_at"))
			{
				A.Type = FMissionAction::EType::ClockFailAt;
				if (!O->TryGetStringField(TEXT("time"), S)) { Err = TEXT("clock_fail_at: missing \"time\" (\"h:mm\")"); return false; }
				A.Text = S;
			}
			else if (Type == TEXT("cutscene"))   // sm-cutscenes
			{
				A.Type = FMissionAction::EType::Cutscene;
				if (!O->TryGetStringField(TEXT("id"), S)) { Err = TEXT("cutscene: missing \"id\""); return false; }
				A.Name = FName(*S);
				if (!O->TryGetStringField(TEXT("event"), A.Sub)) { A.Sub = FString::Printf(TEXT("cutscene:%s"), *S); }
				if (!ParseActions(O, TEXT("else"), D, A.Else, Err)) { Err = FString::Printf(TEXT("cutscene %s else: %s"), *S, *Err); return false; }
			}
			else { Err = FString::Printf(TEXT("unknown action type \"%s\""), *Type); return false; }
			Out.Add(MoveTemp(A));
		}
		return true;
	}

	/** Optional extra named points from another tool's JSON (imported/triggers/day_one.json). Tolerant of layout: a root
	 *  "points" / "triggers" object (name -> entry) or array (entry has "name"/"id"); an entry's position is
	 *  "location_cm" | "ue_cm" | "cm" (UE cm) or "game_m" | "pos" | "position" (game metres); optional "half_extent_cm". */
	int32 LoadTriggerFile(const FString& RepoRelPath, FMissionDef& D)
	{
		const FString Full = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../.."), RepoRelPath));
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Full))
		{
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION trigger_file %s not present (optional); using inline points only"), *Full);
			return 0;
		}
		FObj Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION trigger_file %s is not valid JSON; ignored"), *Full);
			return 0;
		}
		int32 Added = 0;
		auto AddEntry = [&](const FString& Name, const FObj& E)
		{
			if (!E.IsValid() || Name.IsEmpty() || D.Points.Contains(FName(*Name))) { return; }   // inline wins
			FVector V;
			bool bOk = ReadVec3(*E, TEXT("location_cm"), V) || ReadVec3(*E, TEXT("ue_cm"), V) || ReadVec3(*E, TEXT("cm"), V);
			FVector M;
			if (!bOk && (ReadVec3(*E, TEXT("game_m"), M) || ReadVec3(*E, TEXT("pos"), M) || ReadVec3(*E, TEXT("position"), M))) { V = GameToUe(M); bOk = true; }
			if (!bOk) { return; }
			D.Points.Add(FName(*Name), V);
			FVector Ext;
			if (ReadVec3(*E, TEXT("half_extent_cm"), Ext))
			{
				FMissionTriggerDef T; T.Id = FName(*Name); T.Center = V; T.HalfExtent = Ext;
				D.Triggers.Add(T);
			}
			++Added;
		};
		for (const TCHAR* Key : { TEXT("points"), TEXT("triggers") })
		{
			const FObj* AsObj = nullptr; const TArray<TSharedPtr<FJsonValue>>* AsArr = nullptr;
			if (Root->TryGetObjectField(Key, AsObj))
			{
				for (const auto& Pair : (*AsObj)->Values) { AddEntry(FString(*Pair.Key), Pair.Value->AsObject()); }
			}
			else if (Root->TryGetArrayField(Key, AsArr))
			{
				for (const TSharedPtr<FJsonValue>& V : *AsArr)
				{
					FObj E = V->AsObject(); FString N;
					if (E.IsValid() && (E->TryGetStringField(TEXT("name"), N) || E->TryGetStringField(TEXT("id"), N))) { AddEntry(N, E); }
				}
			}
		}
		// imported/triggers/<id>.json "entities" layout (Trigger.img extract): an array of
		// { "name", "type": "point"|"trigger-volume"|..., "location_cm" | "location_game", "area_code" }. Points become
		// named points (inline wins); every entity with a location also grows its area's bounding box (leave_area).
		if (const TArray<TSharedPtr<FJsonValue>>* Ents = nullptr; Root->TryGetArrayField(TEXT("entities"), Ents))
		{
			for (const TSharedPtr<FJsonValue>& V : *Ents)
			{
				FObj E = V->AsObject();
				FString N;
				if (!E.IsValid() || !(E->TryGetStringField(TEXT("name"), N) || E->TryGetStringField(TEXT("id"), N))) { continue; }
				FVector Loc;
				FString Type;
				E->TryGetStringField(TEXT("type"), Type);
				if (Type == TEXT("path"))   // paths carry no root location: waypoint lists only
				{
					const TArray<TSharedPtr<FJsonValue>>* Wps = nullptr;
					if (E->TryGetArrayField(TEXT("waypoints"), Wps) && Wps->Num() > 0)
					{
						TArray<TPair<int32, FVector>> Ordered;
						for (const TSharedPtr<FJsonValue>& Wv : *Wps)
						{
							FObj W = Wv->AsObject();
							FVector WL;
							if (!W.IsValid() || !ReadVec3(*W, TEXT("location_cm"), WL)) { continue; }
							double Idx = 0.0;
							W->TryGetNumberField(TEXT("index"), Idx);
							Ordered.Add(TPair<int32, FVector>(int32(Idx), WL));
						}
						Ordered.Sort([](const TPair<int32, FVector>& A, const TPair<int32, FVector>& B) { return A.Key < B.Key; });
						TArray<FVector>& Path = D.Paths.FindOrAdd(FName(*N));
						for (const auto& Wp : Ordered) { Path.Add(Wp.Value); }
					}
					continue;
				}
				bool bOk = ReadVec3(*E, TEXT("location_cm"), Loc);
				FVector G;
				if (!bOk && ReadVec3(*E, TEXT("location_game"), G)) { Loc = GameToUe(G); bOk = true; }
				if (!bOk) { continue; }
				double Area = -1.0;
				if (E->TryGetNumberField(TEXT("area_code"), Area) && Area >= 0.0)
				{
					FBox& Box = D.AreaBoxes.FindOrAdd(int32(Area));
					if (!Box.IsValid) { Box = FBox(EForceInit::ForceInit); }
					Box += Loc;
				}
				if (Type == TEXT("point")) { AddEntry(N, E); }
			}
			for (auto& Pair : D.AreaBoxes) { Pair.Value = Pair.Value.ExpandBy(1500.0); }   // doors/walls sit just outside the points
		}
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION trigger_file %s: %d extra points"), *Full, Added);
		return Added;
	}

	FString CondText(const FMissionCondition& C)
	{
		switch (C.Type)
		{
		case FMissionCondition::EType::Reach: return FString::Printf(TEXT("reach %s r=%.0f"), *C.Location.ToCompactString(), C.Radius);
		case FMissionCondition::EType::TriggerEnter: return FString::Printf(TEXT("enter trigger %s"), *C.Name.ToString());
		case FMissionCondition::EType::TriggerExit: return FString::Printf(TEXT("exit trigger %s"), *C.Name.ToString());
		case FMissionCondition::EType::Interact: return FString::Printf(TEXT("interact %s%s"), C.bActorTag ? TEXT("tag ") : TEXT("trigger "), *C.Name.ToString());
		case FMissionCondition::EType::Event: return FString::Printf(TEXT("event %s"), *C.Name.ToString());
		case FMissionCondition::EType::Timer: return FString::Printf(TEXT("timer %.1fs"), C.Seconds);
		case FMissionCondition::EType::Flag: return FString::Printf(TEXT("flag %s"), *C.Name.ToString());
		case FMissionCondition::EType::All: return TEXT("all-of");
		case FMissionCondition::EType::Any: return TEXT("any-of");
		case FMissionCondition::EType::HasItem: return FString::Printf(TEXT("has_item %s"), *C.Name.ToString());
		case FMissionCondition::EType::Attempt: return FString::Printf(TEXT("attempt %s %d"), C.bAttemptEq ? TEXT("==") : TEXT(">="), C.AttemptN);
		case FMissionCondition::EType::LeaveArea: return FString::Printf(TEXT("leave_area %d"), C.Area);
		default: return TEXT("none");
		}
	}

	/** Recursive event search (the smoke harness finds its smoke_fail exercise even inside an any()/all() fail). */
	bool CondHasEvent(const FMissionCondition& C, FName Ev)
	{
		if (C.Type == FMissionCondition::EType::Event && C.Name == Ev) { return true; }
		for (const FMissionCondition& K : C.Children) { if (CondHasEvent(K, Ev)) { return true; } }
		return false;
	}

	/** Recursive leash search (a chase stage's fail side): the driver follows the fleeing npc instead of teleporting
	 *  to the destination, so the leash never trips mid-route. */
	bool CondLeash(const FMissionCondition& C, FName& Out)
	{
		if (C.Type == FMissionCondition::EType::Leash) { Out = C.Name; return true; }
		for (const FMissionCondition& K : C.Children) { if (CondLeash(K, Out)) { return true; } }
		return false;
	}

	/** System-produced events (speech:<id>, heat:zero, npc_arrived:<id>, npc_ko:<id>, minigame ids) are never
	 *  auto-fired by the smoke driver — it drives the producing system instead. */
	bool SystemEvent(FName Ev) { return Ev.ToString().Contains(TEXT(":")); }

	/** npc move/lead gait: "sprint" | "run" | "walk" (run:true is "run"; default walk). */
	EBullyGait GaitFor(const FString& Gait, bool bRun)
	{
		if (Gait.Equals(TEXT("sprint"), ESearchCase::IgnoreCase)) { return EBullyGait::Sprint; }
		if (Gait.Equals(TEXT("walk"), ESearchCase::IgnoreCase)) { return EBullyGait::Walk; }
		return bRun || Gait.Equals(TEXT("run"), ESearchCase::IgnoreCase) ? EBullyGait::Run : EBullyGait::Walk;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// subsystem lifecycle

UMissionSubsystem* UMissionSubsystem::Get(UWorld* World)
{
	return World ? World->GetSubsystem<UMissionSubsystem>() : nullptr;
}

bool UMissionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer)) { return false; }
	const UWorld* W = Cast<UWorld>(Outer);
	return W && W->IsGameWorld() && W->GetNetMode() != NM_DedicatedServer && !FParse::Param(FCommandLine::Get(), TEXT("BRNoMission"));
}

TStatId UMissionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMissionSubsystem, STATGROUP_Tickables);
}

void UMissionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString Id;
	if (FParse::Value(FCommandLine::Get(), TEXT("-BRMission="), Id))
	{
		PendingAutoStart = Id;
		AutoStartAt = InWorld.GetTimeSeconds() + 1.0;
		bPendingDevOverride = true;   // harness/cmdline start: bypass the campaign availability gate
	}
	else if (!BootMission.IsEmpty())
	{
		// Data-driven default (DefaultGame.ini): a packaged build with no args boots into this mission.
		PendingAutoStart = BootMission;
		AutoStartAt = InWorld.GetTimeSeconds() + 1.0;
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION boot mission '%s' (config BootMission)"), *BootMission);
	}
	FString ShotsId;
	if (FParse::Value(FCommandLine::Get(), TEXT("-BRMissionShots="), ShotsId) && !ShotsId.IsEmpty())
	{
		// Rendered cutscene-composition pass. We own the mission start (after compile settles), so cancel any
		// -BRMission= auto-start that would otherwise fire during shader compilation.
		Shots.bEnabled = true;
		Shots.MissionId = ShotsId;
		PendingAutoStart.Reset();
		if (!FParse::Value(FCommandLine::Get(), TEXT("-BRShotDir="), Shots.Dir) || Shots.Dir.IsEmpty())
		{
			Shots.Dir = FPaths::ProjectSavedDir() / TEXT("MissionShots");
		}
		int32 V = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("-BRShotResX="), V) && V > 0) { Shots.ResX = V; }
		if (FParse::Value(FCommandLine::Get(), TEXT("-BRShotResY="), V) && V > 0) { Shots.ResY = V; }
		IFileManager::Get().MakeDirectory(*Shots.Dir, true);
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSIONSHOT capture armed: mission '%s' -> %s at %dx%d"), *Shots.MissionId, *Shots.Dir, Shots.ResX, Shots.ResY);
	}
	FString SmokeId;
	const bool bSmokeParam = FParse::Param(FCommandLine::Get(), TEXT("BRMissionSmoke")) || FParse::Value(FCommandLine::Get(), TEXT("-BRMissionSmoke="), SmokeId);
	// -BRMissionShotsDrive: let the smoke driver advance the mission during a shots pass so every stage is captured
	// (undriven shots stall at the first interactive stage). The shots pass owns the deferred mission start and the
	// quit; the driver runs drive-current (empty id), never quits, and skips the fail/retry exercise (a retry would
	// re-shoot a stage and inflate the shot count past TotalStages, quitting before the last stage's capture).
	const bool bShotsDrive = Shots.bEnabled && FParse::Param(FCommandLine::Get(), TEXT("BRMissionShotsDrive"));
	if (bSmokeParam && (!Shots.bEnabled || bShotsDrive))
	{
		Smoke.bEnabled = true;
		Smoke.MissionId = bShotsDrive ? TEXT("") : (SmokeId.IsEmpty() ? TEXT("smoke_day_one") : SmokeId);
		Smoke.bWantFailTest = !bShotsDrive && !FParse::Param(FCommandLine::Get(), TEXT("BRMissionSmokeNoFail"));
		Smoke.bNoQuit = bShotsDrive;
		Smoke.bWrongWay = FParse::Param(FCommandLine::Get(), TEXT("BRMissionSmokeWrongWay"));
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION smoke harness armed: mission '%s' (fail/retry test %s, wrong-way %s, shots-drive %s)"), *Smoke.MissionId, Smoke.bWantFailTest ? TEXT("on") : TEXT("off"), Smoke.bWrongWay ? TEXT("on") : TEXT("off"), bShotsDrive ? TEXT("on") : TEXT("off"));
	}
}

void UMissionSubsystem::Deinitialize()
{
	if (ACameraActor* Cam = CutsceneCam.Get()) { Cam->Destroy(); CutsceneCam = nullptr; }
	DestroyOwnedTriggers();
	DespawnMissionNpcs(TEXT("world teardown"));
	DespawnWaves();
	Spotters.Reset();
	DespawnDefenders();
	Follows.Reset();
	Countdown.bActive = false;
	Race.bActive = false;
	ClockFail.bActive = false;
	Lesson.bActive = false;
	Choice.bActive = false;
	BossBarState.Boss.Reset();
	if (Hud.IsValid()) { Hud->Hide(); Hud.Reset(); }
	Super::Deinitialize();
}

APawn* UMissionSubsystem::GetPlayerPawn() const
{
	return GetWorld() ? UGameplayStatics::GetPlayerPawn(GetWorld(), 0) : nullptr;
}

void UMissionSubsystem::TeleportPlayer(const FVector& Loc, float Yaw, bool bSetYaw)
{
	APawn* Pawn = GetPlayerPawn();
	if (!Pawn) { return; }
	Pawn->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
	if (ACharacter* C = Cast<ACharacter>(Pawn))
	{
		if (C->GetCharacterMovement()) { C->GetCharacterMovement()->StopMovementImmediately(); }
	}
	if (bSetYaw)
	{
		if (AController* Ctl = Pawn->GetController()) { Ctl->SetControlRotation(FRotator(0.f, Yaw, 0.f)); }
	}
}

// ---------------------------------------------------------------------------------------------------------------
// loading

TArray<FString> UMissionSubsystem::ListMissionIds()
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(MissionDir(), TEXT("*.json")), true, false);
	for (FString& F : Files) { F = FPaths::GetBaseFilename(F); }
	return Files;
}

bool UMissionSubsystem::LoadMission(const FString& Id, FMissionDef& D, FString& Err) const
{
	const FString Path = FPaths::Combine(MissionDir(), Id + TEXT(".json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { Err = FString::Printf(TEXT("cannot read %s"), *Path); return false; }
	FObj Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { Err = FString::Printf(TEXT("%s is not valid JSON"), *Path); return false; }

	D.Id = Id;
	Root->TryGetStringField(TEXT("id"), D.Id);
	D.Title = D.Id;
	Root->TryGetStringField(TEXT("title"), D.Title);
	FString S;
	if (Root->TryGetStringField(TEXT("retry"), S)) { D.Retry = S == TEXT("mission") ? FMissionDef::ERetry::Mission : S == TEXT("none") ? FMissionDef::ERetry::None : FMissionDef::ERetry::Stage; }
	double N;
	if (Root->TryGetNumberField(TEXT("fail_delay"), N)) { D.FailDelay = float(N); }
	if (Root->TryGetStringField(TEXT("requires_item"), S)) { D.RequiresItem = FName(*S); }
	Root->TryGetBoolField(TEXT("requires_consume"), D.bRequiresConsume);

	if (const FObj* Start = nullptr; Root->TryGetObjectField(TEXT("start"), Start))
	{
		if (ReadLocation(**Start, D, D.StartLocation)) { D.bHasStart = true; }
		if ((*Start)->TryGetNumberField(TEXT("yaw"), N)) { D.StartYaw = float(N); }
	}

	// points: inline first, then the optional trigger file (inline wins)
	if (const FObj* Pts = nullptr; Root->TryGetObjectField(TEXT("points"), Pts))
	{
		for (const auto& Pair : (*Pts)->Values)
		{
			FVector V;
			if (const TArray<TSharedPtr<FJsonValue>>* A = nullptr; Pair.Value->TryGetArray(A) && A->Num() >= 3) { D.Points.Add(FName(*Pair.Key), FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber())); }
			else if (FObj PO = Pair.Value->AsObject(); PO.IsValid() && ReadLocation(*PO, D, V)) { D.Points.Add(FName(*Pair.Key), V); }
			else { Err = FString::Printf(TEXT("point \"%s\" has no location"), *Pair.Key); return false; }
		}
	}
	if (Root->TryGetStringField(TEXT("trigger_file"), S)) { LoadTriggerFile(S, D); }

	if (const TArray<TSharedPtr<FJsonValue>>* Trs = nullptr; Root->TryGetArrayField(TEXT("triggers"), Trs))
	{
		for (const TSharedPtr<FJsonValue>& V : *Trs)
		{
			FObj O = V->AsObject();
			FMissionTriggerDef T;
			if (!O.IsValid() || !O->TryGetStringField(TEXT("id"), S)) { Err = TEXT("trigger missing \"id\""); return false; }
			T.Id = FName(*S);
			if (!ReadLocation(*O, D, T.Center)) { Err = FString::Printf(TEXT("trigger \"%s\" has no center_cm"), *S); return false; }
			ReadVec3(*O, TEXT("half_extent_cm"), T.HalfExtent);
			if (O->TryGetNumberField(TEXT("yaw"), N)) { T.Yaw = float(N); }
			O->TryGetBoolField(TEXT("interact"), T.bInteract);
			O->TryGetStringField(TEXT("prompt"), T.Prompt);
			D.Triggers.RemoveAll([&](const FMissionTriggerDef& E) { return E.Id == T.Id; });   // inline overrides file-derived
			D.Triggers.Add(T);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Stages = nullptr;
	if (!Root->TryGetArrayField(TEXT("stages"), Stages) || Stages->Num() == 0) { Err = TEXT("mission has no \"stages\""); return false; }
	int32 Index = 0;
	for (const TSharedPtr<FJsonValue>& V : *Stages)
	{
		++Index;
		FObj O = V->AsObject();
		FMissionStage St;
		if (!O.IsValid()) { Err = FString::Printf(TEXT("stage %d is not an object"), Index); return false; }
		St.Id = O->TryGetStringField(TEXT("id"), S) ? FName(*S) : FName(*FString::Printf(TEXT("stage%d"), Index));
		O->TryGetStringField(TEXT("objective"), St.Objective);
		// blip: branch on the JSON value's real type. (Do NOT probe with TryGetBoolField first: a string like
		// "office_trigger" parses as bool false via FString::ToBool, which would silently drop the waypoint.)
		//   array [x,y,z] -> explicit cm   |   false -> no blip, true -> auto   |   "point/trigger id" -> explicit   |   absent -> auto
		if (TSharedPtr<FJsonValue> BlipVal = O->TryGetField(TEXT("blip")))
		{
			if (BlipVal->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>>& BA = BlipVal->AsArray();
				if (BA.Num() < 3) { Err = FString::Printf(TEXT("stage %d: blip array needs 3 numbers"), Index); return false; }
				St.BlipMode = FMissionStage::EBlip::Explicit;
				St.BlipLocation = FVector(BA[0]->AsNumber(), BA[1]->AsNumber(), BA[2]->AsNumber());
			}
			else if (BlipVal->Type == EJson::Boolean)
			{
				St.BlipMode = BlipVal->AsBool() ? FMissionStage::EBlip::Auto : FMissionStage::EBlip::None;
			}
			else if (BlipVal->Type == EJson::String)
			{
				const FString BlipName = BlipVal->AsString();
				FVector BlipV; FJsonObject Tmp; Tmp.SetStringField(TEXT("point"), BlipName);
				if (!ReadLocation(Tmp, D, BlipV)) { Err = FString::Printf(TEXT("stage %d: blip \"%s\" is not a point/trigger"), Index, *BlipName); return false; }
				St.BlipMode = FMissionStage::EBlip::Explicit; St.BlipLocation = BlipV;
			}
			else { Err = FString::Printf(TEXT("stage %d: blip must be an array, bool, or point/trigger name"), Index); return false; }
		}
		const FObj* CO = nullptr;
		if (!O->TryGetObjectField(TEXT("complete"), CO)) { Err = FString::Printf(TEXT("stage %d (%s) has no \"complete\" condition"), Index, *St.Id.ToString()); return false; }
		if (!ParseCondition(*CO, D, St.Complete, Err)) { Err = FString::Printf(TEXT("stage %d complete: %s"), Index, *Err); return false; }
		if (O->TryGetObjectField(TEXT("fail"), CO) && !ParseCondition(*CO, D, St.Fail, Err)) { Err = FString::Printf(TEXT("stage %d fail: %s"), Index, *Err); return false; }
		O->TryGetStringField(TEXT("fail_text"), St.FailText);
		if (O->TryGetObjectField(TEXT("skip_if"), CO) && !ParseCondition(*CO, D, St.SkipIf, Err)) { Err = FString::Printf(TEXT("stage %d skip_if: %s"), Index, *Err); return false; }   // sm-cutscenes
		if (O->TryGetObjectField(TEXT("checkpoint"), CO))
		{
			St.bHasCheckpoint = ReadLocation(**CO, D, St.CheckpointLocation);
			if ((*CO)->TryGetNumberField(TEXT("yaw"), N)) { St.CheckpointYaw = float(N); }
		}
		if (!ParseActions(O, TEXT("on_start"), D, St.OnStart, Err) || !ParseActions(O, TEXT("on_complete"), D, St.OnComplete, Err))
		{
			Err = FString::Printf(TEXT("stage %d: %s"), Index, *Err); return false;
		}
		D.Stages.Add(MoveTemp(St));
	}
	// optional root-level fail: evaluated every tick of every stage (e.g. leave_area for a mission set in one building)
	if (const FObj* RF = nullptr; Root->TryGetObjectField(TEXT("fail"), RF))
	{
		if (!ParseCondition(*RF, D, D.RootFail, Err)) { Err = FString::Printf(TEXT("mission fail: %s"), *Err); return false; }
		Root->TryGetStringField(TEXT("fail_text"), D.RootFailText);
	}
	if (!ParseActions(Root, TEXT("on_complete"), D, D.OnComplete, Err)) { return false; }
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
// control

bool UMissionSubsystem::StartMission(const FString& Id, int32 StartStage, bool bDevOverride)
{
	UWorld* World = GetWorld();
	if (!World) { return false; }
	if (State != EState::Idle) { StopMission(TEXT("restarted")); }

	// Campaign spine: a tabled mission only starts when the campaign says it is available (chapter, prereqs, clock
	// window, not already completed). Harnesses and the dev console bypass the gate via bDevOverride.
	UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(World);
	if (Camp && !bDevOverride && !Camp->IsAvailable(Id))
	{
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION start '%s' refused: not available in the campaign (use Mission.Start / -BRMission= to override)"), *Id);
		return false;
	}
	TSharedPtr<FMissionDef> D = MakeShared<FMissionDef>();
	FString Err;
	if (!LoadMission(Id, *D, Err))
	{
		UE_LOG(LogBullyMission, Error, TEXT("BR_MISSION start '%s' FAILED to load: %s"), *Id, *Err);
		return false;
	}
	// mission_start_item_gate: the player must carry the required item to start (2_G2's flowers). A refused start
	// is soft — a hint and no fail (the original ends with MissionFail(false,false), not retryable). Harnesses/dev
	// bypass with the same bDevOverride as the availability gate above.
	if (!bDevOverride && !D->RequiresItem.IsNone())
	{
		UBullyProgressSubsystem* Prog = UBullyProgressSubsystem::Get(World);
		if (Prog && Prog->GetItemCount(D->RequiresItem) <= 0)
		{
			EnsureHud();
			if (Hud.IsValid()) { Hud->ShowHint(FString::Printf(TEXT("You need %s for this."), *D->RequiresItem.ToString()), 4.f); }
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION start '%s' refused: requires item '%s'"), *Id, *D->RequiresItem.ToString());
			return false;
		}
		if (Prog && D->bRequiresConsume) { Prog->TakeItem(D->RequiresItem, 1); }
	}
	if (Camp) { Camp->NoteAttempt(Id); }
	Mission = D;
	Attempt = 1;
	Flags.Reset(); MissionEvents.Reset(); StageEvents.Reset();
	StartStage = FMath::Clamp(StartStage, 0, D->Stages.Num() - 1);

	if (D->bHasStart) { TeleportPlayer(D->StartLocation, D->StartYaw, true); }
	if (APawn* P = GetPlayerPawn())
	{
		MissionStartPawnLoc = P->GetActorLocation();
		MissionStartYaw = P->GetActorRotation().Yaw;
	}
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION start id=%s title='%s' stages=%d triggers=%d points=%d retry=%s from stage %d"),
		*D->Id, *D->Title, D->Stages.Num(), D->Triggers.Num(), D->Points.Num(),
		D->Retry == FMissionDef::ERetry::Stage ? TEXT("stage") : D->Retry == FMissionDef::ERetry::Mission ? TEXT("mission") : TEXT("none"), StartStage + 1);

	SpawnTriggers();
	EnsureHud();
	BeginStage(StartStage, false);
	return true;
}

void UMissionSubsystem::StopMission(const FString& Reason)
{
	if (State == EState::Idle) { return; }
	const bool bWasDone = State == EState::Complete;
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION stop id=%s (%s)"), Mission.IsValid() ? *Mission->Id : TEXT("?"), *Reason);
	ReleaseCutsceneCamera(0.f);   // never leave gameplay looking through a cutscene camera
	StopCutscene();               // sm-cutscenes
	DestroyOwnedTriggers();
	DespawnMissionNpcs(*Reason);
	DespawnWaves();
	Spotters.Reset();
	DespawnDefenders();
	Follows.Reset();
	Countdown.bActive = false;
	Race.bActive = false;
	ClockFail.bActive = false;
	Lesson.bActive = false;
	Choice.bActive = false;
	BossBarState.Boss.Reset();
	if (Hud.IsValid()) { Hud->ClearChoice(); }
	Speech.bActive = false;
	Minigame = FMinigameState();
	HeatPoints = 0.f;
	if (Hud.IsValid()) { Hud->SetCountdown(-1.f); Hud->SetProgress(-1.f); }
	if (Hud.IsValid()) { Hud->ClearObjective(); Hud->Hide(); }
	State = EState::Idle;
	if (Mission.IsValid() && !bWasDone) { OnMissionFinished.Broadcast(*Mission, false); }
}

bool UMissionSubsystem::SkipToStage(const FString& Arg)
{
	if (!Mission.IsValid() || State == EState::Idle) { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION skip: no active mission")); return false; }
	int32 Target = INDEX_NONE;
	if (Arg.Equals(TEXT("next"), ESearchCase::IgnoreCase) || Arg.IsEmpty()) { Target = StageIndex + 1; }
	else if (Arg.IsNumeric()) { Target = FCString::Atoi(*Arg) - 1; }
	else { Target = Mission->Stages.IndexOfByPredicate([&](const FMissionStage& S) { return S.Id == FName(*Arg); }); }
	if (Target < 0 || Target > Mission->Stages.Num()) { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION skip: bad stage '%s'"), *Arg); return false; }
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION skip -> stage %d"), Target + 1);
	if (Target == Mission->Stages.Num()) { CompleteMission(); return true; }
	State = EState::Running;
	BeginStage(Target, false);
	return true;
}

void UMissionSubsystem::FailMission(const FString& Reason)
{
	if (State != EState::Running || !Mission.IsValid()) { return; }
	const FMissionStage& St = Mission->Stages[StageIndex];
	const FString Why = St.FailText.IsEmpty() ? Reason : St.FailText;   // a stage can carry a keyed fail line
	State = EState::Failed;
	FailTime = GetWorld()->GetTimeSeconds();
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION stage %d: %s -> FAIL (%s) attempt %d"), StageIndex + 1, *St.Objective, *Why, Attempt);
	if (Hud.IsValid())
	{
		Hud->SetBlip(false); Hud->SetPrompt(TEXT(""));
		Hud->SetCountdown(-1.f); Hud->SetProgress(-1.f);
		Hud->ShowBanner(TEXT("MISSION FAILED"), Why, Mission->FailDelay + 0.5f, true);
	}
}

void UMissionSubsystem::RetryNow()
{
	if (!Mission.IsValid()) { return; }
	if (Mission->Retry == FMissionDef::ERetry::None) { StopMission(TEXT("failed, retry disabled")); return; }
	StopCutscene();   // sm-cutscenes
	++Attempt;
	// campaign attempt counting is retry-aware (the original's attempt count gates first-attempt tutorial cards)
	if (UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(GetWorld())) { Camp->NoteAttempt(Mission->Id); }
	// defenders revive on retry (a KO'd escortee would otherwise re-fail instantly on the latched npc_ko event)
	for (auto& Pair : Defenders)
	{
		if (ABullyCombatPawn* D = Pair.Value.Pawn.Get())
		{
			D->SetActorLocation(Pair.Value.SpawnAt + FVector(0, 0, 20.f), false, nullptr, ETeleportType::TeleportPhysics);
			if (D->GetCombat()) { D->GetCombat()->Revive(); }
		}
		const FName Ev(*FString::Printf(TEXT("npc_ko:%s"), *Pair.Key.ToString()));
		StageEvents.Remove(Ev);
		MissionEvents.Remove(Ev);
	}
	// a player-KO fail (1_B) puts him back on his feet for the retry
	if (ABullyCharacter* P = Cast<ABullyCharacter>(GetPlayerPawn()); P && P->GetCombat() && P->GetCombat()->IsOut()) { P->GetCombat()->Revive(); }
	if (Mission->Retry == FMissionDef::ERetry::Stage)
	{
		const FMissionStage& St = Mission->Stages[StageIndex];
		const FVector At = St.bHasCheckpoint ? St.CheckpointLocation : StageStartPawnLoc;
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION retry attempt %d: reload stage %d (%s) at %s"), Attempt, StageIndex + 1, St.bHasCheckpoint ? TEXT("checkpoint") : TEXT("stage-start pos"), *At.ToCompactString());
		TeleportPlayer(At, St.CheckpointYaw, St.bHasCheckpoint);
		State = EState::Running;
		BeginStage(StageIndex, true);
	}
	else
	{
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION retry attempt %d: restart mission at %s"), Attempt, *MissionStartPawnLoc.ToCompactString());
		Flags.Reset(); MissionEvents.Reset();
		TeleportPlayer(MissionStartPawnLoc, MissionStartYaw, true);
		State = EState::Running;
		BeginStage(0, true);
	}
}

void UMissionSubsystem::SetFlag(FName Flag, bool bOn)
{
	if (bOn) { Flags.Add(Flag); } else { Flags.Remove(Flag); }
}

void UMissionSubsystem::FireEvent(FName Event)
{
	StageEvents.Add(Event);
	MissionEvents.Add(Event);
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION event '%s'"), *Event.ToString());
	OnMissionEvent.Broadcast(Event);
}

// ---------------------------------------------------------------------------------------------------------------
// stage machine

void UMissionSubsystem::BeginStage(int32 Index, bool bRetry)
{
	if (!Mission.IsValid()) { return; }
	// shots: the stage that just ended never reached its settle delay (a fast driver beat it) -- capture its end
	// state now, before this stage's teardown/actions change the scene.
	if (Shots.bEnabled && Shots.PendingAt >= 0.0) { FirePendingShot(); }
	if (Index >= Mission->Stages.Num()) { CompleteMission(); return; }
	StageIndex = Index;
	State = EState::Running;
	// sm-cutscenes: skip_if -- a stage an original cutscene already covered (e.g. the arrival text beats) is skipped outright
	if (Mission->Stages[Index].SkipIf.IsValid() && Evaluate(Mission->Stages[Index].SkipIf))
	{
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION stage %d/%d: %s -> skipped (skip_if: %s)"), Index + 1, Mission->Stages.Num(),
			*Mission->Stages[Index].Id.ToString(), *CondText(Mission->Stages[Index].SkipIf));
		if (Smoke.bEnabled) { Smoke.StagesSeen.AddUnique(Index); }
		BeginStage(Index + 1, false);
		return;
	}
	const FMissionStage& St = Mission->Stages[Index];
	StageStartTime = GetWorld()->GetTimeSeconds();
	StageEvents.Reset();
	// chapter-1 primitives are stage-scoped: a new stage disarms a leftover minigame/speech beat (its own on_start re-arms)
	Minigame = FMinigameState();
	Speech.bActive = false;
	Lesson.bActive = false;   // the lesson is stage-scoped (a retry re-arms it from on_start)
	Choice.bActive = false;   // a dialogue choice is stage-scoped too
	BossBarState.Boss.Reset();
	if (Hud.IsValid()) { Hud->ClearChoice(); }
	DespawnWaves();   // a wave/spotter belongs to its stage
	Spotters.Reset();
	LeashFarSince = -1.0;
	ZoneWasInside = false;
	ZoneOutSince = -1.0;
	Race.bActive = false;   // a race is stage-scoped (a retry re-arms it from on_start)
	if (Hud.IsValid()) { Hud->SetProgress(-1.f); Hud->SetCountdown(-1.f); }
	if (APawn* P = GetPlayerPawn()) { StageStartPawnLoc = P->GetActorLocation(); }

	// actors for actor_tag interact conditions
	RefreshStageActors();

	if (Hud.IsValid())
	{
		Hud->SetObjective(Mission->Title, Index + 1, Mission->Stages.Num(), St.Objective);
		Hud->SetPrompt(TEXT(""));
	}
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION stage %d/%d: %s -> begin%s [complete: %s%s]"), Index + 1, Mission->Stages.Num(), *St.Objective,
		bRetry ? TEXT(" (retry)") : TEXT(""), *CondText(St.Complete), St.Fail.IsValid() ? *FString::Printf(TEXT("; fail: %s"), *CondText(St.Fail)) : TEXT(""));
	RunActions(St.OnStart);
	OnStageBegan.Broadcast(*Mission, Index);

	if (Shots.bEnabled && Index < Shots.TotalStages && Shots.Count < Shots.TotalStages)
	{
		// Shoot this beat once the camera has finished blending and Lumen has settled. One shot per stage, newest wins.
		// (TotalStages/Count also fence this off from any mission the captured one chains into on completion.)
		Shots.PendingStage = Index;
		Shots.PendingAt = GetWorld()->GetTimeSeconds() + Shots.ShotDelay;
		Shots.PendingName = FString::Printf(TEXT("%02d_%s"), Index, St.Id.IsNone() ? TEXT("stage") : *St.Id.ToString());
	}
}

void UMissionSubsystem::CompleteStage()
{
	if (!Mission.IsValid() || State != EState::Running) { return; }
	const FMissionStage& St = Mission->Stages[StageIndex];
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION stage %d: %s -> complete"), StageIndex + 1, *St.Objective);
	RunActions(St.OnComplete);
	OnStageCompleted.Broadcast(*Mission, StageIndex);
	if (StageIndex + 1 >= Mission->Stages.Num()) { CompleteMission(); }
	else { BeginStage(StageIndex + 1, false); }
}

void UMissionSubsystem::CompleteMission()
{
	if (!Mission.IsValid()) { return; }
	State = EState::Complete;
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION complete id=%s stages=%d attempts=%d"), *Mission->Id, Mission->Stages.Num(), Attempt);
	// A mission that chains straight into another (its on_complete starts a mission) is a beat in a longer sequence
	// (e.g. a cutscene handing off to gameplay), so skip the "MISSION COMPLETE" end-card and let the hand-off read clean.
	bool bChains = false;
	for (const FMissionAction& A : Mission->OnComplete) { if (A.Type == FMissionAction::EType::StartMission) { bChains = true; break; } }
	if (Hud.IsValid())
	{
		Hud->SetBlip(false); Hud->SetPrompt(TEXT(""));
		if (!bChains) { Hud->ShowBanner(TEXT("MISSION COMPLETE"), Mission->Title, 4.f, false); }
	}
	RunActions(Mission->OnComplete);
	// Campaign spine: the pass side (SMissPass-style on_pass) + completion count. Campaign absent = no-op.
	if (UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(GetWorld())) { Camp->NotifyPassed(Mission->Id); }
	// A terminal mission returns to gameplay, so never leave a cutscene camera live (a chaining one hands off via
	// StopMission, which releases it there). If on_complete deliberately set a camera, that's respected on the chain.
	if (!bChains) { ReleaseCutsceneCamera(0.6f); }
	DestroyOwnedTriggers();
	// mission npcs of a terminal mission clean up here; a chaining mission's npcs live until the next StartMission's
	// StopMission (brief overlap is deliberate: outro walk-offs read through the hand-off)
	if (!bChains) { DespawnMissionNpcs(TEXT("complete")); }
	DespawnWaves();
	Spotters.Reset();
	DespawnDefenders();
	Follows.Reset();
	Countdown.bActive = false;
	Race.bActive = false;
	ClockFail.bActive = false;
	Lesson.bActive = false;
	Choice.bActive = false;
	BossBarState.Boss.Reset();
	if (Hud.IsValid()) { Hud->ClearChoice(); }
	Speech.bActive = false;
	Minigame = FMinigameState();
	HeatPoints = 0.f;
	OnMissionFinished.Broadcast(*Mission, true);
}

void UMissionSubsystem::RunActions(const TArray<FMissionAction>& Actions)
{
	for (const FMissionAction& A : Actions)
	{
		switch (A.Type)
		{
		case FMissionAction::EType::Hint: if (Hud.IsValid()) { Hud->ShowHint(A.Text, A.Seconds); } break;
		case FMissionAction::EType::Banner: if (Hud.IsValid()) { Hud->ShowBanner(A.Text, A.Sub, A.Seconds, false); } break;
		case FMissionAction::EType::Event: FireEvent(A.Name); break;
		case FMissionAction::EType::SetFlag: SetFlag(A.Name, true); break;
		case FMissionAction::EType::ClearFlag: SetFlag(A.Name, false); break;
		case FMissionAction::EType::Teleport: TeleportPlayer(A.Location, A.Yaw, true); break;
		case FMissionAction::EType::Log: UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION log: %s"), *A.Text); break;
		case FMissionAction::EType::StartMission:
			// Defer to Tick (via the auto-start slot) so we never re-enter the stage machine from inside an action.
			if (UWorld* W = GetWorld())
			{
				PendingAutoStart = A.Text;
				AutoStartAt = W->GetTimeSeconds() + FMath::Max(0.f, A.Seconds);
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION start_mission '%s' in %.1fs"), *A.Text, A.Seconds);
			}
			break;
		case FMissionAction::EType::SpawnEnemy:
		{
			ABullyCharacter* Player = Cast<ABullyCharacter>(GetPlayerPawn());
			if (!Player) { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION spawn_enemy: no player pawn")); break; }
			ABullyCombatPawn* E = UBullyCombatTest::SpawnEnemyInFront(GetWorld(), Player, A.Dist, A.HealthFloor, A.EnemyMesh);
			if (E)
			{
				if (A.GateAttack >= 0) { E->bAttackAllowed = A.GateAttack != 0; }
				if (A.GateBlock >= 0) { E->bBlockAllowed = A.GateBlock != 0; }
			}
			if (A.PlayerInvuln >= 0 && Player->GetCombat()) { Player->GetCombat()->bInvulnerable = A.PlayerInvuln != 0; }
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION spawn_enemy dist=%.0f floor=%.0f invuln=%d atk=%d blk=%d -> %s"),
				A.Dist, A.HealthFloor, A.PlayerInvuln, A.GateAttack, A.GateBlock, E ? *E->GetName() : TEXT("FAILED"));
			break;
		}
		case FMissionAction::EType::Fade:
			// Start the fade now; TickFade drives it (fade-out -> optional teleport at full black -> fade-in),
			// so the stage machine keeps running underneath (e.g. the end-card banner shows while black).
			if (UWorld* W = GetWorld())
			{
				FadeOutDur = A.FadeOut; FadeHoldDur = A.FadeHold; FadeInDur = A.FadeIn;
				bFadePendingTeleport = A.bFadeTeleport;
				FadeTeleportLoc = A.Location; FadeTeleportYaw = A.Yaw;
				FadePhase = EFadePhase::Out;
				FadePhaseStart = W->GetTimeSeconds();
				SetCameraFade(0.f);
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION fade out=%.2f hold=%.2f in=%.2f teleport=%d"),
					FadeOutDur, FadeHoldDur, FadeInDur, bFadePendingTeleport ? 1 : 0);
			}
			break;
		case FMissionAction::EType::Reward:
		{
			UBullyProgressSubsystem* Prog = UBullyProgressSubsystem::Get(GetWorld());
			if (Prog)
			{
				Prog->AddMoney(A.Money);
				Prog->AddRespect(A.Respect);
				if (Hud.IsValid()) { Hud->SetWallet(Prog->GetMoney(), Prog->GetRespect(), true); }
			}
			// Toast it: explicit text wins, else a generated line from whatever was granted.
			FString Line = A.Text;
			if (Line.IsEmpty())
			{
				TArray<FString> Parts;
				if (A.Money != 0)   { Parts.Add(FString::Printf(TEXT("$%d"), A.Money)); }
				if (A.Respect != 0) { Parts.Add(FString::Printf(TEXT("%d respect"), A.Respect)); }
				Line = Parts.Num() ? FString::Printf(TEXT("Reward: %s"), *FString::Join(Parts, TEXT("  +  "))) : FString();
			}
			if (!Line.IsEmpty() && Hud.IsValid()) { Hud->ShowHint(Line, 4.f); }
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION reward money=%d respect=%d"), A.Money, A.Respect);
			break;
		}
		case FMissionAction::EType::Letterbox:
			if (Hud.IsValid()) { Hud->SetLetterbox(A.bOn); }
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION letterbox %s"), A.bOn ? TEXT("on") : TEXT("off"));
			break;
		case FMissionAction::EType::Say:
			if (Hud.IsValid()) { Hud->Say(A.Sub, A.Text, A.Seconds); }
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION say %s: %s"), A.Sub.IsEmpty() ? TEXT("-") : *A.Sub, *A.Text);
			break;
		case FMissionAction::EType::Camera:
			ApplyCamera(A);
			break;
		case FMissionAction::EType::Shake:
			ApplyShake(A);
			break;
		case FMissionAction::EType::GiveWeapon:
		case FMissionAction::EType::GiveAmmo:
			if (UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(GetWorld()))
			{
				if (A.Type == FMissionAction::EType::GiveWeapon)
				{
					if (A.WeaponId >= 0) { Camp->GiveWeaponByGameId(A.WeaponId); }
					else { Camp->GiveWeaponByName(FName(*A.Text)); }
				}
				else
				{
					if (A.WeaponId >= 0) { Camp->GiveAmmoByGameId(A.WeaponId, A.Amount); }
					else { Camp->GiveAmmoByName(FName(*A.Text), A.Amount); }
				}
			}
			break;
		case FMissionAction::EType::SetRespect:
			if (UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(GetWorld())) { Camp->SetRespect(A.Name, A.Amount); }
			break;
		case FMissionAction::EType::Clock:
			if (UBullyClockSubsystem* Clock = UBullyClockSubsystem::Get(GetWorld()))
			{
				FString Hs, Ms;
				if (!A.Text.IsEmpty() && A.Text.Split(TEXT(":"), &Hs, &Ms)) { Clock->SetClock(FCString::Atoi(*Hs), FCString::Atoi(*Ms)); }
				else if (A.Amount == 1) { Clock->PauseClock(); }
				else if (A.Amount == 2) { Clock->UnpauseClock(); }
			}
			break;
		case FMissionAction::EType::ChapterSet:
			if (UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(GetWorld())) { Camp->SetChapter(A.Amount); }
			break;
		case FMissionAction::EType::ForceComplete:
			if (UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(GetWorld())) { Camp->NotifyPassed(A.Name.ToString()); }
			break;
		case FMissionAction::EType::SpawnNpc:
			SpawnMissionNpc(A);
			break;
		case FMissionAction::EType::NpcMove:
		case FMissionAction::EType::NpcLead:
		{
			ABullyScriptedNPC* N = NpcById(A.NpcId);
			if (!N) { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION %s: unknown npc '%s'"), A.Type == FMissionAction::EType::NpcLead ? TEXT("npc_lead") : TEXT("npc_move_to"), *A.NpcId.ToString()); break; }
			const FName Ev = !A.Sub.IsEmpty() ? FName(*A.Sub) : FName(*FString::Printf(TEXT("npc_arrived:%s"), *A.NpcId.ToString()));
			// route: "via" names (points OR trigger-file paths) + a "path" between the npc and the goal
			TArray<FVector> Route;
			for (const FString& V : A.Via)
			{
				if (const FVector* P = Mission->Points.Find(FName(*V))) { Route.Add(*P); }
				else if (const TArray<FVector>* Pth = Mission->Paths.Find(FName(*V))) { Route.Append(*Pth); }
				else { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION %s: unknown via point/path '%s'"), *A.NpcId.ToString(), *V); }
			}
			if (!A.Text.IsEmpty())
			{
				if (const TArray<FVector>* P = Mission->Paths.Find(FName(*A.Text))) { Route.Append(*P); }
				else { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION %s: unknown path '%s'"), *A.NpcId.ToString(), *A.Text); }
			}
			if (A.Type == FMissionAction::EType::NpcLead)
			{
				// lead the player: walk the route in legs, pausing while the player trails (see TickLeads)
				FLeadOrder L;
				L.Npc = N;
				L.Wps = Route;
				L.Goal = A.Location;
				L.Radius = A.Dist > 0.f ? A.Dist : 600.f;
				L.Event = Ev;
				L.bRun = A.bRun;
				L.Gait = A.Gait;
				Leads.RemoveAll([&](const FLeadOrder& E) { return E.Npc.Get() == N; });
				Leads.Add(L);
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION npc_lead %s -> %s via %d waypoint(s) (event %s)"), *A.NpcId.ToString(), *A.Location.ToCompactString(), Route.Num(), *Ev.ToString());
			}
			else
			{
				N->ClearCommands(true);
				if (Route.Num() > 0)
				{
					Route.Add(A.Location);
					N->FollowPath(Route, GaitFor(A.Gait, A.bRun), 70.f, FString::Printf(TEXT("mission %s move_to"), *A.NpcId.ToString()));
				}
				else
				{
					N->GoToPoint(A.Location, GaitFor(A.Gait, A.bRun), 70.f, FString::Printf(TEXT("mission %s move_to"), *A.NpcId.ToString()));
				}
				PendingArrive.Add(N, Ev);
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION npc_move_to %s -> %s via %d waypoint(s) (event %s)"), *A.NpcId.ToString(), *A.Location.ToCompactString(), Route.Num(), *Ev.ToString());
			}
			break;
		}
		case FMissionAction::EType::NpcFace:
			if (ABullyScriptedNPC* N = NpcById(A.NpcId))
			{
				if (A.bFacePlayer) { N->FacePlayer(true); }
				else { N->FaceLocation(A.Location, false); }
			}
			break;
		case FMissionAction::EType::NpcDespawn:
			if (ABullyScriptedNPC* N = NpcById(A.NpcId))
			{
				Leads.RemoveAll([&](const FLeadOrder& E) { return E.Npc.Get() == N; });
				PendingArrive.Remove(N);
				Npcs.Remove(A.NpcId);
				N->Despawn();
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION npc_despawn %s"), *A.NpcId.ToString());
			}
			break;
		case FMissionAction::EType::PairAnim:
		{
			// v1 stub: the two face each other and the NPC plays a generic clip; the paired give/receive anims are
			// action-tree work for a later wave.
			ABullyScriptedNPC* N = NpcById(A.NpcId);
			APawn* P = GetPlayerPawn();
			if (!N || !P) { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION pair_anim: unknown npc '%s' or no player"), *A.NpcId.ToString()); break; }
			N->ClearCommands(true);
			N->FacePlayer(true);
			const FVector D = N->GetActorLocation() - P->GetActorLocation();
			TeleportPlayer(P->GetActorLocation(), FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), true);
			TArray<FString> Clips;
			FString(A.Text).ParseIntoArray(Clips, TEXT(","), true);
			if (Clips.Num() > 0) { N->PlayGesture(Clips, false, 0.f); }
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION pair_anim %s (%.1fs, stub)"), *A.NpcId.ToString(), A.Seconds);
			break;
		}
		case FMissionAction::EType::SpeechSequence:
			Speech.Lines = A.Lines;
			Speech.DoneEvent = FName(*FString::Printf(TEXT("speech:%s"), *A.Name.ToString()));
			Speech.Index = 0;
			Speech.LineEndsAt = 0.0;
			Speech.bActive = Speech.Lines.Num() > 0;
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION speech_sequence '%s': %d line(s)"), *A.Name.ToString(), Speech.Lines.Num());
			if (!Speech.bActive) { FireEvent(Speech.DoneEvent); }
			break;
		case FMissionAction::EType::MinigameLaunch:
			Minigame.bArmed = true;
			Minigame.bRunning = false;
			Minigame.Progress = 0.f;
			Minigame.At = A.Location;
			Minigame.Radius = A.Radius;
			Minigame.Seconds = FMath::Max(0.5f, A.Seconds);
			Minigame.Success = A.SuccessEvent;
			Minigame.Fail = A.FailEvent;
			Minigame.Label = A.Text;
			if (Hud.IsValid()) { Hud->SetProgress(0.f, Minigame.Label); }
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION minigame_launch '%s' at %s (%.1fs hold)"), *A.Name.ToString(), *A.Location.ToCompactString(), Minigame.Seconds);
			break;
		case FMissionAction::EType::GiveItem:
		case FMissionAction::EType::TakeItem:
			if (UBullyProgressSubsystem* Prog = UBullyProgressSubsystem::Get(GetWorld()))
			{
				if (A.Type == FMissionAction::EType::GiveItem) { Prog->AddItem(A.Name, A.Amount); }
				else { Prog->TakeItem(A.Name, A.Amount); }
			}
			break;
		case FMissionAction::EType::MoneyEnsure:
			if (UBullyProgressSubsystem* Prog = UBullyProgressSubsystem::Get(GetWorld())) { Prog->EnsureMoney(A.Money); }
			break;
		case FMissionAction::EType::MoneyTake:
			if (UBullyProgressSubsystem* Prog = UBullyProgressSubsystem::Get(GetWorld())) { Prog->TakeMoney(A.Money); }
			break;
		case FMissionAction::EType::HeatSet:
			// punishment-lite stub: heat points that decay only while the player is hidden; fires "heat:zero" at 0
			HeatPoints = float(A.Amount);
			HeatDecay = A.Amp > 0.f ? A.Amp : 25.f;
			SetFlag(TEXT("player_hidden"), false);
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION heat_set %d (decay %.0f/s while hidden)"), A.Amount, HeatDecay);
			break;
		case FMissionAction::EType::Hide:
			SetFlag(TEXT("player_hidden"), A.bOn);
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION hide %s"), A.bOn ? TEXT("on") : TEXT("off"));
			break;
		case FMissionAction::EType::SpawnWave:
			SpawnWave(A);
			break;
		case FMissionAction::EType::Spotter:
			if (ABullyScriptedNPC* N = NpcById(A.NpcId))
			{
				FSpotter S;
				S.Npc = N;
				S.Range = A.Radius;
				S.CosHalf = FMath::Cos(FMath::DegreesToRadians(A.Amp > 0.f ? A.Amp : 60.f) * 0.5f);
				S.Seconds = A.Seconds > 0.f ? A.Seconds : 2.f;
				S.Event = !A.Sub.IsEmpty() ? FName(*A.Sub) : FName(*FString::Printf(TEXT("spotted_by:%s"), *A.NpcId.ToString()));
				Spotters.Add(S);
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION spotter %s: range %.0f cone %.0f deg %.1fs -> event %s"), *A.NpcId.ToString(), S.Range, A.Amp, S.Seconds, *S.Event.ToString());
			}
			else { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION spotter: unknown npc '%s'"), *A.NpcId.ToString()); }
			break;
		case FMissionAction::EType::NpcFollow:
			if (ABullyScriptedNPC* N = NpcById(A.NpcId))
			{
				Follows.RemoveAll([&](const FFollowOrder& E) { return E.Npc.Get() == N; });
				FFollowOrder F;
				F.Npc = N;
				F.Radius = A.Dist > 0.f ? A.Dist : 300.f;
				F.Gait = A.Gait;
				Follows.Add(F);
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION npc_follow %s (stop %.0f cm)"), *A.NpcId.ToString(), F.Radius);
			}
			else { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION npc_follow: unknown npc '%s'"), *A.NpcId.ToString()); }
			break;
		case FMissionAction::EType::CountdownStart:
		{
			const FName Ev = !A.Name.IsNone() ? A.Name : FName(TEXT("countdown:out"));
			// re-arming un-latches a previously fired expiry (a stage retry replays its on_start)
			StageEvents.Remove(Ev);
			MissionEvents.Remove(Ev);
			if (Countdown.bActive && A.bResume)
			{
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION countdown %s: resumed (%.0f s left)"), *Ev.ToString(), Countdown.EndsAt - GetWorld()->GetTimeSeconds());
				break;
			}
			Countdown.bActive = true;
			Countdown.Event = Ev;
			Countdown.bVisible = A.bCountdownVisible;
			Countdown.EndsAt = GetWorld()->GetTimeSeconds() + FMath::Max(1.f, A.Seconds);
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION countdown %s: %.0f s%s"), *Ev.ToString(), A.Seconds, A.bCountdownVisible ? TEXT(" (visible)") : TEXT(""));
			break;
		}
		case FMissionAction::EType::CountdownCancel:
			if (Countdown.bActive)
			{
				Countdown.bActive = false;
				if (Hud.IsValid()) { Hud->SetCountdown(-1.f); }
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION countdown cancelled (%s)"), *Countdown.Event.ToString());
			}
			break;
		case FMissionAction::EType::LessonStart:
			Lesson.bActive = true;
			Lesson.Id = A.Name;
			Lesson.Move = FName(*A.Sub);
			Lesson.Needed = FMath::Max(1, A.Amount);
			Lesson.Count = 0;
			Lesson.Window = A.Seconds > 0.f ? A.Seconds : 10.f;
			Lesson.WindowEndsAt = GetWorld()->GetTimeSeconds() + Lesson.Window;
			Lesson.Prompt = A.Text;
			if (Hud.IsValid())
			{
				Hud->ShowHint(Lesson.Prompt, 4.f);
				Hud->SetProgress(0.f, FString::Printf(TEXT("%s  0 / %d"), *Lesson.Prompt, Lesson.Needed));
			}
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION lesson '%s': perform %s x%d (window %.0f s)"), *Lesson.Id.ToString(), *Lesson.Move.ToString(), Lesson.Needed, Lesson.Window);
			break;
		case FMissionAction::EType::SpawnDefender:
		{
			UWorld* World = GetWorld();
			if (!World) { break; }
			if (ABullyCombatPawn* Existing = DefenderById(A.NpcId))
			{
				// stage retry / re-run: revive and re-position him instead of duplicating
				Existing->SetActorLocation(A.Location + FVector(0, 0, 20.f), false, nullptr, ETeleportType::TeleportPhysics);
				if (Existing->GetCombat()) { Existing->GetCombat()->Revive(); }
				FDefenderState& D = Defenders[A.NpcId];
				D.SpawnAt = A.Location;
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION spawn_defender %s: revived/repositioned (retry)"), *A.NpcId.ToString());
				break;
			}
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			SP.bDeferConstruction = true;
			ABullyCombatPawn* E = World->SpawnActor<ABullyCombatPawn>(ABullyCombatPawn::StaticClass(), A.Location + FVector(0, 0, 20.f), FRotator::ZeroRotator, SP);
			if (!E) { break; }
			if (!A.Mesh.IsEmpty()) { E->PedMeshPath = FString::Printf(TEXT("/Game/Imported/Characters/%s/SK_%s.SK_%s"), *A.Mesh, *A.Mesh, *A.Mesh); }
			E->bStationary = true;             // he holds his ground
			E->bAttackAllowed = false;         // and never swings (a cowering escortee)
			E->Tags.Add(FName(*FString::Printf(TEXT("br_npc:%s"), *A.NpcId.ToString())));   // his KO fires npc_ko:<id>
			E->FinishSpawning(FTransform(FRotator::ZeroRotator, A.Location + FVector(0, 0, 20.f)));
			if (E->GetCombat())
			{
				E->GetCombat()->Team = EBullyCombatTeam::Player;   // enemies may target him; the player's swings skip him
				if (A.Health > 0.f) { E->GetCombat()->SetMaxHealth(A.Health); }
			}
			FDefenderState D;
			D.Pawn = E;
			D.SpawnAt = A.Location;
			D.DrainHp = float(A.Amount);
			D.DrainEvery = A.Seconds > 0.f ? A.Seconds : 5.f;
			D.bDrain = A.Amount > 0;
			Defenders.Add(A.NpcId, D);
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION spawn_defender %s mesh=%s hp=%.0f drain=%d/%.0fs"), *A.NpcId.ToString(), *A.Mesh, A.Health, A.Amount, D.DrainEvery);
			break;
		}
		case FMissionAction::EType::NpcDrain:
			if (FDefenderState* D = Defenders.Find(A.NpcId))
			{
				D->bDrain = A.bOn;
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION npc_drain %s %s"), *A.NpcId.ToString(), A.bOn ? TEXT("on") : TEXT("off"));
			}
			break;
		case FMissionAction::EType::DialogueChoice:
		{
			Choice.bActive = true;
			Choice.Id = A.Name;
			Choice.Options = A.Options;
			if (Hud.IsValid())
			{
				TArray<FString> Labels;
				for (const FChoiceOption& Op : A.Options) { Labels.Add(Op.Label); }
				Hud->SetChoice(A.Text, Labels);
			}
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION dialogue_choice '%s': %d option(s)"), *A.Name.ToString(), A.Options.Num());
			break;
		}
		case FMissionAction::EType::BossBar:
		{
			// track the named one-member wave's health on the HUD progress bar
			BossBarState.Boss.Reset();
			for (const FWaveGroup& G : Waves)
			{
				if (G.Group == A.Name && G.Members.Num() > 0 && G.Members[0].IsValid() && G.Members[0]->GetCombat())
				{
					BossBarState.Boss = G.Members[0];
					BossBarState.Max = G.Members[0]->GetCombat()->MaxHealth;
					break;
				}
			}
			BossBarState.Label = A.Text;
			BossBarState.HalfLine = A.Sub;
			BossBarState.bHalfFired = false;
			if (Hud.IsValid() && BossBarState.Boss.IsValid()) { Hud->SetProgress(1.f, BossBarState.Label); }
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION boss_bar %s (%s, hp %.0f)"), *A.Name.ToString(), *BossBarState.Label, BossBarState.Max);
			break;
		}
		case FMissionAction::EType::RaceStart:
		{
			Race = FRaceState();
			Race.Id = A.Name;
			Race.Laps = FMath::Max(1, A.Amount);
			if (A.Radius > 0.f) { Race.Radius = A.Radius; }
			if (Mission.IsValid())
			{
				if (const TArray<FVector>* Path = Mission->Paths.Find(FName(*A.Sub))) { Race.Wps = *Path; }
			}
			if (Race.Wps.Num() < 2)
			{
				UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION race_start '%s': course '%s' has %d waypoint(s) — race not armed"), *A.Name.ToString(), *A.Sub, Race.Wps.Num());
				break;
			}
			Race.bActive = true;
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION race '%s' armed: %d waypoints x %d lap(s) (checkpoint r %.0f) -> event race:%s_win"),
				*A.Name.ToString(), Race.Wps.Num(), Race.Laps, Race.Radius, *A.Name.ToString());
			break;
		}
		case FMissionAction::EType::ClockFailAt:
		{
			FString Hs, Ms;
			if (!A.Text.Split(TEXT(":"), &Hs, &Ms))
			{
				UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION clock_fail_at: bad time '%s' (want h:mm)"), *A.Text);
				break;
			}
			ClockFail.bActive = true;
			ClockFail.TargetHour = FCString::Atoi(*Hs) + FCString::Atoi(*Ms) / 60.f;
			UBullyClockSubsystem* Clk = UBullyClockSubsystem::Get(GetWorld());
			ClockFail.ArmedHour = Clk ? Clk->GetHourFloat() : 0.f;
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION clock_fail_at %s armed (now %.2f)"), *A.Text, ClockFail.ArmedHour);
			break;
		}
		case FMissionAction::EType::Cutscene: RunCutsceneAction(A); break;   // sm-cutscenes
		}
	}
}

void UMissionSubsystem::ApplyShake(const FMissionAction& A)
{
	if (ABullyCharacter* C = Cast<ABullyCharacter>(GetPlayerPawn()))
	{
		C->AddCameraShake(A.Amp, A.Seconds);
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION shake amp=%.2f dur=%.2f"), A.Amp, A.Seconds);
	}
}

namespace { float FadeSmooth01(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); } }

void UMissionSubsystem::SetCameraFade(float Alpha)
{
	APawn* Pawn = GetPlayerPawn();
	if (!Pawn) { return; }
	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			if (Alpha <= 0.f) { PC->PlayerCameraManager->StopCameraFade(); }
			else { PC->PlayerCameraManager->SetManualCameraFade(FMath::Min(Alpha, 1.f), FLinearColor::Black, /*bFadeAudio*/ false); }
		}
	}
}

void UMissionSubsystem::ApplyCamera(const FMissionAction& A)
{
	UWorld* World = GetWorld();
	APawn* Pawn = GetPlayerPawn();
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!World || !PC) { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION camera: no player controller")); return; }

	if (A.bRelease) { ReleaseCutsceneCamera(A.Blend); return; }

	// aim: at the player, an explicit spot, or (default) keep the camera's current facing by looking slightly ahead
	FVector LookAt = A.Look;
	if (A.bLookPlayer && Pawn) { LookAt = Pawn->GetActorLocation() + FVector(0, 0, 60.f); }
	else if (!A.bHasLook) { LookAt = A.Location + FVector(100.f, 0, 0); }
	const FRotator Rot = (LookAt - A.Location).Rotation();

	ACameraActor* Cam = CutsceneCam.Get();
	if (!Cam)
	{
		FActorSpawnParameters SP; SP.ObjectFlags |= RF_Transient;
		Cam = World->SpawnActor<ACameraActor>(A.Location, Rot, SP);
		CutsceneCam = Cam;
	}
	if (!Cam) { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION camera: spawn failed")); return; }
	Cam->SetActorLocationAndRotation(A.Location, Rot);
	if (A.Fov > 1.f && Cam->GetCameraComponent()) { Cam->GetCameraComponent()->SetFieldOfView(A.Fov); }
	PC->SetViewTargetWithBlend(Cam, A.Blend, VTBlend_Cubic);
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION camera at=%s look=%s fov=%.0f blend=%.2f"),
		*A.Location.ToCompactString(), *LookAt.ToCompactString(), A.Fov, A.Blend);
}

void UMissionSubsystem::ReleaseCutsceneCamera(float Blend)
{
	APawn* Pawn = GetPlayerPawn();
	if (APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		PC->SetViewTargetWithBlend(Pawn, FMath::Max(0.f, Blend), VTBlend_Cubic);
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION camera release -> player (blend=%.2f)"), Blend);
	}
	if (ACameraActor* Cam = CutsceneCam.Get()) { Cam->Destroy(); }
	CutsceneCam = nullptr;
}

void UMissionSubsystem::TickFade(float /*Dt*/)
{
	if (FadePhase == EFadePhase::None) { return; }
	UWorld* World = GetWorld();
	if (!World) { return; }
	const float Elapsed = float(World->GetTimeSeconds() - FadePhaseStart);
	switch (FadePhase)
	{
	case EFadePhase::Out:
	{
		const float Dur = FMath::Max(FadeOutDur, 0.02f);
		if (Elapsed < FadeOutDur) { SetCameraFade(FadeSmooth01(Elapsed / Dur)); break; }
		SetCameraFade(1.f);
		if (bFadePendingTeleport) { TeleportPlayer(FadeTeleportLoc, FadeTeleportYaw, true); bFadePendingTeleport = false; }
		FadePhase = EFadePhase::Hold;
		FadePhaseStart = World->GetTimeSeconds();
		break;
	}
	case EFadePhase::Hold:
		SetCameraFade(1.f);
		if (Elapsed >= FadeHoldDur) { FadePhase = EFadePhase::In; FadePhaseStart = World->GetTimeSeconds(); }
		break;
	case EFadePhase::In:
	{
		const float Dur = FMath::Max(FadeInDur, 0.02f);
		if (Elapsed < FadeInDur) { SetCameraFade(1.f - FadeSmooth01(Elapsed / Dur)); break; }
		SetCameraFade(0.f);
		FadePhase = EFadePhase::None;
		break;
	}
	default: break;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// conditions

bool UMissionSubsystem::Evaluate(const FMissionCondition& C) const
{
	switch (C.Type)
	{
	case FMissionCondition::EType::Reach:
	{
		const APawn* P = GetPlayerPawn();
		if (!P) { return false; }
		const FVector L = P->GetActorLocation();
		return FVector::Dist2D(L, C.Location) <= C.Radius && FMath::Abs(L.Z - C.Location.Z) <= C.ZTolerance;
	}
	case FMissionCondition::EType::TriggerEnter:
	{
		const TWeakObjectPtr<AMissionTrigger>* T = Triggers.Find(C.Name);
		return T && T->IsValid() && (*T)->IsPlayerInside();
	}
	case FMissionCondition::EType::TriggerExit: return StageEvents.Contains(FName(*FString::Printf(TEXT("trigger_exit:%s"), *C.Name.ToString())));
	case FMissionCondition::EType::Interact: return StageEvents.Contains(FName(*FString::Printf(TEXT("interact:%s"), *C.Name.ToString())));
	case FMissionCondition::EType::Event: return StageEvents.Contains(C.Name) || (C.bLatched && MissionEvents.Contains(C.Name));
	case FMissionCondition::EType::Timer: return GetWorld()->GetTimeSeconds() - StageStartTime >= C.Seconds;
	case FMissionCondition::EType::Flag: return Flags.Contains(C.Name);
	case FMissionCondition::EType::HasItem:
	{
		const UBullyProgressSubsystem* Prog = UBullyProgressSubsystem::Get(GetWorld());
		return Prog && Prog->GetItemCount(C.Name) > 0;
	}
	case FMissionCondition::EType::Attempt:
	{
		const UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(GetWorld());
		const int32 A = (Camp && Mission.IsValid()) ? Camp->GetAttempts(Mission->Id) : 1;
		return C.bAttemptEq ? A == C.AttemptN : A >= C.AttemptN;
	}
	case FMissionCondition::EType::LeaveArea:
	{
		const APawn* P = GetPlayerPawn();
		if (!P || !Mission.IsValid()) { return false; }
		const FBox* Box = Mission->AreaBoxes.Find(C.Area);
		if (!Box) { return false; }   // no area data loaded: never fails
		return !Box->IsInside(P->GetActorLocation());
	}
	case FMissionCondition::EType::Leash:
	{
		// chase leash: true after the named npc is farther than Radius from the player for Seconds continuously
		const APawn* P = GetPlayerPawn();
		const TWeakObjectPtr<ABullyScriptedNPC>* N = Npcs.Find(C.Name);
		if (!P || !N || !N->IsValid()) { LeashFarSince = -1.0; return false; }
		if (FVector::Dist2D(P->GetActorLocation(), (*N)->GetActorLocation()) <= C.Radius) { LeashFarSince = -1.0; return false; }
		if (LeashFarSince < 0.0) { LeashFarSince = GetWorld()->GetTimeSeconds(); }
		return GetWorld()->GetTimeSeconds() - LeashFarSince >= C.Seconds;
	}
	case FMissionCondition::EType::ZoneTimer:
	{
		// zone-exit countdown: true after the player has been outside the trigger for Seconds continuously
		// (re-entering cancels); arms only after the player has been inside once this stage
		const TWeakObjectPtr<AMissionTrigger>* T = Triggers.Find(C.Name);
		const bool bInside = T && T->IsValid() && (*T)->IsPlayerInside();
		if (bInside) { ZoneWasInside = true; ZoneOutSince = -1.0; return false; }
		if (!ZoneWasInside) { return false; }
		if (ZoneOutSince < 0.0) { ZoneOutSince = GetWorld()->GetTimeSeconds(); }
		return GetWorld()->GetTimeSeconds() - ZoneOutSince >= C.Seconds;
	}
	case FMissionCondition::EType::All:
		for (const FMissionCondition& K : C.Children) { if (!Evaluate(K)) { return false; } }
		return true;
	case FMissionCondition::EType::Any:
		for (const FMissionCondition& K : C.Children) { if (Evaluate(K)) { return true; } }
		return false;
	default: return false;
	}
}

void UMissionSubsystem::CollectInteracts(const FMissionCondition& C, TArray<const FMissionCondition*>& Out) const
{
	if (C.Type == FMissionCondition::EType::Interact) { Out.Add(&C); }
	for (const FMissionCondition& K : C.Children) { CollectInteracts(K, Out); }
}

void UMissionSubsystem::RefreshStageActors()
{
	TagActors.Reset();
	if (!Mission.IsValid() || StageIndex >= Mission->Stages.Num()) { return; }
	const FMissionStage& St = Mission->Stages[StageIndex];
	TArray<const FMissionCondition*> Interacts;
	CollectInteracts(St.Complete, Interacts);
	if (St.Fail.IsValid()) { CollectInteracts(St.Fail, Interacts); }
	for (const FMissionCondition* C : Interacts)
	{
		if (C->bActorTag)
		{
			TArray<AActor*> Found;
			UGameplayStatics::GetAllActorsWithTag(GetWorld(), C->Name, Found);
			for (AActor* A : Found) { TagActors.Add(A); }
		}
		else if (!Triggers.Contains(C->Name))
		{
			UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION stage %d interacts with unknown trigger '%s'"), StageIndex + 1, *C->Name.ToString());
		}
	}
}

bool UMissionSubsystem::InteractInRange(const FMissionCondition& C, FVector* OutAt) const
{
	if (C.bActorTag)
	{
		const APawn* P = GetPlayerPawn();
		if (!P) { return false; }
		for (const TWeakObjectPtr<AActor>& A : TagActors)
		{
			if (A.IsValid() && FVector::Dist2D(A->GetActorLocation(), P->GetActorLocation()) <= C.Radius)
			{
				if (OutAt) { *OutAt = A->GetActorLocation(); }
				return true;
			}
		}
		return false;
	}
	const TWeakObjectPtr<AMissionTrigger>* T = Triggers.Find(C.Name);
	if (T && T->IsValid() && (*T)->IsPlayerInside())
	{
		if (OutAt) { *OutAt = (*T)->GetActorLocation(); }
		return true;
	}
	return false;
}

bool UMissionSubsystem::DoInteract()
{
	if (State != EState::Running || !Mission.IsValid()) { return false; }
	const FMissionStage& St = Mission->Stages[StageIndex];
	TArray<const FMissionCondition*> Interacts;
	CollectInteracts(St.Complete, Interacts);
	if (St.Fail.IsValid()) { CollectInteracts(St.Fail, Interacts); }
	bool bFired = false;
	for (const FMissionCondition* C : Interacts)
	{
		if (InteractInRange(*C))
		{
			if (C->MoneyCost > 0)   // social: the wallet must cover the ask (gifts, pay-offs)
			{
				UBullyProgressSubsystem* Prog = UBullyProgressSubsystem::Get(GetWorld());
				if (!Prog || Prog->GetMoney() < C->MoneyCost)
				{
					if (Hud.IsValid()) { Hud->ShowHint(FString::Printf(TEXT("You need $%d."), C->MoneyCost), 2.5f); }
					UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION interact '%s' refused: needs $%d"), *C->Name.ToString(), C->MoneyCost);
					continue;
				}
				Prog->TakeMoney(C->MoneyCost);
			}
			const FName Ev(*FString::Printf(TEXT("interact:%s"), *C->Name.ToString()));
			StageEvents.Add(Ev);
			MissionEvents.Add(Ev);
			if (!C->FlagToSet.IsNone()) { SetFlag(C->FlagToSet, true); }
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION interact '%s' (stage %d)"), *C->Name.ToString(), StageIndex + 1);
			OnMissionEvent.Broadcast(Ev);
			bFired = true;
		}
	}
	return bFired;
}

/** The world position a positional condition points at (also the HUD waypoint and the smoke harness target). */
bool UMissionSubsystem::ConditionTarget(const FMissionCondition& C, FVector& Out) const
{
	switch (C.Type)
	{
	case FMissionCondition::EType::Reach: Out = C.Location; return true;
	case FMissionCondition::EType::TriggerEnter:
	case FMissionCondition::EType::TriggerExit:
	case FMissionCondition::EType::Interact:
	{
		if (C.Type == FMissionCondition::EType::Interact && C.bActorTag)
		{
			const APawn* P = GetPlayerPawn();
			float Best = TNumericLimits<float>::Max(); bool bFound = false;
			for (const TWeakObjectPtr<AActor>& A : TagActors)
			{
				if (!A.IsValid()) { continue; }
				const float D = P ? FVector::DistSquared(A->GetActorLocation(), P->GetActorLocation()) : 0.f;
				if (D < Best) { Best = D; Out = A->GetActorLocation(); bFound = true; }
			}
			return bFound;
		}
		const TWeakObjectPtr<AMissionTrigger>* T = Triggers.Find(C.Name);
		if (T && T->IsValid()) { Out = (*T)->GetActorLocation(); return true; }
		return false;
	}
	case FMissionCondition::EType::All:
		for (const FMissionCondition& K : C.Children) { if (!Evaluate(K) && ConditionTarget(K, Out)) { return true; } }
		return false;
	case FMissionCondition::EType::Any:
	{
		const APawn* P = GetPlayerPawn();
		float Best = TNumericLimits<float>::Max(); bool bFound = false;
		for (const FMissionCondition& K : C.Children)
		{
			FVector V;
			if (ConditionTarget(K, V))
			{
				const float D = P ? FVector::DistSquared(V, P->GetActorLocation()) : 0.f;
				if (D < Best) { Best = D; Out = V; bFound = true; }
			}
		}
		return bFound;
	}
	default: return false;
	}
}

bool UMissionSubsystem::StageTarget(FVector& Out) const
{
	if (!Mission.IsValid() || StageIndex >= Mission->Stages.Num()) { return false; }
	const FMissionStage& St = Mission->Stages[StageIndex];
	if (St.BlipMode == FMissionStage::EBlip::Explicit) { Out = St.BlipLocation; return true; }
	if (St.BlipMode == FMissionStage::EBlip::None) { return false; }
	return ConditionTarget(St.Complete, Out);
}

bool UMissionSubsystem::GotoStageTarget()
{
	FVector V;
	if (!StageTarget(V)) { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION goto: current stage has no positional target")); return false; }
	TeleportPlayer(V + FVector(0, 0, 100), 0.f, false);
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION goto stage %d target %s"), StageIndex + 1, *V.ToCompactString());
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
// triggers

void UMissionSubsystem::RegisterTrigger(AMissionTrigger* T)
{
	if (T) { Triggers.Add(T->TriggerId, T); }
}

void UMissionSubsystem::UnregisterTrigger(AMissionTrigger* T)
{
	if (!T) { return; }
	const TWeakObjectPtr<AMissionTrigger>* E = Triggers.Find(T->TriggerId);
	if (E && E->Get() == T) { Triggers.Remove(T->TriggerId); }
}

void UMissionSubsystem::NotifyTrigger(AMissionTrigger* T, bool bEntered)
{
	if (!T) { return; }
	const FName Ev(*FString::Printf(TEXT("trigger_%s:%s"), bEntered ? TEXT("enter") : TEXT("exit"), *T->TriggerId.ToString()));
	StageEvents.Add(Ev);
	MissionEvents.Add(Ev);
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION trigger '%s' %s"), *T->TriggerId.ToString(), bEntered ? TEXT("entered") : TEXT("exited"));
	OnMissionEvent.Broadcast(Ev);
}

void UMissionSubsystem::SpawnTriggers()
{
	UWorld* World = GetWorld();
	for (const FMissionTriggerDef& D : Mission->Triggers)
	{
		if (const TWeakObjectPtr<AMissionTrigger>* Existing = Triggers.Find(D.Id); Existing && Existing->IsValid()) { continue; }   // hand-placed wins
		FActorSpawnParameters Params;
		AMissionTrigger* T = World->SpawnActorDeferred<AMissionTrigger>(AMissionTrigger::StaticClass(), FTransform(FRotator(0.f, D.Yaw, 0.f), D.Center));
		if (!T) { continue; }
		T->TriggerId = D.Id;
		T->bInteract = D.bInteract;
		T->Prompt = D.Prompt;
		T->Box->SetBoxExtent(D.HalfExtent);
		T->FinishSpawning(FTransform(FRotator(0.f, D.Yaw, 0.f), D.Center));
		OwnedTriggers.Add(T);
	}
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION spawned %d trigger volumes (%d registered)"), OwnedTriggers.Num(), Triggers.Num());
}

void UMissionSubsystem::DestroyOwnedTriggers()
{
	for (const TWeakObjectPtr<AMissionTrigger>& T : OwnedTriggers)
	{
		if (T.IsValid()) { T->Destroy(); }
	}
	OwnedTriggers.Reset();
}

// ---------------------------------------------------------------------------------------------------------------
// HUD + tick

void UMissionSubsystem::EnsureHud()
{
	if (!Hud.IsValid()) { Hud = MakeShared<FBullyHUD>(); }
	Hud->Show(GetWorld());
}

void UMissionSubsystem::RefreshHudPerFrame(float Dt)
{
	if (!Hud.IsValid() || !Mission.IsValid()) { return; }
	// keep the wallet readout live (no flash — reward actions do the flash themselves)
	if (UBullyProgressSubsystem* Prog = UBullyProgressSubsystem::Get(GetWorld()))
	{
		Hud->SetWallet(Prog->GetMoney(), Prog->GetRespect(), false);
	}
	if (State == EState::Running)
	{
		FVector V;
		if (StageTarget(V)) { Hud->SetBlip(true, V); } else { Hud->SetBlip(false); }

		// button prompt: first interact condition of this stage whose target is in range
		const FMissionStage& St = Mission->Stages[StageIndex];
		TArray<const FMissionCondition*> Interacts;
		CollectInteracts(St.Complete, Interacts);
		FString Prompt;
		for (const FMissionCondition* C : Interacts)
		{
			if (!InteractInRange(*C) || StageEvents.Contains(FName(*FString::Printf(TEXT("interact:%s"), *C->Name.ToString())))) { continue; }
			Prompt = C->Prompt;
			if (Prompt.IsEmpty())
			{
				const TWeakObjectPtr<AMissionTrigger>* T = Triggers.Find(C->Name);
				if (T && T->IsValid()) { Prompt = (*T)->Prompt; }
			}
			if (Prompt.IsEmpty()) { Prompt = TEXT("Interact"); }
			break;
		}
		Hud->SetPrompt(Prompt);
	}
}

void UMissionSubsystem::Tick(float Dt)
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	if (!PendingAutoStart.IsEmpty() && World->GetTimeSeconds() >= AutoStartAt && GetPlayerPawn())
	{
		const FString Id = PendingAutoStart;
		const bool bDev = bPendingDevOverride;
		PendingAutoStart.Reset();
		bPendingDevOverride = false;
		StartMission(Id, 0, bDev);
	}
	if (Smoke.bEnabled) { TickSmoke(Dt); }
	if (Shots.bEnabled) { TickMissionShots(Dt); }
	TickFade(Dt);
	if (Hud.IsValid()) { Hud->Tick(Dt); }

	if (State == EState::Running && Mission.IsValid())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top)) { DoInteract(); }
		}
		RefreshHudPerFrame(Dt);
		TickSpeech(Dt);
		TickMinigame(Dt);
		TickHeat(Dt);
		TickLeads(Dt);
		TickFollows(Dt);
		TickWaves();
		TickSpotters();
		TickCountdown();
		TickLesson();
		TickRace();
		TickClockFail();
		TickDefenders(Dt);
		TickChoice();
		TickBossBar();
		// visible fail timer -> HUD countdown
		const FMissionStage& St = Mission->Stages[StageIndex];
		if (Hud.IsValid())
		{
			if (St.Fail.Type == FMissionCondition::EType::Timer && St.Fail.bVisible)
			{
				Hud->SetCountdown(St.Fail.Seconds - float(World->GetTimeSeconds() - StageStartTime));
			}
			else if (St.Fail.Type == FMissionCondition::EType::ZoneTimer && St.Fail.bVisible && ZoneOutSince >= 0.0)
			{
				Hud->SetCountdown(St.Fail.Seconds - float(World->GetTimeSeconds() - ZoneOutSince));
			}
			else if (Countdown.bActive && Countdown.bVisible)
			{
				Hud->SetCountdown(float(Countdown.EndsAt - World->GetTimeSeconds()));   // mission-scoped countdown (e.g. the bladder)
			}
			else { Hud->SetCountdown(-1.f); }
		}
		// mission-critical npcs: scripted npcs can't be KO'd in combat today, so the fail path is their unexpected
		// expiry (a future combat-integrated npc reports real KO/hit through the same event)
		for (const FName& Id : MissionCriticalNpcs)
		{
			const TWeakObjectPtr<ABullyScriptedNPC>* N = Npcs.Find(Id);
			if (N && !N->IsValid())
			{
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION mission-critical npc '%s' expired -> event npc_ko:%s"), *Id.ToString(), *Id.ToString());
				FireEvent(FName(*FString::Printf(TEXT("npc_ko:%s"), *Id.ToString())));
				MissionCriticalNpcs.Remove(Id);
				break;   // one per tick; the set shrinks
			}
		}
		if (St.Fail.IsValid() && Evaluate(St.Fail)) { FailMission(TEXT("objective failed")); }
		else if (Mission->RootFail.IsValid() && Evaluate(Mission->RootFail))
		{
			FailMission(Mission->RootFailText.IsEmpty() ? TEXT("mission area left") : Mission->RootFailText);
		}
		else if (Evaluate(St.Complete)) { CompleteStage(); }
	}
	else if (State == EState::Failed && Mission.IsValid())
	{
		if (World->GetTimeSeconds() - FailTime >= Mission->FailDelay) { RetryNow(); }
	}
	else if (State == EState::Complete && Hud.IsValid() && Hud->IsShown() && Hud->GetState().BannerLeft <= 0.f)
	{
		Hud->ClearObjective();
		Hud->Hide();
	}
	bSmokeDriveMinigame = false;   // TickSmoke re-arms it each tick while it hold-drives a minigame
}

void UMissionSubsystem::LogStatus() const
{
	if (!Mission.IsValid() || State == EState::Idle) { UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION status: idle")); return; }
	const FMissionStage& St = Mission->Stages[FMath::Clamp(StageIndex, 0, Mission->Stages.Num() - 1)];
	FVector T = FVector::ZeroVector; const bool bT = StageTarget(T);
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION status: %s state=%d stage %d/%d '%s' attempt=%d target=%s flags=%d"),
		*Mission->Id, int32(State), StageIndex + 1, Mission->Stages.Num(), *St.Objective, Attempt, bT ? *T.ToCompactString() : TEXT("none"), Flags.Num());
}

// ---------------------------------------------------------------------------------------------------------------
// rendered cutscene capture (-BRMissionShots)

void UMissionSubsystem::FirePendingShot()
{
	UWorld* World = GetWorld();
	if (!World || Shots.PendingAt < 0.0) { return; }
	const FString File = Shots.Dir / (Shots.PendingName + TEXT(".png"));
	FHighResScreenshotConfig& HR = GetHighResScreenshotConfig();
	HR.SetResolution(uint32(Shots.ResX), uint32(Shots.ResY), 1.0f);
	HR.SetFilename(File);
	HR.bMaskEnabled = false;
	HR.bDumpBufferVisualizationTargets = false;
	const bool bOk = GEngine->GameViewport && GEngine->GameViewport->Viewport
		&& GEngine->GameViewport->Viewport->TakeHighResScreenShot();
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSIONSHOT %s stage %d -> %s (t=%.1f)"),
		bOk ? TEXT("shot") : TEXT("REFUSED"), Shots.PendingStage + 1, *File, World->GetTimeSeconds());
	++Shots.Count;
	Shots.PendingStage = -1;
	Shots.PendingAt = -1.0;
	// After the last stage's shot, let the capture flush, then quit before the mission chains onward.
	if (Shots.Count >= Shots.TotalStages) { Shots.QuitAt = World->GetTimeSeconds() + 2.0; }
}

void UMissionSubsystem::TickMissionShots(float Dt)
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	const double Now = World->GetTimeSeconds();

	// 1) Hold the mission start until on-demand shader/texture/distance-field compilation drains (uncooked -game),
	//    plus a warm-up, so the very first beat isn't captured through a half-compiled scene.
	if (!Shots.bStarted)
	{
		if (!GetPlayerPawn()) { return; }
		int32 Assets = 0, ShaderJobs = 0;
#if WITH_EDITOR
		Assets = FAssetCompilingManager::Get().GetNumRemainingAssets();
		ShaderJobs = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
#endif
		if (Assets == 0 && ShaderJobs == 0) { if (Shots.IdleSince < 0.0) { Shots.IdleSince = Now; } }
		else { Shots.IdleSince = -1.0; }
		if (Now - Shots.LastCompileLog >= 10.0)
		{
			Shots.LastCompileLog = Now;
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSIONSHOT waiting for compilation: %d assets, %d shader jobs (t=%.0f)"), Assets, ShaderJobs, Now);
		}
		const bool bIdle = Shots.IdleSince >= 0.0 && Now - Shots.IdleSince >= 3.0;
		if (!bIdle) { return; }
		if (Shots.CompileReadyAt < 0.0)
		{
			Shots.CompileReadyAt = Now;
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSIONSHOT compilation idle at t=%.1f; warm-up %.1f s"), Now, Shots.Warmup);
		}
		if (Now - Shots.CompileReadyAt < Shots.Warmup) { return; }

		Shots.bStarted = true;
		if (!StartMission(Shots.MissionId, 0, /*bDevOverride*/ true))
		{
			UE_LOG(LogBullyMission, Error, TEXT("BR_MISSIONSHOT mission '%s' failed to load; quitting"), *Shots.MissionId);
			GEngine->Exec(World, TEXT("quit"));
			return;
		}
		Shots.TotalStages = Mission.IsValid() ? Mission->Stages.Num() : 0;
		// StartMission already ran BeginStage(0) while TotalStages was still 0 (so its hook skipped) -- schedule
		// stage 0's shot here now that the count is known.
		if (Shots.TotalStages > 0 && State == EState::Running)
		{
			Shots.PendingStage = StageIndex;
			Shots.PendingAt = Now + Shots.ShotDelay;
			const FMissionStage& St0 = Mission->Stages[StageIndex];
			Shots.PendingName = FString::Printf(TEXT("%02d_%s"), StageIndex, St0.Id.IsNone() ? TEXT("stage") : *St0.Id.ToString());
		}
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSIONSHOT mission '%s' started: %d stage(s) to capture"), *Shots.MissionId, Shots.TotalStages);
		return;
	}

	// 2) Fire the pending beat shot once its settle delay has passed (re-render at the requested size).
	if (Shots.PendingAt >= 0.0 && Now >= Shots.PendingAt) { FirePendingShot(); }

	// 2b) Safety: the mission completed (or is chaining) with shots still outstanding -- never burn to the script's
	//     time limit; flush whatever is pending and quit.
	if (Shots.bStarted && State == EState::Complete && Shots.QuitAt < 0.0)
	{
		FirePendingShot();
		Shots.QuitAt = Now + 2.0;
	}

	// 3) Done: quit cleanly (the arrival mission would otherwise chain straight into day_one).
	if (Shots.QuitAt >= 0.0 && Now >= Shots.QuitAt)
	{
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSIONSHOT done: %d/%d shot(s) in %s; quitting"), Shots.Count, Shots.TotalStages, *Shots.Dir);
		Shots.QuitAt = -1.0;
		GEngine->Exec(World, TEXT("quit"));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// smoke harness

void UMissionSubsystem::DriveSmoke(const FString& Id, bool bQuitOnDone)
{
	if (Smoke.bEnabled) { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION smoke driver already armed ('%s')"), *Smoke.MissionId); return; }
	Smoke = FSmoke();
	Smoke.bEnabled = true;
	Smoke.MissionId = Id;
	Smoke.bWantFailTest = true;   // re-detected from the loaded mission (smoke_fail stage) once it starts
	Smoke.bNoQuit = !bQuitOnDone;
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION smoke harness armed by code: mission '%s' (quit on done %s)"), *Id, bQuitOnDone ? TEXT("on") : TEXT("off"));
}

void UMissionSubsystem::TickSmoke(float Dt)
{
	UWorld* World = GetWorld();
	if (Smoke.bDone) { return; }
	Smoke.Elapsed += Dt;
	auto Quit = [&](bool bPass, const FString& Why)
	{
		Smoke.bDone = true;
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE %s: %s (stages seen %d/%d, attempts %d, t=%.1fs)"), bPass ? TEXT("PASS") : TEXT("FAIL"), *Why,
			Smoke.StagesSeen.Num(), Mission.IsValid() ? Mission->Stages.Num() : 0, Attempt, Smoke.Elapsed);
		if (Smoke.bNoQuit) { Smoke.bEnabled = false; return; }   // the caller (campaign smoke) keeps driving
		GEngine->Exec(World, TEXT("quit"));
	};
	// wrong-way bookkeeping: the stage under test just completed (or the mission ended) while a skip was pending
	auto NoteWrongWayDone = [&]()
	{
		if (Smoke.bWrongWay && (Smoke.WrongWayPhase == 1 || Smoke.WrongWayPhase == 2))
		{
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE wrongway stage '%s' completed after the %s"), *Smoke.WrongWayStageId.ToString(),
				Smoke.WrongWayPhase == 1 ? TEXT("overshoot") : TEXT("skip-ahead teleport (a fallback caught it)"));
		}
		Smoke.WrongWayPhase = 0;
	};

	if (!Smoke.bStarted)
	{
		if (Smoke.Elapsed > 3.0 && GetPlayerPawn())
		{
			Smoke.bStarted = true;
			// an empty MissionId = drive-current mode (DriveSmoke("")): the mission is already running
			if (!Smoke.MissionId.IsEmpty() && !StartMission(Smoke.MissionId, 0, /*bDevOverride*/ true)) { Quit(false, TEXT("mission failed to load")); return; }
			// Only expect a fail/retry exercise if this mission actually defines a smoke_fail stage (day_one does;
			// a plain errand mission need not). Keeps the harness usable for any mission.
			Smoke.bWantFailTest = false;
			if (!Shots.bEnabled && Mission.IsValid())   // shots-drive never exercises fail/retry (see arming note)
			{
				for (const FMissionStage& St : Mission->Stages)
				{
					if (St.Fail.IsValid() && CondHasEvent(St.Fail, FName("smoke_fail"))) { Smoke.bWantFailTest = true; break; }
				}
			}
		}
		return;
	}
	if (Smoke.Elapsed > (Smoke.bWrongWay ? 300.0 : 150.0)) { Quit(false, TEXT("timeout")); return; }
	if (State == EState::Complete)
	{
		// chained hand-off (a start_mission action is pending): with the no-quit driver, keep driving the next mission
		if (Smoke.bNoQuit && !PendingAutoStart.IsEmpty())
		{
			Smoke.LastStage = -1;
			Smoke.StagesSeen.Reset();
			Smoke.bFailTested = true;      // the fail/retry exercise belongs to the first mission of a chain
			Smoke.bWantFailTest = false;   // (and Attempt is per-mission, so the Attempt>=2 verdict can't span a chain)
			return;
		}
		NoteWrongWayDone();
		const bool bAll = Mission.IsValid() && Smoke.StagesSeen.Num() == Mission->Stages.Num();
		FString Why = bAll ? TEXT("mission completed") : TEXT("completed without visiting every stage");
		if (Smoke.bWrongWay)
		{
			FString Ids;
			for (const FName& N : Smoke.WrongWayStuck)
			{
				if (!Ids.IsEmpty()) { Ids += TEXT(","); }
				Ids += N.ToString();
			}
			Why += FString::Printf(TEXT("; wrong-way: skipped ahead on %d stage(s), %d stuck [%s]"), Smoke.WrongWayTested, Smoke.WrongWayStuck.Num(), *Ids);
		}
		// the fail -> retry exercise belongs to the normal run; the wrong-way run only judges the skips
		const bool bFailOk = Smoke.bWrongWay || !Smoke.bWantFailTest || (Smoke.bFailTested && Attempt >= 2);
		Quit(bAll && bFailOk && Smoke.WrongWayStuck.Num() == 0, Why);
		return;
	}
	if (State != EState::Running || !Mission.IsValid()) { return; }

	const FMissionStage& St = Mission->Stages[StageIndex];
	if (StageIndex != Smoke.LastStage)
	{
		NoteWrongWayDone();
		Smoke.LastStage = StageIndex;
		Smoke.StagesSeen.AddUnique(StageIndex);
		Smoke.LastDrive = Smoke.Elapsed;   // first drive 1.5 s after the stage begins
		Smoke.InsideSince = -1.0;
	}

	// exercise the fail -> retry path once: a stage whose fail condition holds the event "smoke_fail"
	if (Smoke.bWantFailTest && !Smoke.bFailTested && CondHasEvent(St.Fail, FName("smoke_fail")) && Smoke.Elapsed - Smoke.LastDrive > 0.6)
	{
		Smoke.bFailTested = true;
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE firing smoke_fail on stage %d to exercise fail/retry"), StageIndex + 1);
		FireEvent(FName("smoke_fail"));
		return;
	}

	// race drive: run the course's checkpoints in order, one per tick — races are time-critical, so this sits
	// above the 1.5 s drive cadence gate (the rival riders are scripted npc_move_to movers on the same course;
	// checkpoint-hopping always beats them home)
	if (Race.bActive && Race.Wps.Num() > 0)
	{
		Smoke.LastDrive = Smoke.Elapsed;
		TeleportPlayer(Race.Wps[Race.NextIdx] + FVector(0, 0, 100), 0.f, false);
		return;
	}

	if (Smoke.Elapsed - Smoke.LastDrive < 1.5) { return; }

	// wrong-way mode: a positional stage is first overshot, then skipped ahead by teleporting onto the NEXT stage's goal (the
	// far side of a door link). The stage must complete by itself -- the data needs a fallback for that landing spot. If it
	// does not, record it as stuck and fall through so the normal driver below recovers and the rest of the mission still runs.
	if (Smoke.bWrongWay && Smoke.WrongWayPhase < 3 && SkippablePositional(St.Complete))
	{
		const APawn* Pawn = GetPlayerPawn();
		if (!Pawn) { return; }
		if (Smoke.WrongWayPhase == 0)
		{
			FVector Over = Pawn->GetActorLocation();
			PrimaryGoal(St.Complete, Over);
			FVector Dir = (Over - Pawn->GetActorLocation()) * FVector(1.0, 1.0, 0.0);
			Dir = Dir.GetSafeNormal();
			if (Dir.IsNearlyZero()) { Dir = FVector(1.0, 0.0, 0.0); }
			Over += Dir * 800.0 + FVector(0.0, 0.0, 100.0);
			Smoke.WrongWayStageId = St.Id;
			Smoke.WrongWayPhase = 1;
			Smoke.WrongWayAt = Smoke.Elapsed;
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE wrongway overshoot stage %d '%s' to %s"), StageIndex + 1, *St.Id.ToString(), *Over.ToCompactString());
			TeleportPlayer(Over, 0.f, false);
			return;
		}
		if (Smoke.WrongWayPhase == 1)
		{
			if (Smoke.Elapsed - Smoke.WrongWayAt < 1.0) { return; }
			FVector Goal = FVector::ZeroVector;
			const bool bHaveGoal = StageIndex + 1 < Mission->Stages.Num() && SkippablePositional(Mission->Stages[StageIndex + 1].Complete) && PrimaryGoal(Mission->Stages[StageIndex + 1].Complete, Goal);
			if (!bHaveGoal)
			{
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE wrongway stage %d '%s': overshoot left it open (the player walks back); the next stage is not positional, no skip-ahead to test"), StageIndex + 1, *St.Id.ToString());
				Smoke.WrongWayPhase = 3;
			}
			else
			{
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE wrongway: overshoot left stage %d '%s' open (the player walks back); skipping ahead to the next goal %s"), StageIndex + 1, *St.Id.ToString(), *Goal.ToCompactString());
				Smoke.WrongWayPhase = 2;
				Smoke.WrongWayAt = Smoke.Elapsed;
				Smoke.WrongWayTested++;
				TeleportPlayer(Goal + FVector(0.0, 0.0, 100.0), 0.f, false);
				return;
			}
		}
		else if (Smoke.WrongWayPhase == 2)
		{
			if (Smoke.Elapsed - Smoke.WrongWayAt < 2.0) { return; }
			FVector Blip = FVector::ZeroVector;
			const double BlipDist = StageTarget(Blip) ? FVector::Dist(Blip, Pawn->GetActorLocation()) : -1.0;
			Smoke.WrongWayStuck.AddUnique(St.Id);
			Smoke.WrongWayPhase = 3;
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE wrongway STUCK: stage %d '%s' did not complete after skipping ahead (blip target %.0f cm away); recovering with the normal driver"), StageIndex + 1, *St.Id.ToString(), BlipDist);
		}
	}

	// sm-cutscenes: an original cutscene is skipped after a beat (exercises the skip path); nothing else drives meanwhile
	if (UBullyCutsceneSubsystem* CS = UBullyCutsceneSubsystem::Get(GetWorld()); CS && CS->IsPlaying())
	{
		if (CS->GetElapsed() > 1.2f && Smoke.Elapsed - Smoke.LastDrive > 1.2)
		{
			Smoke.LastDrive = Smoke.Elapsed;
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE skipping cutscene %s (stage %d)"), *CS->GetCurrentId(), StageIndex + 1);
			CS->Skip();
		}
		return;
	}
	// campaign-spine drives: a speech beat is skipped (exercises the skip path); an armed minigame is walked to and
	// hold-driven; heat/lead stages are covered by the interact + teleport paths below (':'-events are never fired).
	if (Speech.bActive && Smoke.Elapsed - Smoke.LastDrive > 1.2)
	{
		Smoke.LastDrive = Smoke.Elapsed;
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE skipping the speech beat (stage %d)"), StageIndex + 1);
		SkipSpeechSequence();
		return;
	}
	if (Minigame.bArmed)
	{
		const APawn* Pawn = GetPlayerPawn();
		if (Pawn && FVector::Dist2D(Pawn->GetActorLocation(), Minigame.At) > Minigame.Radius * 0.8f)
		{
			Smoke.LastDrive = Smoke.Elapsed;
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE teleport player to minigame %s"), *Minigame.At.ToCompactString());
			TeleportPlayer(Minigame.At + FVector(0, 0, 100), 0.f, false);
			return;
		}
		bSmokeDriveMinigame = true;   // the driver's hold-E
		return;
	}

	// dialogue-choice drive: pick the first option whose item gate passes (the gate check runs for real)
	if (Choice.bActive)
	{
		Smoke.LastDrive = Smoke.Elapsed;
		UBullyProgressSubsystem* Prog = UBullyProgressSubsystem::Get(World);
		for (int32 i = 0; i < Choice.Options.Num(); ++i)
		{
			const FChoiceOption& O = Choice.Options[i];
			if (O.RequiresItem.IsNone() || (Prog && Prog->GetItemCount(O.RequiresItem) > 0))
			{
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE picks dialogue option %d (%s)"), i + 1, *O.Label);
				ChooseDialogueOption(i);
				break;
			}
		}
		return;
	}

	// lesson drive: perform the taught move through the real combat pipeline (StartMove fires player_move:<move>)
	if (Lesson.bActive)
	{
		if (ABullyCharacter* Player = Cast<ABullyCharacter>(GetPlayerPawn()))
		{
			Smoke.LastDrive = Smoke.Elapsed;
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE performs lesson move %s (%d/%d)"), *Lesson.Move.ToString(), Lesson.Count, Lesson.Needed);
			Player->GetCombat()->StartMove(Lesson.Move);
			return;
		}
	}

	// wave drive: hit the next living wave member through the real combat pipeline (reactions, KO, wave:<group>
	// event all fire; hits_to_beat bosses take their scripted per-hit damage along the same path)
	for (FWaveGroup& G : Waves)
	{
		if (G.bFired) { continue; }
		ABullyCombatPawn* Alive = nullptr;
		for (const TWeakObjectPtr<ABullyCombatPawn>& M : G.Members)
		{
			ABullyCombatPawn* P = M.Get();
			if (P && P->GetCombat() && !P->GetCombat()->IsOut() && P->GetCombat()->GetState() != EBullyCombatState::Beaten) { Alive = P; break; }
		}
		if (Alive)
		{
			Smoke.LastDrive = Smoke.Elapsed;
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE hits wave %s member %s"), *G.Group.ToString(), *Alive->GetName());
			Alive->GetCombat()->ApplyWeaponHit(EBullyHitReaction::Flinch, 500.f);
			return;
		}
	}

	TArray<const FMissionCondition*> Interacts;
	CollectInteracts(St.Complete, Interacts);
	if (Interacts.Num() > 0 && InteractInRange(*Interacts[0]))
	{
		if (Smoke.InsideSince < 0.0) { Smoke.InsideSince = Smoke.Elapsed; }
		if (Smoke.Elapsed - Smoke.InsideSince > 0.4) { UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE pressing interact (stage %d, prompt shown: '%s')"), StageIndex + 1, Hud.IsValid() ? *Hud->GetState().PromptText : TEXT("-")); DoInteract(); }
		return;
	}
	if (St.Complete.Type == FMissionCondition::EType::Event && !SystemEvent(St.Complete.Name)) { Smoke.LastDrive = Smoke.Elapsed; FireEvent(St.Complete.Name); return; }
	// any-of over events only (free_fight: enemy_beaten OR enemy_knocked_out): fire the first one, like a plain event stage
	if (St.Complete.Type == FMissionCondition::EType::Any && !SkippablePositional(St.Complete))
	{
		for (const FMissionCondition& K : St.Complete.Children)
		{
			if (K.Type == FMissionCondition::EType::Event && !SystemEvent(K.Name)) { Smoke.LastDrive = Smoke.Elapsed; FireEvent(K.Name); return; }
		}
	}
	FVector Target;
	// chase stages (a leash on the fail side): follow the fleeing npc — land just behind him, on ground he just
	// crossed (teleporting to the destination would trip the leash, which is the honest behavior being tested)
	FName LeashId;
	if (CondLeash(St.Fail, LeashId))
	{
		// follow him until he's home; once his arrival event has latched, drive the stage's own target instead
		const FName ArrivedEv(*FString::Printf(TEXT("npc_arrived:%s"), *LeashId.ToString()));
		const bool bArrived = StageEvents.Contains(ArrivedEv) || MissionEvents.Contains(ArrivedEv);
		if (const TWeakObjectPtr<ABullyScriptedNPC>* N = Npcs.Find(LeashId); !bArrived && N && N->IsValid())
		{
			Smoke.LastDrive = Smoke.Elapsed;
			const FVector Behind = (*N)->GetActorLocation() - (*N)->GetActorForwardVector().GetSafeNormal2D() * 120.f;
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE chase: player follows %s"), *LeashId.ToString());
			TeleportPlayer(Behind + FVector(0, 0, 100), 0.f, false);
			return;
		}
	}
	if (PrimaryGoal(St.Complete, Target))
	{
		Smoke.LastDrive = Smoke.Elapsed;
		// npc-coincident goals (actor_tag interacts): landing ON the capsule bounce-loops the player, and a wide
		// sidestep can land past the floor's edge -- land 80 cm off toward the player, but only with floor under it
		auto FloorUnder = [&](const FVector& At)
		{
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(BullySmokeGround), true);
			return GetWorld()->LineTraceSingleByObjectType(H, At + FVector(0, 0, 200), At + FVector(0, 0, -600), FCollisionObjectQueryParams(ECC_WorldStatic), Q);
		};
		for (const auto& Pair : Npcs)
		{
			if (!Pair.Value.IsValid()) { continue; }
			const FVector D2 = (Target - Pair.Value->GetActorLocation()) * FVector(1.0, 1.0, 0.0);
			if (D2.Size() < 150.f)
			{
				const APawn* P2 = GetPlayerPawn();
				FVector Dir = P2 ? (P2->GetActorLocation() - Pair.Value->GetActorLocation()) * FVector(1.0, 1.0, 0.0) : FVector(1.f, 0.f, 0.f);
				if (!Dir.Normalize()) { Dir = FVector(1.f, 0.f, 0.f); }   // degenerate (player on top of the npc): any side
				const FVector Candidate = Pair.Value->GetActorLocation() + Dir * 80.f;
				if (FloorUnder(Candidate)) { Target = Candidate; }   // no floor off to the side: keep the npc's own spot
				break;
			}
		}
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE teleport player to stage %d target %s"), StageIndex + 1, *Target.ToCompactString());
		TeleportPlayer(Target + FVector(0, 0, 100), 0.f, false);
		return;
	}
	// drive heartbeat: if a stage ever wedges the driver silently, this says where and why nothing looked actionable
	if (Smoke.Elapsed - Smoke.LastBeatLog > 10.0)
	{
		Smoke.LastBeatLog = Smoke.Elapsed;
		const APawn* P = GetPlayerPawn();
		FVector Goal;
		const bool bGoal = PrimaryGoal(St.Complete, Goal);
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION_SMOKE heartbeat: stage %d '%s' t=%.0f player=%s goal=%s interacts=%d"),
			StageIndex + 1, *St.Id.ToString(), Smoke.Elapsed, P ? *P->GetActorLocation().ToCompactString() : TEXT("none"),
			bGoal ? *Goal.ToCompactString() : TEXT("none"), [&]{ TArray<const FMissionCondition*> I2; CollectInteracts(St.Complete, I2); return I2.Num(); }());
	}
}

bool UMissionSubsystem::SkippablePositional(const FMissionCondition& C) const
{
	switch (C.Type)
	{
	case FMissionCondition::EType::Reach:
	case FMissionCondition::EType::TriggerEnter: return true;
	case FMissionCondition::EType::Any:
		for (const FMissionCondition& K : C.Children) { if (SkippablePositional(K)) { return true; } }
		return false;
	case FMissionCondition::EType::All:
		for (const FMissionCondition& K : C.Children) { if (!SkippablePositional(K)) { return false; } }
		return C.Children.Num() > 0;
	default: return false;
	}
}

bool UMissionSubsystem::PrimaryGoal(const FMissionCondition& C, FVector& Out) const
{
	if (C.Type == FMissionCondition::EType::Any)
	{
		for (const FMissionCondition& K : C.Children) { if (PrimaryGoal(K, Out)) { return true; } }
		return false;
	}
	return ConditionTarget(C, Out);
}

// ---------------------------------------------------------------------------------------------------------------
// chapter-1 primitives runtime (spawn_npc / npc_move_to / npc_lead / pair_anim / speech_sequence / minigame / heat)

ABullyScriptedNPC* UMissionSubsystem::NpcById(FName Id) const
{
	const TWeakObjectPtr<ABullyScriptedNPC>* N = Npcs.Find(Id);
	return (N && N->IsValid()) ? N->Get() : nullptr;
}

ABullyScriptedNPC* UMissionSubsystem::SpawnMissionNpc(const FMissionAction& A)
{
	UWorld* World = GetWorld();
	if (!World) { return nullptr; }
	if (ABullyScriptedNPC* Existing = NpcById(A.NpcId))
	{
		// stage retry / re-run: reuse the actor (repositioned) instead of duplicating it
		Existing->ClearCommands(true);
		Existing->SetActorLocationAndRotation(A.Location + FVector(0, 0, 90.f), FRotator(0.f, A.Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (A.bFacePlayer) { Existing->FacePlayer(true); }
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION spawn_npc %s: reused (retry)"), *A.NpcId.ToString());
		return Existing;
	}
	const FString MeshPath = FString::Printf(TEXT("/Game/Imported/Characters/%s/SK_%s.SK_%s"), *A.Mesh, *A.Mesh, *A.Mesh);
	ABullyScriptedNPC* N = World->SpawnActorDeferred<ABullyScriptedNPC>(ABullyScriptedNPC::StaticClass(),
		FTransform(FRotator(0.f, A.Yaw, 0.f), A.Location + FVector(0, 0, 90.f)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!N) { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION spawn_npc %s: spawn failed"), *A.NpcId.ToString()); return nullptr; }
	N->MeshPath = MeshPath;
	N->NpcId = A.NpcId.ToString();
	N->GroupId = Mission.IsValid() ? Mission->Id : TEXT("mission");
	N->Tags.Add(A.Name.IsNone() ? FName(*FString::Printf(TEXT("br_npc_%s"), *A.NpcId.ToString())) : A.Name);
	N->FinishSpawning(N->GetActorTransform());
	N->OnCommandDone.AddUObject(this, &UMissionSubsystem::OnNpcCommandDone);
	if (A.bFacePlayer) { N->FacePlayer(true); }
	Npcs.Add(A.NpcId, N);
	if (A.bMissionCritical) { MissionCriticalNpcs.Add(A.NpcId); }
	RefreshStageActors();   // a mid-stage spawn joins the current stage's actor_tag interact set
	// invulnerable / untargetable / min_health: scripted NPCs are not combat pawns, so nothing can target or damage
	// them today; the flags are recorded on the log line and a critical one expiring unexpectedly fires npc_ko:<id>.
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION spawn_npc %s mesh=%s at %s%s%s%s"),
		*A.NpcId.ToString(), *A.Mesh, *A.Location.ToCompactString(),
		A.bMissionCritical ? TEXT(" critical") : TEXT(""), A.bInvulnerable ? TEXT(" invuln") : TEXT(""), A.bUntargetable ? TEXT(" untargetable") : TEXT(""));
	return N;
}

void UMissionSubsystem::OnNpcCommandDone(ABullyScriptedNPC* Npc, const FSNPCCommand& Cmd, const FString& /*Result*/)
{
	if (!Npc || (Cmd.Type != ESNPCCmd::GoTo && Cmd.Type != ESNPCCmd::FollowPath)) { return; }
	FName Ev;
	bool bFound = false;
	for (const auto& Pair : PendingArrive) { if (Pair.Key.Get() == Npc) { Ev = Pair.Value; bFound = true; break; } }
	if (!bFound) { return; }
	PendingArrive.Remove(Npc);
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION npc %s arrived -> event %s"), *Npc->NpcId, *Ev.ToString());
	FireEvent(Ev);
}

void UMissionSubsystem::DespawnMissionNpcs(const TCHAR* Why)
{
	if (Npcs.Num() == 0 && Leads.Num() == 0) { return; }
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION despawn %d mission npc(s) (%s)"), Npcs.Num(), Why);
	for (auto& Pair : Npcs) { if (Pair.Value.IsValid()) { Pair.Value->Despawn(); } }
	Npcs.Reset();
	PendingArrive.Reset();
	Leads.Reset();
	MissionCriticalNpcs.Reset();
}

void UMissionSubsystem::TickLeads(float /*Dt*/)
{
	if (Leads.Num() == 0) { return; }
	const APawn* P = GetPlayerPawn();
	for (int32 i = Leads.Num() - 1; i >= 0; --i)
	{
		FLeadOrder& L = Leads[i];
		ABullyScriptedNPC* N = L.Npc.Get();
		if (!N) { Leads.RemoveAt(i); continue; }
		const double Now = GetWorld()->GetTimeSeconds();
		const FVector Cur = L.WpIndex < L.Wps.Num() ? L.Wps[L.WpIndex] : L.Goal;   // current leg target (via/path waypoints, then the goal)
		const bool bFinal = L.WpIndex >= L.Wps.Num();
		const float DNpc = FVector::Dist2D(N->GetActorLocation(), Cur);
		if (DNpc <= (bFinal ? 120.f : 90.f))
		{
			if (!bFinal) { ++L.WpIndex; L.NoProgressAt = Now; L.LastDist = TNumericLimits<float>::Max(); continue; }   // next leg
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION npc_lead %s arrived -> event %s"), *N->NpcId, *L.Event.ToString());
			FireEvent(L.Event);
			Leads.RemoveAt(i);
			continue;
		}
		// no ground gained for 3 s (blocked on geometry): snap to the current leg target, like the walker's own
		// stuck policy -- its snap does not fire while the player stands on the goal, which is exactly the lead case
		if (DNpc < L.LastDist - 50.f) { L.LastDist = DNpc; L.NoProgressAt = Now; }
		else if (Now - L.NoProgressAt > 3.0)
		{
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION npc_lead %s: no progress toward %s (%.0f cm short) -- snapped"), *N->NpcId, *Cur.ToCompactString(), DNpc);
			N->ClearCommands(true);
			N->SetActorLocation(Cur + FVector(0, 0, 90.f), false, nullptr, ETeleportType::TeleportPhysics);
			L.NoProgressAt = Now;
			continue;
		}
		if (!L.bWaitingForPlayer)
		{
			// pause while the player trails (compared by distance to the FINAL goal, so a teleport past the npc
			// reads as caught up)
			const float DNpcGoal = FVector::Dist2D(N->GetActorLocation(), L.Goal);
			if (P && FVector::Dist2D(P->GetActorLocation(), L.Goal) > DNpcGoal + 200.f)
			{
				N->ClearCommands(true);
				L.bWaitingForPlayer = true;
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION npc_lead %s: waiting for the player"), *N->NpcId);
				continue;
			}
			if (!N->IsBusy()) { N->GoToPoint(Cur, GaitFor(L.Gait, L.bRun), 90.f, FString::Printf(TEXT("mission %s lead"), *N->NpcId)); }
		}
		else if (!P || FVector::Dist2D(P->GetActorLocation(), L.Goal) <= FVector::Dist2D(N->GetActorLocation(), L.Goal) + 200.f)
		{
			L.bWaitingForPlayer = false;   // player caught up (same distance-to-goal threshold as the pause)
		}
	}
}

void UMissionSubsystem::TickSpeech(float /*Dt*/)
{
	if (!Speech.bActive) { return; }
	UWorld* World = GetWorld();
	if (!World) { return; }
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right)) { SkipSpeechSequence(); return; }
	}
	if (World->GetTimeSeconds() < Speech.LineEndsAt) { return; }
	if (Speech.Index >= Speech.Lines.Num())
	{
		Speech.bActive = false;
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION speech_sequence done -> event %s"), *Speech.DoneEvent.ToString());
		FireEvent(Speech.DoneEvent);
		return;
	}
	const FSpeechLine& L = Speech.Lines[Speech.Index++];
	if (Hud.IsValid()) { Hud->Say(L.Speaker, L.Text, L.Seconds); }
	Speech.LineEndsAt = World->GetTimeSeconds() + L.Seconds;
}

void UMissionSubsystem::SkipSpeechSequence()
{
	if (!Speech.bActive) { return; }
	Speech.bActive = false;
	if (Hud.IsValid()) { Hud->Say(TEXT(""), TEXT(""), 0.f); }
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION speech_sequence skipped -> event %s"), *Speech.DoneEvent.ToString());
	FireEvent(Speech.DoneEvent);
}

void UMissionSubsystem::TickMinigame(float Dt)
{
	if (!Minigame.bArmed) { return; }
	UWorld* World = GetWorld();
	const APawn* P = GetPlayerPawn();
	if (!World || !P) { return; }
	const bool bInRange = FVector::Dist2D(P->GetActorLocation(), Minigame.At) <= Minigame.Radius
		&& FMath::Abs(P->GetActorLocation().Z - Minigame.At.Z) <= 400.f;
	if (!Minigame.bRunning)
	{
		bool bStart = false;
		if (bInRange)
		{
			if (APlayerController* PC = World->GetFirstPlayerController()) { bStart = PC->WasInputKeyJustPressed(EKeys::E); }
			bStart = bStart || bSmokeDriveMinigame;
		}
		if (bStart)
		{
			Minigame.bRunning = true;
			Minigame.Progress = 0.f;
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION minigame started (%.1fs)"), Minigame.Seconds);
		}
		return;
	}
	if (!bInRange)   // stepped out mid-hold: fail event, re-arm at zero
	{
		Minigame.bRunning = false;
		Minigame.Progress = 0.f;
		if (Hud.IsValid()) { Hud->SetProgress(0.f, Minigame.Label); }
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION minigame interrupted -> event %s"), *Minigame.Fail.ToString());
		FireEvent(Minigame.Fail);
		return;
	}
	Minigame.Progress += Dt;
	if (Hud.IsValid()) { Hud->SetProgress(Minigame.Progress / Minigame.Seconds, Minigame.Label); }
	if (Minigame.Progress >= Minigame.Seconds)
	{
		Minigame.bArmed = false;
		Minigame.bRunning = false;
		if (Hud.IsValid()) { Hud->SetProgress(-1.f); }
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION minigame complete -> event %s"), *Minigame.Success.ToString());
		FireEvent(Minigame.Success);
	}
}

void UMissionSubsystem::TickHeat(float Dt)
{
	if (HeatPoints <= 0.f) { return; }
	if (!Flags.Contains(TEXT("player_hidden"))) { return; }   // heat only drains from a hiding spot
	HeatPoints -= HeatDecay * Dt;
	if (HeatPoints <= 0.f)
	{
		HeatPoints = 0.f;
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION heat decayed -> event heat:zero"));
		FireEvent(TEXT("heat:zero"));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// wave-B runtime: spawn_wave groups + stealth spotters

void UMissionSubsystem::SpawnWave(const FMissionAction& A)
{
	UWorld* World = GetWorld();
	if (!World || !Mission.IsValid()) { return; }
	// re-issuing a group (stage retry) clears its previous members first
	DespawnWaves(A.Name);
	FWaveGroup G;
	G.Group = A.Name;
	G.AdvanceAt = A.AdvanceAt;
	for (const FString& Pn : A.PointNames)
	{
		const FVector* P = Mission->Points.Find(FName(*Pn));
		if (!P) { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION spawn_wave %s: unknown point '%s'"), *A.Name.ToString(), *Pn); continue; }
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		SP.bDeferConstruction = true;
		ABullyCombatPawn* E = World->SpawnActor<ABullyCombatPawn>(ABullyCombatPawn::StaticClass(), *P + FVector(0, 0, 20.f), FRotator::ZeroRotator, SP);
		if (!E) { continue; }
		if (!A.Mesh.IsEmpty()) { E->PedMeshPath = FString::Printf(TEXT("/Game/Imported/Characters/%s/SK_%s.SK_%s"), *A.Mesh, *A.Mesh, *A.Mesh); }
		E->bStationary = !A.bHostile;
		E->HitsToBeat = A.HitsToBeat;
		// "target": every other member fights the named defender instead of the player (the pack splits, like the original)
		if (!A.Sub.IsEmpty() && G.Members.Num() % 2 == 0)
		{
			if (ABullyCombatPawn* Victim = DefenderById(FName(*A.Sub))) { E->PreferredVictim = Victim; }
		}
		E->FinishSpawning(FTransform(FRotator::ZeroRotator, *P + FVector(0, 0, 20.f)));
		if (A.Health > 0.f && E->GetCombat()) { E->GetCombat()->SetMaxHealth(A.Health); }   // boss health pools (1_B)
		G.Members.Add(E);
	}
	Waves.Add(MoveTemp(G));
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION spawn_wave %s: %d member(s), hostile=%d hits_to_beat=%d advance_at=%d -> event wave:%s"),
		*A.Name.ToString(), Waves.Last().Members.Num(), A.bHostile ? 1 : 0, A.HitsToBeat, A.AdvanceAt, *A.Name.ToString());
}

void UMissionSubsystem::TickWaves()
{
	for (FWaveGroup& G : Waves)
	{
		if (G.bFired) { continue; }
		int32 Down = 0;
		for (const TWeakObjectPtr<ABullyCombatPawn>& M : G.Members)
		{
			const ABullyCombatPawn* P = M.Get();
			if (!P || !P->GetCombat() || P->GetCombat()->IsOut() || P->GetCombat()->GetState() == EBullyCombatState::Beaten) { ++Down; }
		}
		const int32 Need = G.AdvanceAt > 0 ? FMath::Min(G.AdvanceAt, G.Members.Num()) : G.Members.Num();
		if (Need > 0 && Down >= Need)
		{
			G.bFired = true;
			const FName Ev(*FString::Printf(TEXT("wave:%s"), *G.Group.ToString()));
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION wave %s: %d/%d down -> event %s"), *G.Group.ToString(), Down, G.Members.Num(), *Ev.ToString());
			FireEvent(Ev);
		}
	}
}

void UMissionSubsystem::TickSpotters()
{
	if (Spotters.Num() == 0) { return; }
	const APawn* P = GetPlayerPawn();
	if (!P || !GetWorld()) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	for (FSpotter& S : Spotters)
	{
		if (S.bFired) { continue; }
		ABullyScriptedNPC* N = S.Npc.Get();
		if (!N) { S.bFired = true; continue; }
		const FVector NP = N->GetActorLocation();
		const FVector PP = P->GetActorLocation();
		bool bSeen = false;
		if (FVector::Dist2D(NP, PP) <= S.Range && FMath::Abs(NP.Z - PP.Z) <= 500.f)
		{
			const FVector ToP = (PP - NP).GetSafeNormal2D();
			if (FVector::DotProduct(N->GetActorForwardVector().GetSafeNormal2D(), ToP) >= S.CosHalf)
			{
				// eye-to-chest line of sight; world geometry blocks, props/foliage do not (v1)
				FHitResult H;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(BullySpotter), true);
				Q.AddIgnoredActor(N);
				bSeen = !GetWorld()->LineTraceSingleByObjectType(H, NP + FVector(0, 0, 150), PP + FVector(0, 0, 90), FCollisionObjectQueryParams(ECC_WorldStatic), Q);
			}
		}
		if (bSeen)
		{
			if (S.SeenSince < 0.0) { S.SeenSince = Now; }
			if (Now - S.SeenSince >= S.Seconds)
			{
				S.bFired = true;
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION spotter %s saw the player -> event %s"), *N->NpcId, *S.Event.ToString());
				FireEvent(S.Event);
			}
		}
		else { S.SeenSince = -1.0; }
	}
}

void UMissionSubsystem::DespawnWaves(FName Group)
{
	for (int32 i = Waves.Num() - 1; i >= 0; --i)
	{
		if (!Group.IsNone() && Waves[i].Group != Group) { continue; }
		for (const TWeakObjectPtr<ABullyCombatPawn>& M : Waves[i].Members)
		{
			if (M.IsValid()) { M->Destroy(); }
		}
		Waves.RemoveAt(i);
	}
}

void UMissionSubsystem::TickFollows(float /*Dt*/)
{
	if (Follows.Num() == 0) { return; }
	const APawn* P = GetPlayerPawn();
	if (!P) { return; }
	for (int32 i = Follows.Num() - 1; i >= 0; --i)
	{
		ABullyScriptedNPC* N = Follows[i].Npc.Get();
		if (!N) { Follows.RemoveAt(i); continue; }
		if (FVector::Dist2D(N->GetActorLocation(), P->GetActorLocation()) <= Follows[i].Radius) { continue; }   // close enough
		if (!N->IsBusy()) { N->GoToPoint(P->GetActorLocation(), GaitFor(Follows[i].Gait, true), 70.f, FString::Printf(TEXT("mission %s follow"), *N->NpcId)); }
	}
}

void UMissionSubsystem::TickCountdown()
{
	if (!Countdown.bActive) { return; }
	if (GetWorld()->GetTimeSeconds() >= Countdown.EndsAt)
	{
		Countdown.bActive = false;
		if (Hud.IsValid()) { Hud->SetCountdown(-1.f); }
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION countdown expired -> event %s"), *Countdown.Event.ToString());
		FireEvent(Countdown.Event);
	}
}

void UMissionSubsystem::TickRace()
{
	if (!Race.bActive || Race.Wps.Num() == 0) { return; }	const APawn* P = GetPlayerPawn();
	if (!P) { return; }
	const FVector PP = P->GetActorLocation();
	const FVector& Wp = Race.Wps[Race.NextIdx];
	if (FVector::Dist2D(PP, Wp) <= Race.Radius && FMath::Abs(PP.Z - Wp.Z) <= 500.f)
	{
		const int32 Done = Race.NextIdx + 1;
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION race '%s': checkpoint %d/%d (lap %d/%d)"), *Race.Id.ToString(), Done, Race.Wps.Num(), Race.Lap, Race.Laps);
		++Race.NextIdx;
		if (Race.NextIdx >= Race.Wps.Num())
		{
			Race.NextIdx = 0;
			++Race.Lap;
			if (Race.Lap > Race.Laps)
			{
				Race.bActive = false;
				const FName WinEv(*FString::Printf(TEXT("race:%s_win"), *Race.Id.ToString()));
				UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION race '%s': player finished -> event %s"), *Race.Id.ToString(), *WinEv.ToString());
				FireEvent(WinEv);
			}
		}
	}
}

void UMissionSubsystem::TickClockFail()
{
	if (!ClockFail.bActive) { return; }
	const UBullyClockSubsystem* Clk = UBullyClockSubsystem::Get(GetWorld());
	if (!Clk) { return; }
	// circular hours: fire once the clock has run from the arming time up to the target (wraps midnight)
	auto CircDist = [](float From, float To) { float D = To - From; return D < 0.f ? D + 24.f : D; };
	if (CircDist(ClockFail.ArmedHour, Clk->GetHourFloat()) >= CircDist(ClockFail.ArmedHour, ClockFail.TargetHour))
	{
		ClockFail.bActive = false;
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION clock reached %.2f (fail edge) -> event clock:fail"), ClockFail.TargetHour);
		FireEvent(FName("clock:fail"));
	}
}

void UMissionSubsystem::TickLesson()
{
	if (!Lesson.bActive || !GetWorld()) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	// a performed move arrives as a mission event (player_move:<Move>); consume it so each performance counts once
	const FName MoveEv(*FString::Printf(TEXT("player_move:%s"), *Lesson.Move.ToString()));
	if (StageEvents.Contains(MoveEv))
	{
		StageEvents.Remove(MoveEv);
		++Lesson.Count;
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION lesson '%s': %s %d/%d"), *Lesson.Id.ToString(), *Lesson.Move.ToString(), Lesson.Count, Lesson.Needed);
		if (Hud.IsValid()) { Hud->SetProgress(float(Lesson.Count) / Lesson.Needed, FString::Printf(TEXT("%s  %d / %d"), *Lesson.Prompt, Lesson.Count, Lesson.Needed)); }
		if (Lesson.Count >= Lesson.Needed)
		{
			Lesson.bActive = false;
			if (Hud.IsValid()) { Hud->SetProgress(-1.f); }
			const FName Done(*FString::Printf(TEXT("lesson:%s_done"), *Lesson.Id.ToString()));
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION lesson '%s' complete -> event %s"), *Lesson.Id.ToString(), *Done.ToString());
			FireEvent(Done);
			return;
		}
		Lesson.WindowEndsAt = Now + Lesson.Window;   // a success re-opens the attempt window
	}
	if (Now >= Lesson.WindowEndsAt)   // window expired with no performance: re-prompt (the original taunts; no fail)
	{
		Lesson.WindowEndsAt = Now + Lesson.Window;
		if (Hud.IsValid()) { Hud->ShowHint(Lesson.Prompt, 3.f); }
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION lesson '%s': re-prompt (%d/%d)"), *Lesson.Id.ToString(), Lesson.Count, Lesson.Needed);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// wave-D runtime: defenders (combat-pawn escortees) + dialogue choices

ABullyCombatPawn* UMissionSubsystem::DefenderById(FName Id) const
{
	const FDefenderState* D = Defenders.Find(Id);
	return (D && D->Pawn.IsValid()) ? D->Pawn.Get() : nullptr;
}

void UMissionSubsystem::TickDefenders(float /*Dt*/)
{
	if (Defenders.Num() == 0) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	for (auto& Pair : Defenders)
	{
		FDefenderState& D = Pair.Value;
		ABullyCombatPawn* P = D.Pawn.Get();
		if (!P) { continue; }   // (his KO fires through the combat component's tag hook)
		if (D.bDrain && Now - D.LastDrain >= D.DrainEvery && P->GetCombat() && !P->GetCombat()->IsOut())
		{
			D.LastDrain = Now;
			P->GetCombat()->ApplyWeaponHit(EBullyHitReaction::Flinch, D.DrainHp);   // scripted pressure during the approach
		}
	}
}

void UMissionSubsystem::DespawnDefenders()
{
	for (auto& Pair : Defenders)
	{
		if (Pair.Value.Pawn.IsValid()) { Pair.Value.Pawn->Destroy(); }
	}
	Defenders.Reset();
}

bool UMissionSubsystem::ChooseDialogueOption(int32 Index)
{
	if (!Choice.bActive || !Choice.Options.IsValidIndex(Index)) { return false; }
	const FChoiceOption& O = Choice.Options[Index];
	if (!O.RequiresItem.IsNone())
	{
		UBullyProgressSubsystem* Prog = UBullyProgressSubsystem::Get(GetWorld());
		if (!Prog || Prog->GetItemCount(O.RequiresItem) <= 0)
		{
			if (Hud.IsValid()) { Hud->ShowHint(TEXT("You don't have that."), 2.5f); }
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION dialogue_choice '%s': option %d needs item %s"), *Choice.Id.ToString(), Index + 1, *O.RequiresItem.ToString());
			return false;
		}
	}
	Choice.bActive = false;
	if (Hud.IsValid()) { Hud->ClearChoice(); }
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION dialogue_choice '%s': picked %d (%s) -> event %s"), *Choice.Id.ToString(), Index + 1, *O.Label, *O.Event.ToString());
	FireEvent(O.Event);
	return true;
}

void UMissionSubsystem::TickChoice()
{
	if (!Choice.bActive) { return; }
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) { return; }
	if (PC->WasInputKeyJustPressed(EKeys::J) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Left)) { ChooseDialogueOption(0); }
	else if (PC->WasInputKeyJustPressed(EKeys::K) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up)) { ChooseDialogueOption(1); }
	else if (PC->WasInputKeyJustPressed(EKeys::L) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Right)) { ChooseDialogueOption(2); }
}

void UMissionSubsystem::TickBossBar()
{
	ABullyCombatPawn* B = BossBarState.Boss.Get();
	if (!B || !B->GetCombat()) { return; }
	UBullyCombatComponent* C = B->GetCombat();
	if (C->IsOut())
	{
		if (Hud.IsValid()) { Hud->SetProgress(-1.f); }
		BossBarState.Boss.Reset();
		return;
	}
	const float Frac = BossBarState.Max > 0.f ? C->Health / BossBarState.Max : 0.f;
	if (Hud.IsValid()) { Hud->SetProgress(Frac, BossBarState.Label); }
	if (!BossBarState.bHalfFired && Frac < 0.5f)
	{
		BossBarState.bHalfFired = true;
		if (Hud.IsValid() && !BossBarState.HalfLine.IsEmpty()) { Hud->Say(BossBarState.Label, BossBarState.HalfLine, 2.5f); }
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION boss_bar %s under half"), *BossBarState.Label);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// console

namespace
{
	UMissionSubsystem* SysFor(UWorld* W)
	{
		UMissionSubsystem* S = UMissionSubsystem::Get(W);
		if (!S) { UE_LOG(LogBullyMission, Warning, TEXT("BR_MISSION: no mission subsystem in this world")); }
		return S;
	}

	FAutoConsoleCommandWithWorldAndArgs GStart(TEXT("Mission.Start"), TEXT("Mission.Start <id> [stage#]: load Config/Missions/<id>.json and begin (dev override: bypasses the campaign availability gate)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W)
		{
			if (UMissionSubsystem* S = SysFor(W)) { if (A.Num() >= 1) { S->StartMission(A[0], A.Num() >= 2 ? FCString::Atoi(*A[1]) - 1 : 0, /*bDevOverride*/ true); } else { UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION usage: Mission.Start <id> [stage#]")); } }
		}));
	FAutoConsoleCommandWithWorldAndArgs GStop(TEXT("Mission.Stop"), TEXT("Stop the active mission"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W) { if (UMissionSubsystem* S = SysFor(W)) { S->StopMission(TEXT("console")); } }));
	FAutoConsoleCommandWithWorldAndArgs GSkip(TEXT("Mission.Skip"), TEXT("Mission.Skip [next|stage#|stageId]: jump to a stage"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W) { if (UMissionSubsystem* S = SysFor(W)) { S->SkipToStage(A.Num() ? A[0] : TEXT("next")); } }));
	FAutoConsoleCommandWithWorldAndArgs GFire(TEXT("Mission.Fire"), TEXT("Mission.Fire <event>: fire a mission event"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W) { if (UMissionSubsystem* S = SysFor(W)) { if (A.Num()) { S->FireEvent(FName(*A[0])); } } }));
	FAutoConsoleCommandWithWorldAndArgs GFail(TEXT("Mission.Fail"), TEXT("Mission.Fail [reason]: fail the current stage (triggers retry)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W) { if (UMissionSubsystem* S = SysFor(W)) { S->FailMission(A.Num() ? FString::Join(A, TEXT(" ")) : TEXT("console")); } }));
	FAutoConsoleCommandWithWorldAndArgs GInteract(TEXT("Mission.Interact"), TEXT("Press the interact button (as if E)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W) { if (UMissionSubsystem* S = SysFor(W)) { S->DoInteract(); } }));
	FAutoConsoleCommandWithWorldAndArgs GGoto(TEXT("Mission.Goto"), TEXT("Teleport the player onto the current stage's target"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W) { if (UMissionSubsystem* S = SysFor(W)) { S->GotoStageTarget(); } }));
	FAutoConsoleCommandWithWorldAndArgs GStatus(TEXT("Mission.Status"), TEXT("Log the mission state"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W) { if (UMissionSubsystem* S = SysFor(W)) { S->LogStatus(); } }));
	FAutoConsoleCommandWithWorldAndArgs GList(TEXT("Mission.List"), TEXT("List Config/Missions/*.json"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld*)
		{
			for (const FString& Id : UMissionSubsystem::ListMissionIds()) { UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION available: %s"), *Id); }
		}));
}

// ---------------------------------------------------------------------------------------------------------------
// sm-cutscenes: original baked cutscenes (cutscene action)

void UMissionSubsystem::RunCutsceneAction(const FMissionAction& A)
{
	const FString Id = A.Name.ToString();
	const FName Ev(*A.Sub);
	UBullyCutsceneSubsystem* CS = UBullyCutsceneSubsystem::Get(GetWorld());
	const UBullyCutsceneSubsystem::ETier Tier = CS ? CS->Probe(Id) : UBullyCutsceneSubsystem::ETier::None;
	if (Tier == UBullyCutsceneSubsystem::ETier::None)
	{
		if (A.Else.Num() > 0)
		{
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION cutscene %s not imported: running the %d fallback beat(s)"), *Id, A.Else.Num());
			RunActions(A.Else);
		}
		else
		{
			UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION cutscene %s not imported: passing through -> event %s"), *Id, *Ev.ToString());
			FireEvent(Ev);
		}
		return;
	}
	if (Tier == UBullyCutsceneSubsystem::ETier::Audio && A.Else.Num() > 0)
	{
		// original audio + subtitles over the fallback's staging: its camera/letterbox/fade/npc beats run, its text does not
		TArray<FMissionAction> Staging;
		for (const FMissionAction& E : A.Else)
		{
			using T = FMissionAction::EType;
			if (E.Type != T::Say && E.Type != T::SpeechSequence && E.Type != T::Hint && E.Type != T::Banner) { Staging.Add(E); }
		}
		RunActions(Staging);
	}
	EnsureHud();
	SetFlag(FName(*FString::Printf(TEXT("cutscene:%s"), *Id)), true);
	TWeakObjectPtr<UMissionSubsystem> WeakThis(this);
	const FString MissionId = Mission.IsValid() ? Mission->Id : FString();
	const UBullyCutsceneSubsystem::ETier Played = CS->Play(Id, Hud, [WeakThis, Ev, MissionId](bool bSkipped)
	{
		UMissionSubsystem* M = WeakThis.Get();
		if (!M || !M->Mission.IsValid() || M->Mission->Id != MissionId || M->State != EState::Running) { return; }
		UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION cutscene %s -> event %s"), bSkipped ? TEXT("skipped") : TEXT("finished"), *Ev.ToString());
		M->FireEvent(Ev);
	});
	UE_LOG(LogBullyMission, Display, TEXT("BR_MISSION cutscene %s playing (%s tier)"), *Id,
		Played == UBullyCutsceneSubsystem::ETier::Full ? TEXT("full") : Played == UBullyCutsceneSubsystem::ETier::Audio ? TEXT("audio") : TEXT("none"));
	if (Played == UBullyCutsceneSubsystem::ETier::None) { FireEvent(Ev); }
}

void UMissionSubsystem::StopCutscene()
{
	if (UBullyCutsceneSubsystem* CS = UBullyCutsceneSubsystem::Get(GetWorld())) { CS->Stop(); }
}
