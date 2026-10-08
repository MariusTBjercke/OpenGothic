# Handoff: rain (weather) in the OpenGothic fork

Status as of 2026-10-08 (third session). Written for the next agent picking this up. Read `AGENTS.md` first, then
`docs/fork/research/rain.md` (how the original engine does it, with Ghidra addresses).

## Where things are

- Branch: **`feat/rain`**, **not merged into `master` yet**. The user is play-testing it; merge when they say so
  (fast-forward or merge commit, then push `master`).
- The user's play folder `D:\GothicDev\OpenGothic-play` runs `feat/rain@cb281256` (see `BUILD.txt` there).
  It holds the user's real saves; never use it as a working directory for automated runs. `save game` typed in the
  console there overwrites `save_slot_1.sav`, the first menu slot.

Commits on the branch, oldest first (all self-contained, see `git log master..feat/rain`):

| Commit | What |
| ------ | ---- |
| `95afc350` | research notes on vanilla rain (`docs/fork/research/rain.md`) |
| `ccb23809` | `-marvin "<cmd>;<cmd>"` runs console commands at startup |
| `c9b749ef` | `Weather` class: daily rain window, weight, `Wld_IsRaining`, `zstartrain`, sound, particles, save entry |
| `e0ed81ea` | rain dims sun/ambient in `shader/lighting/sky_exposure.comp` (after exposure) |
| `3b07cb80`, `1511b227`, `a837923e` | smoke test: `-Marvin`, `-ScreenshotAt`, PrintWindow capture, music off by default |
| `13c4098a`, `7a93c49f` | `scripts/deploy-play.ps1` and its launchers (devmode launcher must not use `-window`) |
| `a2bb5c0a` | drops drawn additive (`ADD`) so they stay visible in the dim rain light |
| `83f6d4d5`, `3c923b30` | start theme (`GAMESTART.WAV`) skipped in benchmark and stopped when a game starts loading |
| `5430ff4c` | `save game` / `load game` console commands (slot 1, as in the original) |
| `7e8ce3eb` | `-marvin` commands run after the first drawn world frame; their output goes to `log.txt` |
| `06575ea0` | `weather` console command |
| `b9ff997d` | smoke test `-LoadSave <file>` |
| `2ccd4e65` | drops stop at roofs and the ground (one physics ray per new drop) |
| `d63dbd0c`, `bcba6f14` | `-novideo` flag (skips script videos such as the intro); the smoke test passes it |
| `ff6b424f` | drops stay on in portal rooms (rain visible from caves), shelter ray starts above the world mesh |
| `0f5036f5` | overcast sky, haze and hidden sun/moon (`SceneDesc.rain`, sky/fog shaders) |
| `8e07daec` | splashes where rain lands (second emitter, `ParticleFx::spawnHook` replaces `clipLife`) |
| `54163a1f` | drops tilted by a varying wind (`zRainWindScale`) |
| `1947b281` | indoor check for the muffled sound: roof ray plus portal room or walls on three sides |
| `97ad8db0` | overcast darker (0.3 of horizon radiance) and without the cross at the zenith (user reports) |
| `e2be86dd` | splashes from a queue of drop landings (same place and time), 6 to 12 cm, softer; user liked it |
| `e8580156` | wet surfaces: rain height map, darker albedo after the G-buffer, sky sheen after the lights |
| `b24f5c52` | sheen faint (0.15), only under the overcast, not on foliage; darker wet albedo (user: looked like ice) |
| `4147b855` | soft wet edges (9 taps, 1.2 m), map 128 m traced once with new cells first (user: hard edges, pop-in) |
| `28bfda58` | `wait <ms>` in `-marvin` startup commands |
| `cb281256` | wetness on the game clock (5 min wet, 45 min dry, stepped through sleep), saved in `worlds/<zen>/wetness` |
| docs commits | `AGENTS.md`, research notes, this file |

## What works (verified)

- Logic matches vanilla: one random window per game day rolled at the noon wrap (not when sleeping over noon),
  weight ramp 20/60/20 %, `Wld_IsRaining` = weight > 0.3, `zstartrain [pos]` semantics, `skyEffects=0` disables.
- Sound: `RAIN_01.WAV` loop owned by `Weather`, volume slope as vanilla, x0.25 indoors and under water.
  User heard it. Indoor check verified with `weather` after loading saves made at `NW_CITY_HABOUR_HUT_03_IN`
  (`sheltered`) and `NW_CITY_HABOUR_05` (not).
- Overcast sky: benchmark screenshots at 13:00 (grey sky with darker cloud variation, hazy distance, water
  reflects grey), 23:00 (stars gone, near black sky) and at half coverage. FPS unchanged (76).
