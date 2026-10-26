// OWNER: mission-fw
// Data-driven mission / objective framework. A mission is a JSON file (Config/Missions/<id>.json): named points,
// trigger volumes and an ordered list of stages. The subsystem runs the stage machine: show the stage's objective +
// waypoint on the HUD, poll its completion condition (and optional fail condition) every frame, run actions on
// start/complete, advance, and on failure reload the stage (or the whole mission). Nothing about a mission is
// hardcoded; combat/NPC systems plug in by firing events (FireEvent) that "event" conditions listen for, and by
// subscribing to the OnStage*/OnMission* delegates.
//
// Schema (all coordinates UE cm; "game_m": [x, y, z] in game metres is accepted anywhere a location is, UE = (100x, -100y, 100z)):
//
//   { "_format": "bully-remake.mission/1", "id": "...", "title": "...",
//     "retry": "stage" | "mission" | "none",  "fail_delay": 2.5,
//     "start": { "location_cm": [x,y,z], "yaw": 0 }          // optional: teleport the player here when the mission starts
//     "trigger_file": "imported/triggers/day_one.json",       // optional extra named points (repo-relative); inline wins
//     "points":   { "name": { "location_cm": [x,y,z] } },
//     "triggers": [ { "id": "...", "center_cm": [..], "half_extent_cm": [..], "yaw": 0, "interact": false, "prompt": "..." } ],
//     "stages": [ { "id": "...", "objective": "...",
//                   "blip": "<point|trigger id>" | [x,y,z] | false,   // default: derived from the complete condition
//                   "complete": <condition>, "fail": <condition>,     // fail optional
//                   "checkpoint": { "location_cm": [..], "yaw": 0 },  // where a stage retry puts the player (default: where the stage began)
//                   "on_start": [<action>], "on_complete": [<action>] } ],
//     "on_complete": [<action>] }
//
//   conditions: { "type": "reach_location", "point": "name" | "location_cm": [..], "radius_cm": 300, "z_tolerance_cm": 400 }
//               { "type": "trigger_enter" | "trigger_exit", "trigger": "id" }
//               { "type": "interact", "trigger": "id" | "actor_tag": "tag", "radius_cm": 250, "prompt": "Open the door",
//                                       "money": 2, "set_flag": "player_hidden" }   // money: wallet must cover it (taken on
//                                                                                    //  fire; short = hint, no fire). set_flag:
//                                                                                    //  latch a mission flag when it fires.
//                                                                                    //  "social" is an alias with the same fields.
//               { "type": "event", "name": "...", "latched": false }   // latched = also true if fired earlier in the mission
//               { "type": "timer", "seconds": 5, "visible": true }     // visible: HUD countdown while it runs (use on a fail timer)
//               { "type": "flag", "name": "..." }
//               { "type": "has_item", "name": "chocolates" }           // session item inventory (BullyProgressSubsystem)
//               { "type": "attempt", "n": 1, "eq": true }              -- campaign attempt count for this mission (gte unless eq)
//               { "type": "leave_area", "area": 2 }                    -- player outside the trigger-file area's bounds
//                                                                         (bbox of that area's trigger-file points + margin)
//               { "type": "all" | "any", "conditions": [ ... ] }
//   stage extras: "fail_text": "..." (the fail banner's reason line); a root-level "fail" condition + "fail_text" on the
//   mission itself is evaluated every tick of every stage (e.g. 1_02B's leave-the-school fail).
//
//   NOTE for the smoke driver: events whose names contain ':' are system-produced (speech:<id>, heat:zero,
//   npc_arrived:<id>, minigame ids, npc_ko:<id>) and are never auto-fired by the driver; it exercises the producing
//   system instead (skips the speech beat, hold-drives the minigame, hides and waits out the heat decay).
//   actions:    { "type": "hint", "text": "...", "seconds": 5 } | { "type": "banner", "text": "...", "sub": "..." }
//               { "type": "event", "name": "..." } | { "type": "set_flag" | "clear_flag", "name": "..." }
//               { "type": "teleport", "point": "name" | "location_cm": [..], "yaw": 0 } | { "type": "log", "text": "..." }
//               { "type": "start_mission", "mission": "<id>", "seconds": 1.0 }   // chain into the next mission after a beat
//               { "type": "spawn_enemy", "distance_cm": 300, "health_floor": 5, "player_invulnerable": true,
//                                        "gate_attack": true, "gate_block": true,
//                                        "mesh": "/Game/Imported/Characters/PFH1_Seth/SK_PFH1_Seth.SK_PFH1_Seth" }  // sparring partner / cutscene stand-in (mesh optional)
//               { "type": "fade", "out": 0.6, "hold": 0.5, "in": 0.9,              // cinematic fade-to-black and back;
//                                 "point": "name" | "location_cm": [..], "yaw": 0 } // with a point, teleport at full black
//               { "type": "reward", "money": 75, "respect": 15, "text": "..." }     // pay the persistent wallet + toast
//               { "type": "letterbox", "on": true }                                 // slide cinematic bars in (true) / out (false)
//               { "type": "say", "speaker": "PREFECT", "text": "...", "seconds": 4 } // one cutscene dialogue line at the bottom
//               { "type": "camera", "point": "name" | "location_cm": [..],           // cut/blend to a scripted cutscene camera
//                                   "look": "player" | "<point>" | "look_cm": [..],  //   aimed at the player, a point, or a spot
//                                   "fov": 50, "blend": 1.2 }                         //   (blend 0 = hard cut)
//               { "type": "camera", "release": true, "blend": 0.8 }                  // hand the view back to the player's camera
//               { "type": "shake", "amp": 1.0, "seconds": 0.3 }                      // kick the player's camera (impact beats)
//               { "type": "give_weapon", "weapon": 303 | "slingshot" }               // campaign grant (numeric = original game id)
//               { "type": "give_ammo", "weapon": 309 | "stinkbomb", "n": 5 }         // top the weapon's ammo up to n
//               { "type": "set_respect", "faction": "bullies", "value": 20 }         // set a faction's respect (campaign table)
//               { "type": "clock", "set": "8:30" } | { "type": "clock", "pause": true } | { "type": "clock", "unpause": true }
//               { "type": "chapter_set", "chapter": 2 }                              // advance the campaign chapter
//               { "type": "force_complete", "mission": "id" | "missions": ["id",..] } // mark passed without playing (applies on_pass)
//
//   campaign-spine chapter-1 primitives (1_02B/1_02C):
//               { "type": "spawn_npc", "npc": "gary", "point": "name", "mesh": "GN_Boy01",  // friendly scripted NPC
//                 "yaw": 0, "mission_critical": true, "invulnerable": true, "min_health": 0, // (ABullyScriptedNPC; a
//                 "untargetable": true, "face": "player", "tag": "br_npc_gary",              //  critical NPC expiring
//                 "ped_model_id": 130 }                                                     //  fires "npc_ko:<id>").
//                 spawn_npc with an existing id REUSES it (re-positioned) so stage retries don't duplicate.
//               { "type": "npc_move_to", "npc": "gary", "point": "name", "run": true,        // walk/run there; fires
//                 "event": "npc_arrived:gary" }                                              // the event on arrival
//               { "type": "npc_lead", "npc": "gary", "point": "name", "radius_cm": 600 }     // lead the player: pause
//                                                                                            //  while he trails behind
//               { "type": "npc_face", "npc": "gary", "face": "player" | "point": "name" }
//               { "type": "npc_despawn", "npc": "gary" }
//               { "type": "pair_anim", "npc": "eunice", "seconds": 1.5, "clips": "A,B" }     // v1 STUB: the pair face
//                                                                                            //  each other; the NPC plays
//                                                                                            //  a generic clip; timed
//               { "type": "speech_sequence", "id": "russell", "lines": [{ "speaker": "GARY", // ordered multi-speaker
//                 "text": "...", "seconds": 3 }] }                                           //  lines; Enter/pad-B skips
//                                                                                            //  to the end; fires
//                                                                                            //  "speech:<id>" when done
//               { "type": "minigame_launch", "id": "lockpick", "point": "name" | "trigger": "id", "seconds": 4,
//                 "success": "minigame:lockpick_ok", "fail": "minigame:lockpick_fail", "text": "Picking lock..." }
//                 -- v1 STUB: press E in range to start a timed progress bar; stay in range until it fills (success),
//                    step out early (fail). NOT the original's dial minigame.
//               { "type": "give_item", "item": "chocolates", "n": 1 } | { "type": "take_item", ... }  // session inventory
//               { "type": "money_ensure", "money": 2 } | { "type": "money_take", "money": 2 }          // wallet floor / take
//               { "type": "heat_set", "points": 200, "decay": 25 }   // punishment-lite STUB: heat points, decaying
//                                                                    //  `decay`/s only while the player is hidden;
//                                                                    //  fires "heat:zero" at 0
//               { "type": "hide", "on": true }                       // the player ducks into a hiding spot
//
//   chapter-1 wave-B primitives (1_03 chase/boss, 1_04 sniping):
//               { "type": "spawn_wave", "group": "w1", "points": ["pt", ...], "mesh": "GN_Bully01",
//                 "health": 60, "hostile": true, "hits_to_beat": 0, "advance_at": 0 }
//                 -- hostile combat pawns at named points (hostile:false = stationary target dummies); fires
//                    "wave:<group>" when advance_at members are down (0 = all). hits_to_beat N: scripted hit-count
//                    health (N landed hits drop him, damage number irrelevant — the Davis boss model).
//                    "hits_ranged_only": true -- only weapon hits use that count; melee does normal damage (1_04 jocks).
//                    Object destruction (1_04's bus windows) is a v1 stand-in: stationary 1-hp dummies.
//               { "type": "spotter", "npc": "burton", "range_cm": 1500, "cone_deg": 60, "seconds": 2 }
//                 -- stealth spotter on a spawned scripted npc: player in range + view cone + line of sight for
//                    `seconds` (continuous) fires "spotted_by:<id>" (once per stage arming).
//               npc_move_to / npc_lead gain "via": ["pt", ...] (inline/file point names) and "path": "<trigger-file
//               path name>" — the move follows those waypoints (the Trigger.img paths are nav-tested routes).
//   conditions: ... plus
//               { "type": "leash", "npc": "davis", "radius_cm": 3500, "seconds": 20 }
//                 -- chase leash (use as a fail): true after the named scripted npc is farther than radius from the
//                    player for `seconds` continuously.
//               { "type": "zone_timer", "trigger": "id", "seconds": 15, "visible": true }
//                 -- zone-exit countdown (the 1_04 tree perch): true after the player has been OUTSIDE the trigger
//                    for `seconds` continuously; re-entering cancels. Only arms after the player has been inside at
//                    least once this stage. "visible" shows the countdown on the HUD while out.
//   conditions: ... plus
//   chapter-1 wave-C primitives (1_05 escort, 1_06_01 hobo lesson):
//               { "type": "npc_follow", "npc": "algie", "radius_cm": 300, "gait": "run" }   // escort follow: trails
//                 the player, stopping within radius; mission-scoped (survives stage changes), cosmetic-safe — never
//                 gate a stage on a follower's arrival without a player-side alternative.
//               { "type": "countdown_start", "seconds": 180, "visible": true, "event": "bladder_out",
//                 "resume": true }   // mission-scoped countdown (Algie's bladder): fires the event at 0; "resume"
//                                    // keeps the remaining time if one is already running (per-stage re-arms don't
//                                    // reset it). Re-arming un-latches a previously fired event (retry-safe).
//               { "type": "countdown_cancel" }                         // stops it (goal reached in time)
//               { "type": "lesson_start", "id": "jab", "move": "Jab1", "count": 3, "seconds": 10,
//                 "prompt": "Light attack (LMB)" }   // fight-lesson framework v1: perform the named combat move
//                 // `count` times; each attempt window (seconds) expiring just re-prompts (the original's taunt loop).
//                 // Fires "lesson:<id>_done". Player moves report as "player_move:<Move>" mission events.
//                 // (The original's per-button sequence widget is a later wave; v1 = perform-the-move.)
//   actions: ...
//   chapter-1 wave-D primitives (1_07 defend, 1_08 conversation):
//               { "type": "spawn_defender", "npc": "bucky", "point": "name", "mesh": "NDH3_Bucky", "health": 300,
//                 "drain_hp": 25, "drain_every": 5 }   // a combat pawn on the PLAYER's team (enemies can hit him,
//                 //  the player's swings skip him): stationary, never attacks, tagged br_npc:<id> (his KO fires
//                 //  npc_ko:<id>). drain_* = scripted health pressure (the original's 25 hp / 5 s during the approach);
//                 //  toggle with npc_drain. A stage retry revives him (reposition + full health + un-latch npc_ko).
//               { "type": "npc_drain", "npc": "bucky", "on": false }
//               spawn_wave gains "target": "bucky" — every other member (even indices) takes that mission defender as
//               its PreferredVictim (the rest fight the player), so a pack splits like the original's.
//               { "type": "dialogue_choice", "id": "fatty", "prompt": "...", "options": [
//                 { "label": "Hand over the chocolate", "event": "dlg:pay", "requires_item": "chocolate" },
//                 { "label": "Bully him", "event": "dlg:bully" } ] }   // HUD option list; J/K/L or D-pad left/up/right
//                 //  pick; requires_item greys the option (a hint instead of firing). Stage-scoped; a retry re-arms.
//   chapter-1 wave-F primitive (1_B's Russell):
//               { "type": "boss_bar", "group": "boss", "label": "RUSSELL", "half_line": "Russell is furious!" }
//                 -- boss health bar (drives the HUD progress bar off the named one-member wave) + a one-time bark
//                    when he drops under half (the original's 50% anger flag, as a readout instead of a phase).
//   sm-cutscenes (original baked cutscenes, UBullyCutsceneSubsystem / BullyCutscene.h):
//               { "type": "cutscene", "id": "1-03", "event": "speech:intro", "else": [<action>, ...] }
//                 -- plays the imported original (tools.importers `cutscene build`): Full tier = decoded camera + actors
//                    + original audio + subtitles; Audio tier (no actor assets) = original audio + subtitles over the
//                    `else` beats' staging (camera/letterbox/fade/teleport/npc actions run, their say/speech/hint/banner
//                    text does not). Skippable (Enter / pad B). Fires `event` (default "cutscene:<id>") when it ends and
//                    latches the mission flag "cutscene:<id>". Content absent: runs `else` (and fires nothing), or with no
//                    `else` fires the event at once -- a dedicated cutscene stage then just passes through.
//   stage extra: "skip_if": <condition> -- checked when the stage begins; true skips the stage outright (no actions), e.g.
//                    { "type": "flag", "name": "cutscene:1-1-1" } on the text beats an original cutscene replaces.
//
// Campaign spine: StartMission consults UBullyCampaignSubsystem::IsAvailable (ids outside the campaign table are
// always available) unless the start came from -BRMission=, the Mission.Start console exec, or a smoke/shots
// harness (bDevOverride — harness runs must keep working); star-marker starts go through the gate. CompleteMission
// feeds Campaign.NotifyPassed(id) (the SMissPass-style on_pass block) and counts attempts per mission id.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "InputCoreTypes.h"   // sm-humandrive: FKey (human driver key presses)
#include "MissionSubsystem.generated.h"

