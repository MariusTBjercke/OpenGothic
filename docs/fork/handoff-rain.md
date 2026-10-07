# Handoff: rain (weather) in the OpenGothic fork

Status as of 2026-10-07. Written for the next agent picking this up. Read `AGENTS.md` first, then
`docs/fork/research/rain.md` (how the original engine does it, with Ghidra addresses).

## Where things are

- Branch: **`feat/rain`** (pushed to `origin`), **not merged into `master` yet**. The user is play-testing it;
  merge when they say so (fast-forward or merge commit, then push `master`).
- The user's play folder `D:\GothicDev\OpenGothic-play` runs `feat/rain@3c923b30` (see `BUILD.txt` there).
  It holds the user's real saves; never use it as a working directory for automated runs.

Commits on the branch, oldest first (all self-contained, see `git log master..feat/rain`):

| Commit | What |
| ------ | ---- |
| `95afc350` | research notes on vanilla rain (`docs/fork/research/rain.md`) |
| `ccb23809` | `-marvin "<cmd>;<cmd>"` runs console commands once the first world is loaded |
| `c9b749ef` | `Weather` class: daily rain window, weight, `Wld_IsRaining`, `zstartrain`, sound, particles, save entry |
| `e0ed81ea` | rain dims sun/ambient in `shader/lighting/sky_exposure.comp` (after exposure) |
| `3b07cb80`, `1511b227`, `a837923e` | smoke test: `-Marvin`, `-ScreenshotAt`, PrintWindow capture, music off by default |
| `13c4098a`, `7a93c49f` | `scripts/deploy-play.ps1` and its launchers (devmode launcher must not use `-window`) |
| `a2bb5c0a` | drops drawn additive (`ADD`) so they stay visible in the dim rain light |
| `83f6d4d5`, `3c923b30` | start theme (`GAMESTART.WAV`) skipped in benchmark and stopped when a game starts loading |
| docs commits | `AGENTS.md`: fork save rule, play folder, deliberate differences from upstream |

## What works (verified)

- Logic matches vanilla: one random window per game day rolled at the noon wrap (not when sleeping over noon),
  weight ramp 20/60/20 %, `Wld_IsRaining` = weight > 0.3, `zstartrain [pos]` semantics, `skyEffects=0` disables.
- Sound: `RAIN_01.WAV` loop owned by `Weather` (own `Tempest::SoundEffect`, follows the listener), volume slope as
  vanilla, x0.25 inside portal rooms.
- Drops: world-space box emitter around a point 12.5 m in front of the camera, ~1000 drops at full weight,
  `SKYRAIN.TGA`, velocity aligned, additive. User confirmed they are visible after switching to `ADD`.
- Lighting: darker overcast look. Sky stays blue (only dimmed).
- Smoke test passes, FPS unchanged (~76 on RTX 5070 Ti). User heard the rain and saw the drops in-game.
- Start theme fix confirmed by the user; vanilla behaves the same (`zCMenu::Leave` releases the sound).

## Not verified yet

1. **Save/load of the weather state** (entry `worlds/<zen>/weather`). No console command saves the game, so it
   needs a manual check: save while it rains, reload, rain continues. The entry can be read back with
   `System.IO.Compression` (layout: `float prevSkyTime, rainStart, rainStop; int32 rainCtr; u8 lightning; u8 rainActive`).
2. **NPC weather lines** via `Wld_IsRaining` (`B_Say_GuildGreetings`) in real play.
3. **GCC/Clang build** (`-Wall -Wconversion -Werror`). Only MSVC was built locally. CI runs on push to `master`
   or on a PR into `master` in the fork; watch for implicit conversions in `weather.cpp`, `marvin.cpp`,
   `commandline.cpp`, `mainwindow.cpp`, `gothic.cpp`.

## Gotchas learned the hard way

- **Noon is the sky-day wrap.** Testing with `set time 12 0;zstartrain 0.5` clips the forced window to zero
  weight. Use `set time 13 0`.
- **Benchmark mode has no player**, so `WorldSound::tick` (and zone music, listener range checks) never run.
  Anything that relies on `WorldSound::isInListenerRange` fails there; that is why `Weather` owns its sound.