- Splashes on the harbour pavement and on barrels (`goto waypoint NW_CITY_HABOUR_05`), none inside the huts.
- Wind: drops visibly slanted at the harbour.
- Drops: world-space box emitter 12.5 m in front of the camera, ~1000 drops at full weight, additive. User saw them.
- Drops end at the first static hit and are not spawned under roofs. Screenshot comparison in Xardas' tower (new
  game start, not a portal room): streaks inside before, none after. Counter numbers are in the research notes.
- Drops are no longer switched off in portal rooms (user report: rain outside vanished when looking out of a
  cave). Vanilla only hides rain when no outdoor area is visible (research notes, "Sound volume"). In two cave
  runs (`goto waypoint NW_CITYFOREST_CAVE_01` / `_06`) the rays skipped every drop.
- Lighting: darker overcast look. Sky stays blue (only dimmed).
- **Save/load of the weather state**, automated: run 1 forced rain and ran `save game`; the entry
  `worlds/newworld.zen/weather` held `prevSkyTime 13:00, rainStart 12:00, rainStop 14:12`. Run 2 started from that
  save with `-LoadSave` and `weather` printed `rain 12:00-14:12, weight 1.00, raining`. Recipe in `AGENTS.md`.
- **CI** (fork, `workflow_dispatch`): `feat/rain@e3ef5032` (run 37677931592) green on Linux GCC, Windows MinGW,
  macOS arm64 and x64. `feat/rain@045795ce` (run 37680465171): Windows MinGW (GCC) and both macOS jobs (Clang)
  green with `-Werror`; both Linux jobs hung for 28 min in `apt-get update` (GitHub mirror) and the run was
  cancelled. Code-wise that covers GCC and Clang.
- Smoke test passes; benchmark FPS ~76 on RTX 5070 Ti with rain, same as without drop rays.

## Not verified yet

1. **NPC weather lines** via `Wld_IsRaining` (`B_Say_GuildGreetings`) in real play: start rain with
   `zstartrain 0.5`, then talk to an ambient NPC outdoors.
2. **Drops at roofs from the ground**: the screenshots were taken inside the tower and from the high benchmark
   camera. Standing under a roof overhang or a market stall in the city has not been looked at.
3. **Wet surfaces in real play** (`e8580156`): only screenshots at the harbour, the farm path and in a hut.
4. **The second batch in real play** (`0f5036f5` to `1947b281`): overcast sky, splashes, wind tilt and the
   muffled sound in huts were checked with screenshots and the `weather` command only.

Looking out of a cave (`ff6b424f`) was confirmed by the user in play.

## Gotchas learned the hard way

- **Noon is the sky-day wrap.** Testing with `set time 12 0;zstartrain 0.5` clips the forced window to zero
  weight. Use `set time 13 0`. Even then the window starts at 12:00 (clamped), so `weather` shows `12:00-14:12`.
- **Benchmark mode has no player**, so `WorldSound::tick` (and zone music, listener range checks) never run.
  Anything that relies on `WorldSound::isInListenerRange` fails there; that is why `Weather` owns its sound.
  F5 quick save needs a living player too; use `save game` in `idle` mode for save tests.
- **`MainWindow::saveGame` draws a screenshot** with `Renderer::screenshoot`. Before the renderer has drawn the
  world once, that throws in `DrawCommands::visibilityPass` (`setBinding`). It crashed `save game` when `-marvin`
  ran inside `onWorldLoaded`, even at its end; the commands now run at the end of `MainWindow::render`.
- **Blended particles are lit** (forward shading) and go black under the rain lighting; additive ones are unlit.
- **Dimming the sun in `Sky` brightens the image**: auto exposure is computed from the sun color. Apply weather
  dimming after exposure, in `sky_exposure.comp`, like the existing `clouds` factor.
- **`PfxBucket` keeps a reference to its `ParticleFx`** for the lifetime of the world view, so the rain
  declaration is a function-local static (`Weather::rainParticles`). The bucket sizes itself from `ppsValue` on
  creation; `Weather` sets it to the max before creating the emitter and scales it per tick afterwards. For the same
  reason the drop ray hook (`clipDrop`) is a plain function that looks up `Gothic::inst().world()`, not a lambda
  capturing a `Weather`.
- **`PfxBucket::tickEmit` skips particles whose `init` leaves `life == 0`**, so `clipLife` can drop a particle by
  returning 0. A particle with `life == 0` that was counted would never be freed.
- **`World::roomAt` gives false positives** (e.g. `TURMOST02`, `MATTEO`) when the camera is above roofs next to a
  portal room, which lowers the sound. It also scans all BSP sectors per call. It no longer switches drops off.
- **The overcast lights the scene**: the sky irradiance (ambient) is computed from the sky view LUT including the
  overcast layer, while auto exposure is still set by the dry sun. A bright overcast made rain scenes brighter than
  dry ones in shade (user report). Tune `RainOvercastBrightness` in `shader/sky/clouds.glsl` against measured
  screenshots: same waypoint dry and rain (`-Marvin "cheat god;set time 13 0;[zstartrain 0.5;]goto waypoint X"`,
  idle, shot at 14 s) and compare mean brightness. `cheat god` keeps the hero alive near monsters; dense forest
  waypoints can put the camera inside foliage. `NW_CITY_TO_FARM2_05` and `NW_CITY_HABOUR_05` give usable views.