class AMissionTrigger;
class FBullyHUD;
class APawn;
class ACameraActor;
class ABullyScriptedNPC;
class ABullyCombatPawn;
struct FSNPCCommand;
struct FInputActionValue;

struct FMissionCondition
{
	enum class EType : uint8 { None, Reach, TriggerEnter, TriggerExit, Interact, Event, Timer, Flag, All, Any, HasItem, Attempt, LeaveArea, Leash, ZoneTimer };
	EType Type = EType::None;
	FVector Location = FVector::ZeroVector;   // Reach
	float Radius = 300.f;                     // Reach radius / Interact actor range
	float ZTolerance = 400.f;
	FName Name;                               // trigger id / event name / flag / actor tag / item id (has_item)
	bool bActorTag = false;                   // Interact: Name is an actor tag instead of a trigger id
	bool bLatched = false;                    // Event
	float Seconds = 0.f;                      // Timer
	FString Prompt;                           // Interact
	TArray<FMissionCondition> Children;       // All / Any
	// campaign spine: Interact/Social extras (money gate + flag latch), Attempt (attempt-count gate),
	// LeaveArea (area bounds from the trigger file), Timer "visible" (HUD countdown)
	int32 MoneyCost = 0;
	FName FlagToSet;
	int32 AttemptN = 0;
	bool bAttemptEq = false;
	int32 Area = -1;
	bool bVisible = false;
	bool IsValid() const { return Type != EType::None; }
};

