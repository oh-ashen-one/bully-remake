# Story mode play loop (sm-playloop, 2026-10-24)

Launch the game, pick New Story / Continue, play the campaign from the gates; progress is saved.

## Player entry

- Double-click `Play Bully Story Mode.command` (repo root) or run `tools/play_story.sh`. It builds the editor target if the
  sources are newer than the built module, then launches `UnrealEditor -game -windowed -ResX=1920 -ResY=1080 -BRStory` on
  `/Game/Imported/Maps/Bullworth` through the GPU slot wrapper (class `capture`, max hold raised to 4 h so a play session is
  not killed at the default 40 min; `BR_PLAY_MAX_HOLD=<s>` overrides). **Always windowed.** `--dry-run` prints the build
  decision and the exact command and starts nothing; `--no-build`, `--res WxH`, `--slot NAME`, `-- <extra engine args>`.
- `-BRStory` is ignored (logged) when any mission harness flag is present (`-BRMission=`, `-BRMissionSmoke`,
  `-BRCampaignSmoke`, `-BRMissionShots=`), so every harness behaves as before. Town life / sandbox stay off (any `-BR*` switch
  turns them off; add `-BRTownLife` to try the town crowd in story mode). Keep `BootMission` empty in `DefaultGame.ini`.

## Flow

1. Title screen (Slate overlay, world paused behind it): **Continue** (only when a save exists, shows its summary), **New
   Story**, **Quit**. W/S, arrows, D-pad / left stick, mouse; Enter, Space, A, click to confirm. Esc does nothing here.
2. New Story starts `arrival` -> `day_one`; on the day_one pass the game is in campaign free roam: gold mission stars spawn
   from `Config/Campaign.json` (availability = chapter, prereqs, clock window). Walking into a star starts its mission.
   A star that appears under the player's feet does not fire until he leaves and re-enters.
3. Free roam HUD (no mission running): header `CHAPTER n`, objective `Go to the yellow star: <title>`, the waypoint diamond
   + distance on the nearest available gold star, cash top right. With no star up it says why (`<title> starts between 8:00
   and 19:00 ...` or `No story mission available right now`).
4. Mission pass: `MISSION PASSED` banner with the reward line (money, respect, faction respect deltas, unlocked weapons).
   Fail: `MISSION FAILED` + retry, as before.
5. Esc menu gains **Save & Quit** (story mode only); gamepad Start also opens the menu.

## Save (`BullyStory` slot, `Saved/SaveGames/BullyStory.sav`; `UBullySaveGame`)

Captured: campaign completions + attempts, chapter, faction respect, collectibles, granted weapons, persistent story flags
(mission events named `world_*`), clock (day, time, paused), wallet (money, respect) + item inventory, weapon ammo, and a
respawn point (where the last mission was passed, or the free-roam spot of a manual save) + last mission id.
**Never saved: mission-machine state.** Autosave fires after every mission pass and chapter change, but only once no mission
is active and no `start_mission` chain is pending (so never between arrival and day_one, nor inside 1_02B -> 1_02C).
Save & Quit in free roam writes the current spot; mid-mission it keeps the last autosave and the mission restarts on
Continue. Continue restores everything, teleports to the respawn point (the dorm after day_one), refreshes the stars.
Delete the `.sav` to start over; New Story overwrites it at the first autosave.

## Verification

- `tools/ue/run_story_smoke.sh` (headless, `-nullrhi`, two runs): `-BRStorySmoke=new` (title with no Continue -> New Story ->
  the mission smoke driver plays arrival + day_one -> autosave -> 1_02B star + free-roam HUD guidance -> state mutated and
  written via Save & Quit's code path) then `-BRStorySmoke=continue` (title offers Continue -> every field restored, star and
  guidance back). Uses slot `BullyStorySmoke`.
- `tools/ue/run_story_shots.sh` (offscreen render, needs the smoke save): PNGs with UI of the title, the star + guidance from
  18 m and 5 m. Found that the engine's emissive/unlit debug materials do not render for the star beacon (EmissiveMeshMaterial
  draws nothing; M_SimpleUnlitTranslucent draws black), so the beacon is the opaque basic-shape material, gold, plus a point light.

## Not done / known gaps

- No in-game pause-menu "Continue from last save" after a death; no multiple slots / slot picker; no save-corruption repair
  beyond refusing newer-version files.
- The guidance blip is the existing waypoint (diamond); no minimap. Interior stars are still skipped (area != 0).
- Every weapon is still unlocked from the start (Weapons.json v1); the save stores ammo and granted weapons only.
- Attended play (window, mouse capture, gamepad menu navigation, the title's mouse hover) is verified by construction and
  by the offscreen render, not by hand: Hari's playtest is still owed.