- **Wet sheen turns into ice or snow** when it is strong, reflects the clear sky after rain, or lands on foliage
  (pine branches face up). Keep it faint, scale it with the overcast (`rainCover`) and skip alpha tested pixels
  (G-buffer hint bit 2). Test "wet, then rain stopped": save while it rains, load with `-LoadSave` (wetness starts
  at the rain weight) and run `zstartrain 1`.
- **Anything per pixel in the sky must not depend on the view azimuth near the zenith**: samples that follow the
  view direction (as `applyClouds` does for the dry cloud highlight) form a cross when looking straight up.
- **`-marvin` commands run in one frame**, before the next world tick, so state that changes on tick (weight,
  wetness) is stale in a `weather` right after `set time` or `zstartrain`. Put `wait 2000;` in between.
- **Script videos block `-marvin`**: startup commands wait for a drawn world frame, and a new game plays the intro
  first. The smoke test passes `-novideo`; without it an idle run on a new game never ran its commands.
- **Screenshots**: `-ScreenshotAt` uses `PrintWindow(PW_RENDERFULLCONTENT)`. The benchmark camera path is close to
  deterministic, but two runs at the same second can differ by a few frames and in exposure; thin drops are hard to
  compare from the high benchmark camera. A temporary counter in the code and the tower start view were more useful.
- **`GAMESTART.WAV`** is a 43 s music track played as a sound effect; `musicEnabled=0` does not silence it.
- **Ghidra**: the MCP tools may not register in the client; the plugin's HTTP API at `http://127.0.0.1:8089`
  works directly (`/search_strings?search_term=`, `/search_functions?name_pattern=`, `/decompile_function?address=`,
  `/get_xrefs_to?address=`, `POST /disassemble_bytes {"start_address","length"}`, `/read_memory?address=&length=`).
  `Gothic2.exe` has named ZenGin functions. Document behavior in our own words, never commit decompiled code.
- Most source files in the working tree are CRLF (`core.autocrlf=true`); `common/world/weather.*`, `AGENTS.md` and
  `docs/fork/*` are LF. `git ls-files --eol <file>` tells which. Multi-line `sed`/`perl` patches with `\n` silently
  miss on CRLF files. Use the Edit tool.
- Partial commits: `git add -p` is interactive and unavailable. Write the diff to a file, keep the wanted hunks,
  and `git apply --cached` it.

## Suggested next steps (in order)

1. **Close the open checks** above, then merge `feat/rain` into `master` when the user says so, and redeploy the
   play folder (`scripts/deploy-play.ps1`). The user has not asked for a merge yet.
2. **Tuning from user feedback**: wet darkening (0.3) and sheen (0.12) in `shader/lighting/rain_wet.frag`, wetting
   and drying times in `Weather::tickWetness`, overcast brightness (`rainOvercast` in `shader/sky/clouds.glsl`), haze amount
   (0.6 in `rainClouds`, `shader/sky/sky_common.glsl`), splash rate/size/alpha, drop count.
3. **Wet surfaces beyond 64 m** count as exposed (outside the rain map), so far indoor floors seen through a door
   could look wet. Not seen yet; a bigger map or a second coarse ring would fix it.
4. **Lightning**: the flag is rolled and saved like the original, but G2 does not seem to render it (ZenKit marks
   the save fields G1 only). Research before building anything.
5. **Unit tests for pure logic** (`Weather::skyTime`, `rainWeightAt`, window rolling): there is no test target,
   but ZenKit vendors doctest (`lib/ZenKit/vendor/doctest`), usable for a small fork-only test executable.
6. Optional `-nomusic` flag for quiet manual test sessions. Offered to the user, not requested yet; ask first.

## Working with the user

- Conversation in Norwegian; repository docs and commits in English, Conventional Commits. Commits on this branch
  carry no co-author trailer.
- **Don't dispatch CI after every change.** The user is mostly testing for themselves; run CI when a version is
  close to done. A push to `master` runs it anyway (`gh workflow run Build ... --ref feat/rain` exists for the
  rest). Verify locally with the MSVC build and the smoke test meanwhile.
- Isolated fork: no issues/PRs/comments upstream (see `AGENTS.md`).
- The user play-tests via `scripts/deploy-play.ps1` and gives quick feedback (they watch smoke-test windows too).
  Prefer visible/readable effects over strict realism (asked for brighter drops).
- Before claiming a visual or audio change works, capture screenshots with the smoke test or ask the user to
  check; several "fixes" in the first session only became correct after the user listened or looked.