/** One line of a speech_sequence action. */
struct FSpeechLine
{
	FString Speaker, Text;
	float Seconds = 4.f;
};

/** One option of a dialogue_choice action. */
struct FChoiceOption
{
	FString Label;
	FName Event;
	FName RequiresItem;   // option fires only while the player holds one (else a hint)
};

struct FMissionAction
{
	enum class EType : uint8 { Hint, Banner, Event, SetFlag, ClearFlag, Teleport, Log, StartMission, SpawnEnemy, Fade, Reward, Letterbox, Say, Camera, Shake, GiveWeapon, GiveAmmo, SetRespect, Clock, ChapterSet, ForceComplete,
		SpawnNpc, NpcMove, NpcLead, NpcFace, NpcDespawn, PairAnim, SpeechSequence, MinigameLaunch, GiveItem, TakeItem, MoneyEnsure, MoneyTake, HeatSet, Hide,
		SpawnWave, Spotter, NpcFollow, CountdownStart, CountdownCancel, LessonStart, SpawnDefender, NpcDrain, DialogueChoice, BossBar, RaceStart, ClockFailAt,
		Cutscene /* sm-cutscenes */ };
	EType Type = EType::Log;
	FString Text, Sub;
	FName Name;
	float Seconds = 5.f;
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
	// start_mission: Text = mission id, Seconds = delay before it starts (so the complete banner can show).
	// spawn_enemy: spawn a sparring partner in front of the player and (optionally) set the tutorial gates/invuln.
	float Dist = 300.f;          // spawn distance in front, cm
	float HealthFloor = 0.f;     // >0 = beaten + humiliation instead of KO
	int32 PlayerInvuln = -1;     // -1 leave as-is, 0 clear, 1 set Jimmy invulnerable
	int32 GateAttack = -1;       // -1 leave, 0/1 allow the spawned enemy to attack
	int32 GateBlock = -1;        // -1 leave, 0/1 allow the spawned enemy to block
	FString EnemyMesh;           // optional ped mesh override (/Game/Imported/Characters/<N>/SK_<N>.SK_<N>); empty = config default
	// fade: cinematic fade-to-black -> (optional) teleport the player at full black -> fade back in. Seconds each.
	// With a point/location, the teleport happens while the screen is black (no visible jump); Location/Yaw hold it.
	float FadeOut = 0.5f, FadeHold = 0.4f, FadeIn = 0.8f;
	bool bFadeTeleport = false;
	// reward: grant to the persistent wallet (UBullyProgressSubsystem) and toast it. Text = optional custom banner line.
	int32 Money = 0, Respect = 0;
	// letterbox: slide the cinematic bars in (bOn) or out. say: Sub = speaker label, Text = line, Seconds = how long it reads.
	bool bOn = true;
	// camera: Location = camera position; Look / bLookPlayer / bHasLook = aim; Fov (0 = leave); Blend = seconds (0 = hard cut).
	// bRelease hands the view back to the player's own camera.
	FVector Look = FVector::ZeroVector;
	bool bHasLook = false, bLookPlayer = false, bRelease = false;
	float Fov = 0.f, Blend = 0.5f;
	// shake: Amp = shake amplitude (1.0 = a firm hit), Seconds = duration.
	float Amp = 1.f;
	// campaign spine: give_weapon/give_ammo (WeaponId = original numeric game id, -1 = Text holds a Weapons.json id;
	// Amount = ammo count), set_respect (Name = faction, Amount = value), chapter_set (Amount), force_complete
	// (Name = mission id; a "missions" array expands to one action per id). clock: Text = "h:mm" to set,
	// Amount 1 = pause, 2 = unpause.
	int32 WeaponId = -1;
	int32 Amount = 0;
	// chapter-1 primitives: npc actions (NpcId = script id; spawn_npc: Text = ped mesh folder name, Location/Yaw = spawn,
	// flags below; npc_move_to/npc_lead: Location = goal, Sub = arrived-event override (default "npc_arrived:<id>"),
	// Dist = lead follow radius; npc_face: Location or bFacePlayer; npc_despawn: NpcId; pair_anim: NpcId + Text =
	// comma-separated clip candidates, Seconds). spawn_npc reusing an existing id repositions it (stage retries).
	FName NpcId;
	bool bMissionCritical = false, bInvulnerable = false, bUntargetable = false, bFacePlayer = false, bRun = false;
	float MinHealth = 0.f;
	FString Mesh;
	// speech_sequence: Lines play in order on the HUD; Enter / gamepad right face button skips to the end; fires
	// "speech:<id>" (Name = id) when done or skipped.
	TArray<FSpeechLine> Lines;
	// minigame_launch (v1 = timed hold-interact progress bar, NOT the original's dial): Name = id, trigger id in Sub
	// (optional), Location/Radius = the spot, Seconds = fill time, SuccessEvent/FailEvent, Text = HUD label.
	FName SuccessEvent, FailEvent;
	float Radius = 250.f;
	// heat_set: Amount = points, Amp reused as decay-per-second. hide: bOn. give_item/take_item: Name = item, Amount = n.
	// money_ensure/money_take: Money field (dollars, the wallet's unit).
	// spawn_wave: Name = group (fires "wave:<group>"), PointNames resolve against the mission points, Mesh = ped folder,
	// Health (0 = combat default), bHostile (false = stationary target dummy), HitsToBeat (boss hit-count health),
	// AdvanceAt members down to fire (0 = all). npc_move_to/npc_lead: Via = extra waypoints (point OR path names),
	// Text = a trigger-file path name. spotter: NpcId, Radius = range, Amp = cone degrees, Seconds = continuous spot time.
	// Gait: "walk" | "run" | "sprint" for the npc actions (run:true == "run").
	FString Gait;
	TArray<FString> PointNames;
	TArray<FString> Via;
	float Health = 0.f;
	bool bHostile = true;
	int32 HitsToBeat = 0;
	bool bHitsRangedOnly = false;   // spawn_wave "hits_ranged_only": hits_to_beat counts weapon hits only (melee = normal damage)
	int32 AdvanceAt = 0;
	// npc_follow: NpcId + Dist (stop radius, default 300) + Gait — mission-scoped escort follow. countdown_start:
	// Seconds + bResume ("resume": keep remaining time if running) + Name (expiry event, default "countdown:out") +
	// bCountdownVisible (HUD). lesson_start: Name = id (fires "lesson:<id>_done"), Sub = move name, Amount = count,
	// Seconds = per-attempt window, Text = HUD prompt.
	bool bResume = false, bCountdownVisible = false;
	// spawn_defender: NpcId + Location + Mesh + Health (max hp) + Amount (drain_hp) + Seconds (drain_every).
	// npc_drain: NpcId + bOn. spawn_wave: Sub = target defender id (every other member). dialogue_choice: Name = id,
	// Text = prompt, Options. race_start: Name = id (fires "race:<id>_win"), Sub = course (trigger-file path name),
	// Amount = laps (default 1), Radius = checkpoint radius (default 900). clock_fail_at: Text = "h:mm" (world-clock
	// fail edge, fires "clock:fail" once; mission-long, not stage-scoped).
	TArray<FChoiceOption> Options;
	// sm-cutscenes: cutscene -- Name = cutscene id (Cuts.img naming, e.g. "1-03"), Sub = done event (default
	// "cutscene:<id>"), Else = the fallback beats run when the imported original is absent.
	TArray<FMissionAction> Else;
};