- **Blended particles are lit** (forward shading) and go black under the rain lighting; additive ones are unlit.
- **Dimming the sun in `Sky` brightens the image**: auto exposure is computed from the sun color. Apply weather
  dimming after exposure, in `sky_exposure.comp`, like the existing `clouds` factor.
- **`PfxBucket` keeps a reference to its `ParticleFx`** for the lifetime of the world view, so the rain
  declaration is a function-local static (`Weather::rainParticles`). The bucket sizes itself from `ppsValue` on
  creation; `Weather` sets it to the max before creating the emitter and scales it per tick afterwards.
- **`World::roomAt` gives false positives** (e.g. `TURMOST02`, `MATTEO`) when the camera is above roofs next to a
  portal room, which turns drops off and lowers the sound. It also scans all BSP sectors per call.
- **Screenshots**: `-ScreenshotAt` uses `PrintWindow(PW_RENDERFULLCONTENT)`; `CopyFromScreen` captured whatever
  window was on top. The benchmark camera is deterministic, so same-second shots compare well.
- **`GAMESTART.WAV`** is a 43 s music track played as a sound effect; `musicEnabled=0` does not silence it.
- **Ghidra**: the MCP tools may not register in the client; the plugin's HTTP API at `http://127.0.0.1:8089`
  works directly (`/search_strings?search_term=`, `/search_functions?name_pattern=`, `/decompile_function?address=`,
  `/get_xrefs_to?address=`, `POST /disassemble_bytes {"start_address","length"}`, `/read_memory?address=&length=`).
  `Gothic2.exe` has named ZenGin functions. Document behavior in our own words, never commit decompiled code.
- Files in the working tree are CRLF (`core.autocrlf=true`); multi-line `sed`/`perl` patches with `\n` silently
  miss. Use the Edit tool.

## Suggested next steps (in order)

1. **Close the open checks** above (save/load, NPC lines, CI build), then merge `feat/rain` into `master`.
2. **`weather` console command** printing today's window (as clock times), current weight and sheltered state.
   Cheap, and it answers "why haven't I seen rain" (the user slept over noon for days and kept the default window).
3. **Drops stop at roofs and ground** (biggest visual gap, vanilla ray-tests every drop). Options:
   - implement `flyCollDet` in `PfxBucket` (parsed in `ParticleFx`, unused today) with a physics ray per new
     particle (`DynamicWorld::ray`) that shortens its lifetime to the hit; ~700 rays/s at full rain is cheap; or
   - a top-down height map around the camera (GPU), more work and touches the renderer.
   The first also enables splashes and fixes rain inside houses that are not portal rooms.
4. **Splashes** with `SKYRAINSPLASH.TGA` at the hit points (vanilla does this). `mrk*` fields are parsed but not
   implemented either; a small second emitter fed with hit positions is simpler.
5. **Grey overcast sky**: research `RenderRainCloudLayer` (0x005e5d00) for how much vanilla darkens and how
   `SKYRAINCLOUDS.TGA` is blended, then add a rain term to the sky/cloud shaders (`shader/sky/*`) and some fog.
   Renderer files change often upstream; keep the hook small (one push constant / scene field).
6. **Better shelter detection** than `roomAt`: a short upward physics ray from the camera (roof check), cached for
   a few frames; keep `roomAt` for portal rooms.
7. **Wind tilt** from `[SKY_OUTDOOR] zRainWindScale` (0.003) and the sky's global wind; vanilla tilts the fall
   direction only.
8. **Unit tests for pure logic** (`Weather::skyTime`, `rainWeightAt`, window rolling): there is no test target,
   but ZenKit vendors doctest (`lib/ZenKit/vendor/doctest`), usable for a small fork-only test executable.
9. Optional `-nomusic` flag for quiet manual test sessions (music and start theme off for one session without
   touching `Gothic.ini`). Offered to the user, not requested yet; ask before building it.

## Working with the user

- Conversation in Norwegian; repository docs and commits in English, Conventional Commits.
- Isolated fork: no issues/PRs/comments upstream (see `AGENTS.md`).
- The user play-tests via `scripts/deploy-play.ps1` and gives quick feedback (they watch smoke-test windows too).
  Prefer visible/readable effects over strict realism (asked for brighter drops).
- Before claiming a visual or audio change works, capture screenshots with the smoke test or ask the user to
  check; several "fixes" in this session only became correct after the user listened or looked.