struct FMissionStage
{
	FName Id;
	FString Objective;
	enum class EBlip : uint8 { Auto, None, Explicit } BlipMode = EBlip::Auto;
	FVector BlipLocation = FVector::ZeroVector;
	FMissionCondition Complete;
	FMissionCondition Fail;
	FString FailText;                         // optional fail-banner reason line (keyed fail text)
	bool bHasCheckpoint = false;
	FVector CheckpointLocation = FVector::ZeroVector;
	float CheckpointYaw = 0.f;
	TArray<FMissionAction> OnStart, OnComplete;
	FMissionCondition SkipIf;                 // sm-cutscenes: evaluated at stage begin; true = skip the stage (no actions)
};

struct FMissionTriggerDef
{
	FName Id;
	FVector Center = FVector::ZeroVector;
	FVector HalfExtent = FVector(100, 100, 150);
	float Yaw = 0.f;
	bool bInteract = false;
	FString Prompt;
};

struct FMissionDef
{
	FString Id, Title;
	enum class ERetry : uint8 { Stage, Mission, None } Retry = ERetry::Stage;
	float FailDelay = 2.5f;
	bool bHasStart = false;
	FVector StartLocation = FVector::ZeroVector;
	float StartYaw = 0.f;
	TMap<FName, FVector> Points;
	TArray<FMissionTriggerDef> Triggers;
	TArray<FMissionStage> Stages;
	TArray<FMissionAction> OnComplete;
	// root-level fail: evaluated every tick of every stage (e.g. leaving the school mid-mission)
	FMissionCondition RootFail;
	FString RootFailText;
	// mission_start_item_gate (2_G2): the player must carry this item to start (soft refuse: hint + no fail);
	// harnesses/dev bypass with the same bDevOverride as the availability gate. bRequiresConsume: take 1 on start.
	FName RequiresItem;
	bool bRequiresConsume = false;
	// trigger-file area bounds (area code -> bbox of that area's entities + margin), for leave_area conditions
	TMap<int32, FBox> AreaBoxes;
	// trigger-file paths (name -> waypoints), for npc_move_to/npc_lead "path"/"via" routes
	TMap<FName, TArray<FVector>> Paths;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FMissionStageDelegate, const FMissionDef& /*Mission*/, int32 /*StageIndex (0-based)*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMissionEventDelegate, FName /*Event*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMissionDoneDelegate, const FMissionDef& /*Mission*/, bool /*bSuccess*/);

UCLASS(Config = Game)
class UMissionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	enum class EState : uint8 { Idle, Running, Failed, Complete };

	/** Data-driven boot mission: the id auto-started when no -BRMission= is on the command line (empty = none).
	 *  Set in DefaultGame.ini under [/Script/BullyRemake.MissionSubsystem] (e.g. BootMission=day_one) so a packaged,
	 *  double-clickable build drops straight into that mission. -BRMission= on the command line always overrides this. */
	UPROPERTY(Config)
	FString BootMission;

	static UMissionSubsystem* Get(UWorld* World);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// --- control (console: Mission.Start / Stop / Skip / Fire / Fail / Interact / Goto / Status / List) ---
	/** bDevOverride bypasses the campaign availability gate: -BRMission=, the Mission.Start console exec and the
	 *  smoke/shots harnesses pass it; star-marker and chained starts go through the gate. */
	bool StartMission(const FString& Id, int32 StageIndex = 0, bool bDevOverride = false);
	void StopMission(const FString& Reason);
	/** `Arg` is a 1-based stage number, a stage id, or "next". Completes nothing in between (no actions run for skipped stages). */
	bool SkipToStage(const FString& Arg);
	void FailMission(const FString& Reason);
	/** Latches the event for the current stage (and the mission) and tells listeners. Combat/NPC code calls this. */
	void FireEvent(FName Event);
	/** The "press E" action: fires every interact condition of the current stage whose target is in range. Returns whether one fired. */
	bool DoInteract();
	void SetFlag(FName Flag, bool bOn);
	bool HasFlag(FName Flag) const { return Flags.Contains(Flag); }
	/** Test aid: teleport the player onto the current stage's target (if it has a positional condition). */
	bool GotoStageTarget();
	void LogStatus() const;
	static TArray<FString> ListMissionIds();

	/** Arm the smoke driver from code instead of the command line. With bQuitOnDone=false the driver logs its
	 *  PASS/FAIL line but hands control back instead of quitting the engine (-BRCampaignSmoke uses this to run the
	 *  day_one boot chain before its own star/clock checks). Harness starts are dev overrides of the campaign gate. */
	void DriveSmoke(const FString& Id, bool bQuitOnDone);

	// --- state ---
	EState GetState() const { return State; }
	bool IsActive() const { return State == EState::Running || State == EState::Failed; }
	const FMissionDef* GetMission() const { return Mission.IsValid() ? Mission.Get() : nullptr; }
	int32 GetStageIndex() const { return StageIndex; }
	int32 GetAttempt() const { return Attempt; }
	FBullyHUD* GetHUD() const { return Hud.Get(); }

	// --- story mode (BullyStorySubsystem) ---
	/** A start_mission chain / auto-start is queued (the player is mid-sequence, e.g. arrival -> day_one). */
	bool HasPendingStart() const { return !PendingAutoStart.IsEmpty(); }
	/** Make sure the HUD overlay exists and is shown, and return it (free-roam guidance draws on the mission HUD). */
	FBullyHUD* AcquireHud() { EnsureHud(); return Hud.Get(); }
	/** Set by story mode while free-roam guidance owns the HUD: the end-of-mission clear/hide is skipped. */
	bool bKeepHud = false;

	// --- triggers ---
	void RegisterTrigger(AMissionTrigger* T);
	void UnregisterTrigger(AMissionTrigger* T);
	void NotifyTrigger(AMissionTrigger* T, bool bEntered);

	FMissionStageDelegate OnStageBegan;
	FMissionStageDelegate OnStageCompleted;
	FMissionDoneDelegate OnMissionFinished;   // success or fail-without-retry/stop
	FMissionEventDelegate OnMissionEvent;     // every FireEvent + "trigger_enter:<id>" / "trigger_exit:<id>" / "interact:<key>"

private:
	bool LoadMission(const FString& Id, FMissionDef& Out, FString& Error) const;
	void BeginStage(int32 Index, bool bRetry);
	void CompleteStage();
	void CompleteMission();
	void RetryNow();
	void RunActions(const TArray<FMissionAction>& Actions);
	bool Evaluate(const FMissionCondition& C) const;
	void CollectInteracts(const FMissionCondition& C, TArray<const FMissionCondition*>& Out) const;
	/** (Re)collect actors for the current stage's actor_tag interact conditions. Runs at BeginStage AND when a
	 *  spawn_npc lands mid-stage (an on_start spawn must join the stage's tag set). */
	void RefreshStageActors();
	bool InteractInRange(const FMissionCondition& C, FVector* OutAt = nullptr) const;
	bool ConditionTarget(const FMissionCondition& C, FVector& Out) const;
	bool StageTarget(FVector& Out) const;
	APawn* GetPlayerPawn() const;
	void TeleportPlayer(const FVector& Loc, float Yaw, bool bSetYaw);
	void SpawnTriggers();
	void DestroyOwnedTriggers();
	void EnsureHud();
	void RefreshHudPerFrame(float Dt);
	void TickFade(float Dt);           // drives the cinematic fade (fade action); independent of the stage machine
	void SetCameraFade(float Alpha);   // 0 = clear, 1 = full black, via the player camera manager
	void ApplyCamera(const FMissionAction& A);   // cut/blend to (or release) the scripted cutscene camera
	void ReleaseCutsceneCamera(float Blend);     // hand the view back to the player; destroy the owned camera actor
	void ApplyShake(const FMissionAction& A);    // kick the player's camera (cutscene impact beats)

	// chapter-1 primitives runtime (spawn_npc / npc_move_to / npc_lead / pair_anim / speech_sequence / minigame_launch /
	// heat_set / hide). Mission-scoped: all of it is torn down on Stop/Complete; spawn_npc reuses an id on stage retry.
	ABullyScriptedNPC* SpawnMissionNpc(const FMissionAction& A);
	ABullyScriptedNPC* NpcById(FName Id) const;
	void OnNpcCommandDone(ABullyScriptedNPC* Npc, const FSNPCCommand& Cmd, const FString& Result);
	void DespawnMissionNpcs(const TCHAR* Why);
	void TickLeads(float Dt);
	void TickSpeech(float Dt);
	void SkipSpeechSequence();             // Enter / pad right button, and the smoke driver: jump to the end of the beat
	void TickMinigame(float Dt);
	void TickHeat(float Dt);

	TMap<FName, TWeakObjectPtr<ABullyScriptedNPC>> Npcs;
	TMap<TWeakObjectPtr<ABullyScriptedNPC>, FName> PendingArrive;   // npc -> event fired when its move finishes
	// sm-humandrive: where each mission npc stood when the current stage began (a stage retry puts them back there,
	// otherwise a chase retry starts with the runaway already at his destination)
	TMap<FName, FTransform> StageStartNpcXf;
	// sm-humandrive: the player's ammo when the current stage began (a stage retry gives it back, like the original's
	// retry, which restarts with the ammo the mission handed out; otherwise a 1_04 tree retry starts with an empty slingshot)
	TMap<FName, int32> StageStartAmmo;
	bool bHasStageStartAmmo = false;
	// sm-humandrive: a knocked-out player fails the stage (retry revives him) when the stage has no KO rule of its own
	double PlayerOutSince = -1.0;
	bool bForceFailReason = false;
	struct FLeadOrder
	{
		TWeakObjectPtr<ABullyScriptedNPC> Npc;
		TArray<FVector> Wps;             // via/path waypoints before the goal
		int32 WpIndex = 0;
		FVector Goal = FVector::ZeroVector;
		float Radius = 600.f;          // pause while the player trails farther than this (goal-distance compared)
		FName Event;                   // fired on arrival
		bool bRun = true;              // gait (see Gait)
		FString Gait;                  // "walk" | "run" | "sprint" (empty = bRun)
		bool bWaitingForPlayer = false;
		float LastDist = TNumericLimits<float>::Max();   // progress watch: no ground gained for 3 s -> snap to the goal
		double NoProgressAt = 0.0;                       // (the walker's own snap won't fire while the player stands on it)
	};
	TArray<FLeadOrder> Leads;
	TSet<FName> MissionCriticalNpcs;   // spawn_npc mission_critical: if one expires unexpectedly, "npc_ko:<id>" fires
	struct FSpeechSeqState
	{
		bool bActive = false;
		FName DoneEvent;
		int32 Index = 0;
		double LineEndsAt = 0.0;
		TArray<FSpeechLine> Lines;
	} Speech;
	struct FMinigameState
	{
		bool bArmed = false, bRunning = false;
		FVector At = FVector::ZeroVector;
		float Radius = 250.f, Seconds = 4.f, Progress = 0.f;
		FName Success, Fail;
		FString Label;
	} Minigame;
	bool bSmokeDriveMinigame = false;   // set by TickSmoke while a minigame is armed (the driver's hold-E)
	float HeatPoints = 0.f, HeatDecay = 25.f;   // punishment-lite stub: decays only while the player is hidden

	// wave-B runtime: waves (spawn_wave) and stealth spotters, both stage-scoped
	struct FWaveGroup
	{
		FName Group;
		TArray<TWeakObjectPtr<ABullyCombatPawn>> Members;
		int32 AdvanceAt = 0;             // downed members needed for the event (0 = all)
		bool bFired = false;
	};
	TArray<FWaveGroup> Waves;
	struct FSpotter
	{
		TWeakObjectPtr<ABullyScriptedNPC> Npc;
		float Range = 1500.f, CosHalf = 0.5f, Seconds = 2.f;
		double SeenSince = -1.0;
		bool bFired = false;
		FName Event;
	};
	TArray<FSpotter> Spotters;
	// leash / zone_timer condition state (one of each per stage; reset in BeginStage)
	mutable double LeashFarSince = -1.0;
	mutable bool ZoneWasInside = false;
	mutable double ZoneOutSince = -1.0;
	void SpawnWave(const FMissionAction& A);
	void TickWaves();
	void TickSpotters();
	void DespawnWaves(FName Group = NAME_None);

	// wave-C runtime: escort follows (mission-scoped), the mission countdown, the fight lesson (stage-scoped)
	struct FFollowOrder
	{
		TWeakObjectPtr<ABullyScriptedNPC> Npc;
		float Radius = 300.f;
		FString Gait;
	};
	TArray<FFollowOrder> Follows;
	void TickFollows(float Dt);
	struct FMissionCountdown
	{
		bool bActive = false;
		double EndsAt = 0.0;
		FName Event;
		bool bVisible = false;
	} Countdown;
	void TickCountdown();
	struct FLessonState
	{
		bool bActive = false;
		FName Id, Move;
		int32 Needed = 3, Count = 0;
		float Window = 10.f;
		double WindowEndsAt = 0.0;
		FString Prompt;
	} Lesson;
	void TickLesson();
	// minimal data-driven race (2_04; the course is any trigger-file path, so the 3_R08 race series can reuse it):
	// the player passes the course's waypoints in order (checkpoint radius), `laps` times -> "race:<id>_win".
	// Rivals are plain data (spawn_npc + npc_move_to on the same course); their npc_arrived:<id> events are the
	// mission's lose condition (first rival home = race lost). No rubber-band/position HUD in v1.
	struct FRaceState
	{
		bool bActive = false;
		FName Id;
		TArray<FVector> Wps;
		int32 Laps = 1, Lap = 1, NextIdx = 0;
		float Radius = 900.f;
	} Race;
	void TickRace();
	// clock_fail_at (2_G2): mission-long edge — fires "clock:fail" when the world clock reaches the target time
	// (circular minutes-of-day; a retry past the target re-arms into the far future, so it fires at most once).
	struct FClockFailState
	{
		bool bActive = false;
		float TargetHour = 0.f, ArmedHour = 0.f;
	} ClockFail;
	void TickClockFail();

	// sm-cutscenes: the cutscene action (original baked cutscenes via UBullyCutsceneSubsystem)
	void RunCutsceneAction(const FMissionAction& A);
	void StopCutscene();
	/** sm-cutscenes: original cutscenes already shown during this mission run; a retry plays the fallback beats instead
	 *  (the original skips its intro cutscene after the first attempt). Reset by StartMission, kept across retries. */
	TSet<FString> CutscenesSeen;

	// wave-D runtime: mission defenders (combat-pawn escortees on the player's team) and the dialogue choice
	struct FDefenderState
	{
		TWeakObjectPtr<ABullyCombatPawn> Pawn;
		FVector SpawnAt = FVector::ZeroVector;
		float DrainHp = 0.f, DrainEvery = 5.f;
		double LastDrain = 0.0;
		bool bDrain = false;
	};
	TMap<FName, FDefenderState> Defenders;
	void TickDefenders(float Dt);
	ABullyCombatPawn* DefenderById(FName Id) const;
	void DespawnDefenders();
	struct FChoiceState
	{
		bool bActive = false;
		FName Id;
		TArray<FChoiceOption> Options;
	} Choice;
	void TickChoice();
	bool ChooseDialogueOption(int32 Index);

	// wave-F: boss health bar (drives the HUD progress bar off a named one-member wave) + the under-half bark
	// (boss_bar action: Name = group, Text = label, Sub = half bark). Stage-scoped.
	struct FBossBarState
	{
		TWeakObjectPtr<ABullyCombatPawn> Boss;
		float Max = 0.f;
		FString Label;
		FString HalfLine;
		bool bHalfFired = false;
	};
	FBossBarState BossBarState;
	void TickBossBar();
	bool bPlayerHidden = false;

	// rendered cutscene capture (-BRMissionShots=<id> in a -game -RenderOffScreen run): grab one HighResShot a beat
	// after each stage begins -- real mission cameras, spawned characters, the authored framings -- so the cutscene
	// composition gets a pixel pass (headless logs confirm the cuts fire; only a render shows what they frame).
	void TickMissionShots(float Dt);
	void FirePendingShot();   // takes the scheduled stage shot now (also used to flush it on stage end before teardown)
	struct FMissionShots
	{
		bool bEnabled = false;
		FString MissionId;
		FString Dir;
		int32 ResX = 1920, ResY = 1080;
		double Warmup = 2.0;           // extra settle after compile-idle before the mission starts
		double ShotDelay = 1.9;        // after a stage begins, wait this long (camera blend + Lumen settle) then shoot
		double CompileReadyAt = -1.0;  // world time shaders/assets went (and stayed) idle; <0 = still compiling
		double IdleSince = -1.0;
		double LastCompileLog = -100.0;
		bool bStarted = false;
		int32 TotalStages = 0;         // stages in the captured mission (it may chain into another -- we stop after these)
		int32 PendingStage = -1;       // stage index of the pending shot, or -1
		double PendingAt = -1.0;       // world time to fire the pending shot
		FString PendingName;           // file stem for the pending shot
		int32 Count = 0;               // shots taken
		double QuitAt = -1.0;          // world time to quit after the last shot; <0 = not scheduled
	} Shots;

	// smoke harness (-BRMissionSmoke[=id]): drives the player through the mission headless and logs PASS/FAIL
	void TickSmoke(float Dt);
	/** Wrong-way smoke helpers. SkippablePositional: a teleport can satisfy (or step past) this condition -- it holds a
	 *  reach_location / trigger_enter. PrimaryGoal: the first positional goal in declaration order (the stage's "intended"
	 *  target; an `any` lists its original trigger first and its fallbacks after). */
	bool SkippablePositional(const FMissionCondition& C) const;
	bool PrimaryGoal(const FMissionCondition& C, FVector& Out) const;
	struct FSmoke
	{
		bool bEnabled = false;
		FString MissionId;
		double Elapsed = 0.0;
		double LastDrive = -10.0;
		double InsideSince = -1.0;
		bool bStarted = false;
		bool bFailTested = false;
		double LastBeatLog = -10.0;   // heartbeat: one line per 10 game-s while driving (wedge forensics)
		bool bDone = false;
		bool bWantFailTest = true;
		bool bNoQuit = false;          // DriveSmoke(.., false): log PASS/FAIL but keep running (campaign smoke)
		int32 LastStage = -1;
		TArray<int32> StagesSeen;
		// wrong-way mode (-BRMissionSmokeWrongWay): per positional stage, first overshoot its point, then teleport straight to
		// the next stage's goal (what a door-link teleport does). The skipped stage must complete on its own (a data fallback);
		// one that does not is recorded as stuck and the normal driver then recovers so the rest of the mission is still checked.
		bool bWrongWay = false;
		int32 WrongWayPhase = 0;       // 0 = not started on this stage, 1 = overshot (waiting), 2 = skipped ahead (waiting), 3 = finished
		double WrongWayAt = 0.0;       // Smoke.Elapsed of the last wrong-way teleport
		FName WrongWayStageId;         // stage under test (for the log once it completes)
		int32 WrongWayTested = 0;      // stages the skip-ahead teleport was applied to
		TArray<FName> WrongWayStuck;   // stages still open after the skip-ahead teleport
	} Smoke;

	// ---- sm-humandrive: HUMAN-MODE smoke driver (-BRMissionSmokeHuman; tools/ue/run_mission_smoke.sh --human) -------------
	// Plays the mission using only what a player can do: movement INPUT (Enhanced Input IA_Move / IA_Sprint / IA_Jump injected
	// through the real bindings) along a runtime navmesh path toward what the HUD shows (blip, prompt target, the npc being
	// chased / followed, the fight), real attack / grapple / block / lock-on / weapon input for fights, real E / Enter / J-K-L
	// key presses (E only while the prompt is on screen), and it waits speech and cutscene beats out (or presses Enter with
	// -BRMissionSmokeHumanSkipSpeech). No teleports (the mission's own scripted teleports/fades still run, and a mission with no
	// "start" begins at its campaign star). No progress toward the goal for 30 s (or 60 s with nothing actionable) logs
	// "BR_MISSION_SMOKE human STUCK" and the run FAILS. Logs game-time minutes per mission. Code: MissionHumanDrive.cpp.
	void TickSmokeHuman(float Dt);
	struct FHumanDrive
	{
		bool bEnabled = false;
		bool bSkipSpeech = false;          // press Enter on speech beats instead of waiting them out
		bool bChain = false;               // keep driving start_mission chains (arrival -> day_one -> ...)
		double Limit = 1800.0;             // game seconds for the whole run
		double StuckSeconds = 30.0;
		// navmesh bounds (spawned per mission) + build wait
		TArray<TWeakObjectPtr<AActor>> NavVolumes;
		TArray<FBox> NavBoxes;
		bool bNavWaiting = false;
		double NavWaitStart = 0.0;
		double NavWaitTotal = 0.0;         // excluded from the human minutes
		bool bPreStartDone = false;
		// path following
		TArray<FVector> Path;
		int32 PathIdx = 0;
		FVector PathGoal = FVector::ZeroVector;
		double PathAt = -100.0;
		bool bPathOk = false;
		bool bPathPartial = false;
		FString GoalWhy;
		// progress / stuck
		FVector ProgGoal = FVector::ZeroVector;
		float BestDist = TNumericLimits<float>::Max();
		double ProgressAt = 0.0;
		double BlockedSince = -1.0;
		double LastJump = -10.0;
		double RecoverUntil = -1.0;
		FVector RecoverDir = FVector::ZeroVector;
		int32 Recoveries = 0;
		double IdleSince = -1.0;
		FString IdleWhy;
		// stage / mission bookkeeping
		int32 LastStage = -1;
		FString LastMissionId;
		double StageStartAt = 0.0;
		double MissionStartAt = 0.0;
		double MissionNavWait = 0.0;
		int32 MissionFails = 0;
		int32 StageFails = 0;
		bool bFailNoted = false;
		TArray<FString> Summary;
		double LastBeat = -10.0;
		// input
		TArray<FKey> Release;
		double LastKeyAt = -10.0;
		// fight
		double LastAtk = -10.0;
		int32 AtkPhase = 0;
		int32 EnemySwings = 0;
		FName LastEnemyMove;
		double LastWeaponCycle = -10.0;
		double LastShot = -10.0;
		// chase: where each mission npc has run (a player runs where he saw the npc run -- ledges and drops included)
		TMap<FName, TArray<FVector>> Trails;
		double FightSig = -1.0;            // fight-stall watch: health/state signature and when it last changed
		double FightSigAt = 0.0;
		double LastFightAt = -10.0;
		FString FightInfo;                 // last fight tick: me vs target (beat log)
		bool bForceSprint = false;         // set around a HumanSteer call: sprint even for a short hop (a chase's breadcrumbs)
	} Human;
	/** Human driver helpers (MissionHumanDrive.cpp). */
	void HumanPrepareNav(const FMissionDef& D, const FVector& PlayerAt);
	bool HumanNavReady();
	bool HumanFindPath(const FVector& From, const FVector& To, TArray<FVector>& Out, bool& bPartial) const;
	void HumanPressKey(const FKey& Key);
	void HumanInject(FName Action, const FInputActionValue& V) const;
	/** Steer toward Goal (nav path, replanned as needed). Returns the remaining straight distance. */
	float HumanSteer(const FVector& Goal, float ArriveCm, bool bSprint, bool bMovingGoal);
	bool HumanFight(const FMissionStage& St);
	FString HumanStuckReport(const TCHAR* Why) const;

	TSharedPtr<FMissionDef> Mission;
	EState State = EState::Idle;
	int32 StageIndex = 0;
	int32 Attempt = 1;
	double StageStartTime = 0.0;
	double FailTime = 0.0;
	FVector StageStartPawnLoc = FVector::ZeroVector;
	FVector MissionStartPawnLoc = FVector::ZeroVector;
	float MissionStartYaw = 0.f;
	TSet<FName> StageEvents;
	TSet<FName> MissionEvents;
	TSet<FName> Flags;
	TMap<FName, TWeakObjectPtr<AMissionTrigger>> Triggers;
	TArray<TWeakObjectPtr<AMissionTrigger>> OwnedTriggers;
	TArray<TWeakObjectPtr<AActor>> TagActors;       // actors found for the current stage's actor_tag interact conditions
	TSharedPtr<FBullyHUD> Hud;
	FString PendingAutoStart;
	double AutoStartAt = 0.0;
	bool bPendingDevOverride = false;   // PendingAutoStart came from -BRMission= (start_mission chains stay gated)

	// cinematic fade state (fade action)
	enum class EFadePhase : uint8 { None, Out, Hold, In } FadePhase = EFadePhase::None;
	float FadeOutDur = 0.5f, FadeHoldDur = 0.4f, FadeInDur = 0.8f;
	double FadePhaseStart = 0.0;
	bool bFadePendingTeleport = false;
	FVector FadeTeleportLoc = FVector::ZeroVector;
	float FadeTeleportYaw = 0.f;

	// scripted cutscene camera (camera action): an owned ACameraActor we blend the view to, and release/destroy on
	// mission end so gameplay never gets left looking through it.
	TWeakObjectPtr<ACameraActor> CutsceneCam;
};
