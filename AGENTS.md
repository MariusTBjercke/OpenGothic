# AGENTS.md

Orientation notes for AI coding agents (and humans) working in this repository.
Read this before exploring the code. Keep it up to date when you learn something
non-obvious that the next agent would otherwise have to rediscover.

## What this is

OpenGothic is an open-source re-implementation of the Gothic 2: Night of the Raven
engine (Gothic 1 works partially). It loads the original game data and Daedalus
scripts; it ships no assets of its own. C++20, CMake, custom graphics engine
(Tempest) with Vulkan / DX12 / Metal backends.

## Repository and remotes

This checkout is a **fork**.

| Remote     | URL                                            | Use                         |
| ---------- | ---------------------------------------------- | --------------------------- |
| `origin`   | `git@github.com:MariusTBjercke/OpenGothic.git` | our fork, push here         |
| `upstream` | `git@github.com:Try/OpenGothic.git`            | original project, fetch only |

Pushing to `upstream` is disabled on purpose (push URL is a dummy).

Pulling upstream changes into the fork:

```bash
git fetch upstream
git merge upstream/master        # on master, then push to origin
git submodule update --init --recursive
```

**Keep upstream merges cheap.** Prefer adding new files over editing upstream files,
keep edits to upstream files small and localized, and do not reformat or mass-rename
upstream code. Fork-only material (this file, `CLAUDE.md`, `.claude/`, and any future
`docs/fork/`) lives in files upstream does not have, so it never conflicts.

## Commit messages

We use [Conventional Commits](https://www.conventionalcommits.org/) for our own commits:

```
<type>(<optional scope>): <imperative summary, lower case, no trailing period>
```

Types: `feat`, `fix`, `perf`, `refactor`, `docs`, `build`, `ci`, `test`, `chore`, `style`, `revert`.
Suggested scopes match top-level areas: `game`, `world`, `npc`, `script`, `graphics`,
`shader`, `physics`, `sound`, `ui`, `save`, `spacer`, `build`, `ci`.
Mark breaking changes (for example a savegame format change) with `!` and a `BREAKING CHANGE:` footer.

Examples: `fix(npc): stop combat AI from re-equipping broken weapons`,
`feat(spacer): add vob duplication`, `docs: describe save versioning in AGENTS.md`.

Upstream history (Try/OpenGothic) uses free-form messages such as `editor: ...`,
`cleanup`, `Update Tempest`. Leave those untouched when merging; the convention applies
to commits made in this fork. Work on a branch and merge into `origin/master`.

**Fork policy (current):** this is an isolated fork for experimenting and learning. Do not open
issues, PRs or comments on Try/OpenGothic. Contributing upstream may happen later; until then, prefer
self-contained commits (one concern each) so individual fixes can be cherry-picked onto a clean
branch from `upstream/master` if we decide to send them.

### Deliberate differences from upstream

Keep these when merging upstream changes:

- **Start theme stops on load.** `GAMESTART.WAV` (43 s main menu theme, a sound effect, not music) is stopped when a
  game starts loading (`Gothic::stopStartTheme` in `MainWindow::onStartLoading`) and skipped in benchmark mode.
  Upstream keeps it playing into the level and declined changing that in issue #764, assuming vanilla does the same.
  Vanilla does not: `zCMenu::Enter` (0x004db780) plays GAMESTART once when the main menu first opens, and
  `zCMenu::Leave` (0x004db910) releases that sound, which stops it as soon as the menu is left for a new game or a
  load. Also observed in the user's Steam Gothic II.
- **Rain** (`common/world/weather.*`), `-marvin` startup commands, the `-novideo` flag, and the Windows helper
  scripts in `scripts/`.

## Layout

| Path                | Contents |
| ------------------- | -------- |
| `game/main.cpp`     | Entry point of the game executable `Gothic2Notr`. Only file in `game/`. |
| `common/`           | The engine itself, built as static library `OpenGothic`. Shared by game and editor. |
| `spacer/`           | Work-in-progress world editor (`Spacer` executable), upstream's current focus. |
| `shader/`           | GLSL shaders, compiled to SPIR-V at build time and embedded in the binary. |
| `lib/`              | Dependencies. Submodules: `Tempest`, `ZenKit`, `bullet3`, `dmusic`, `TinySoundFont`, `rapidjson`. Vendored: `edd-dbg`, `miniz`. |
| `linux/`, `packaging/` | Debian packaging, Nix flake, AppStream metadata. |
| `scripts/`          | `Gothic2Notr.bat` launcher copied next to the Windows build; `build-windows.ps1`, `smoke-test.ps1` and `deploy-play.ps1` (fork-only) build/test/deploy helpers. |

### Inside `common/`

| Path | What lives there |
| ---- | ---------------- |
| `gothic.{h,cpp}` | `Gothic` singleton (`Gothic::inst()`): game paths, options, ini file, current `GameSession`, global sounds/fx caches. |
| `commandline.{h,cpp}` | `CommandLine::inst()`: parsed CLI flags (`-g`, `-game:`, `-w`, `-rt`, `-ms`, ...). See README for the list. |
| `mainwindow.{h,cpp}` | Window, main loop, input routing, top-level rendering. |
| `marvin.{h,cpp}` | Developer console ("marvin mode"). Command table at the top of `marvin.cpp`; many original commands are stubbed as `C_Invalid`. |
| `game/` | Session and rules: `gamesession` (owns the `World`), `gamescript` (Daedalus VM + script externals), `playercontrol`, `inventory`, `fightalgo`, `movealgo`, `damagecalculator`, `questlog`, `serialize` (save files). |
| `game/definitions/` | Wrappers around script-defined tables (camera, music, sfx, pfx, spells, fight AI, SVM). |
| `game/compatibility/` | Ikarus/LeGo emulation: a fake 32-bit memory space (`mem32`, `directmemory`, `cpu32`) so mods that poke engine memory partially work. |
| `world/` | `World`, waynet (`waymatrix`, `waypoint`, `waypath`), spatial index, AI queue, focus, sound emitters. |
| `world/objects/` | Vob types: `npc` (largest file, ~4.8k lines), `item`, `interactive` (mobsi), `fireplace`, `pfxemitter`, ... |
| `world/triggers/` | zCTrigger family: movers, script triggers, cutscene camera, code master, zones. |
| `graphics/` | `renderer.cpp` (frame graph), `worldview`, `rtscene` (ray tracing scene), `drawclusters`/`drawbuckets`/`drawcommands` (GPU-driven drawing), `shaders.cpp` (pipeline creation), `mesh/` (skeleton, animation, pose), `pfx/`, `sky/`. |
| `physics/` | Bullet wrapper: `dynamicworld` (NPC/item collision, ray tests), `collisionworld`. |
| `sound/`, `gamemusic`, `dmusic/` | Sound effects, DirectMusic playback (on top of `lib/dmusic`). |
| `bink/` | Standalone Bink video decoder (port of ffmpeg's). |
| `ui/` | In-game menus (scripted `gamemenu`, dialog, inventory, document, console). |
| `utils/` | Ini parser, install detection, crash log, worker threads, file helpers. |

## Common tasks: where to look

- **Implement or fix a Daedalus script external** (`Npc_*`, `AI_*`, `Mdl_*`, `Wld_*`, ...):
  `common/game/gamescript.cpp`. Externals are registered with `bindExternal("name", &GameScript::name)`
  near the top of the file (~200 of them); the implementation is a member function of the same
  lower-case name. Partially implemented ones log `not implemented call [...]`.
- **AI action queue (`AI_*` calls)**: queued via `world/aiqueue.*`, dispatched in `Npc::nextAiAction`
  (called from `Npc::implAiTick`) in `world/objects/npc.cpp`.
- **Save/load**: `common/game/serialize.h`. Saves are zip archives with named entries; `Serialize::Version::Current`
  is the format version. Upstream bumps `Current` and gates reads on `fin.version()` (`if(fin.version()<55) ...`).
  **In this fork, do not bump `Current` or change existing entries.** Store fork-only state in a new entry
  and read it optionally (`if(fin.setEntry("worlds/",wname,"/weather")) ...`; `setEntry` returns false when the
  entry is missing). That keeps saves loadable both ways between this fork and upstream OpenGothic, and avoids
  a version clash when upstream bumps the number itself. Vanilla Gothic saves are a different format either way.
- **New shader**: put the source under `shader/`, then register it with `add_shader(...)` in
  `shader/CMakeLists.txt` (nothing is picked up automatically). Variants are made with `-D` defines on
  the same source. Pipelines are created in `common/graphics/shaders.cpp`.
- **New source file**: `common/**` and `spacer/**` are globbed (`GLOB_RECURSE`). Re-run CMake configure
  after adding files.
- **Console command**: add to the table and `CmdType` enum in `common/marvin.*`. Put a variant with arguments
  (`"zstartrain %f"`) before the bare one (`"zstartrain"`), otherwise the bare entry wins with `C_Extra`.
- **Graphics option / CLI flag**: `common/commandline.*` and `Gothic::Options` in `common/gothic.h`.
- **Weather / rain** (fork): `common/world/weather.*`, owned by `World` (`world.weather()`). Reference behavior of
  the original and current status in `docs/fork/research/rain.md`; open checks, gotchas and next steps in
  `docs/fork/handoff-rain.md`. Lighting hook in `shader/lighting/sky_exposure.comp` (push constant `rain`).
- **Run console commands at startup** (fork): `-marvin "set time 13 0;zstartrain 0.5"` executes `;`-separated
  marvin commands after the first frame that drew the world (`MainWindow::runStartupMarvinCmds`, called from
  `render`) and logs `marvin: "<cmd>"` or `marvin: "<cmd>" failed`; what a command prints is logged as
  `marvin output: ...`. Works without `-devmode`. Running them earlier, in `onWorldLoaded`, crashed `save game`:
  its screenshot is drawn before the renderer had drawn the world once.
- **Console save/load**: `save game` / `load game` use `save_slot_1.sav` like the original console (slot 1, not
  the quick save slot 0). `weather` (fork) prints today's rain window, weight and shelter state.
- **Research on the original engine**: `Gothic2.exe` (2.6) is analyzed in a local Ghidra project with named
  ZenGin functions. Record findings in `docs/fork/research/<topic>.md` as behavior and constants in your own words;
  never commit decompiled code.

## Building

Requirements: CMake >= 3.16, a C++20 compiler, `glslangValidator` on `PATH`
(ships with the Vulkan SDK on Windows; `glslang-tools` on Linux; `brew install glslang` on macOS).
Submodules must be initialized:

```bash
git submodule update --init --recursive
```

Configure and build (any platform):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --target Gothic2Notr Spacer -j
```

Output goes to `build/opengothic/`.

### Windows (MSVC)

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1            # RelWithDebInfo, Gothic2Notr + Spacer
powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Clean -Target Gothic2Notr
```

The script imports the MSVC x64 environment via `vswhere`, re-reads `PATH` from the registry (so tools
installed with winget in the same session are found), checks for `cmake`/`ninja`/`glslangValidator`
and initializes submodules if needed. A clean build is ~1100 steps.

Requirements: Visual Studio with the C++ workload, CMake, Ninja, Vulkan SDK, and
**Windows SDK 10.0.26100 or newer**. With older SDKs (for example 10.0.22621) the Tempest DX12
backend fails with `D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW: undeclared identifier`.
DX12 cannot simply be switched off with `-DTEMPEST_BUILD_DIRECTX12=OFF` under MSVC, because
`game/main.cpp` references `Tempest::DirectX12Api` whenever `_MSC_VER` is defined.
Upstream CI builds Windows with MSYS2 MinGW64 (no DX12), so MSVC-only breakage can go unnoticed there.

Gotchas:
- On non-MSVC compilers the engine is built with `-Wall -Wconversion -Werror`. Implicit narrowing
  conversions fail the build. Use explicit casts (`int(x)`, `uint32_t(y)`), matching the existing code.
- A `Debug` build adds `-fsanitize=address,leak`, which MSVC does not accept as written. Use
  `RelWithDebInfo` for everyday work.
- The version string comes from `build.h`. The real one is `common/build.h`; CI overwrites a separate
  `game/build.h`, which only affects `game/main.cpp`.
- There is **no unit test suite**. Verification means: it compiles (locally and on CI for Linux, Windows,
  macOS), `scripts/smoke-test.ps1` passes, and for gameplay changes it behaves correctly in game.

## Running

The game needs an installed copy of Gothic 2 NotR:

```bash
build/opengothic/Gothic2Notr -g "<path to Gothic II>"           # add -nomenu -w newworld.zen to skip the menu
build/opengothic/Gothic2Notr -g "<path>" -window -devmode        # windowed, marvin mode on
```

Runtime files are written to the working directory: `log.txt`, `crash.log`, `Gothic.ini`, saves
(`save_slot_N.sav`). When diagnosing a crash, read `log.txt` and `crash.log` first. `log.txt` is locked while
the game runs; stop the process before reading it on Windows.

### Play folder (for the human playing, Windows)

`scripts/deploy-play.ps1` builds, runs the smoke test and copies the result to a stable folder outside the repo
(default `OpenGothic-play` next to the Gothic installation, override with `-Dest` or `OPENGOTHIC_PLAY_DIR`).
It writes `BUILD.txt` (branch/commit), `Play.bat` and `Play (devmode).bat` (same plus `-devmode`; never add
`-window` to a launcher meant for playing, it disables mouse capture), and never touches saves or
`Gothic.ini` there. It refuses to deploy while the game runs from that folder. Don't run test builds with that
folder as working directory; it holds the user's real saves.

### Smoke test

There is no unit test suite. After a change that compiles, run the smoke test (Windows):

```powershell
# Gothic path: -GothicPath, else $env:OPENGOTHIC_GOTHIC_PATH (also read from the user registry env if the shell is stale)
powershell -ExecutionPolicy Bypass -File scripts/smoke-test.ps1 -Build      # build Gothic2Notr, then test
powershell -ExecutionPolicy Bypass -File scripts/smoke-test.ps1 -Mode idle -Seconds 45 -ExtraArgs "-game:Mod.ini"
```

- `benchmark` mode (default) starts with `-nomenu -window -benchmark ci`. The game loads the world, plays the
  `TIMEDEMO` camera path, logs `Benchmark: low 1% = .. fps = ..` and exits by itself (~35 s on an RTX 5070 Ti).
  Vanilla `newworld.zen` has that camera; other worlds or mods may not, so use `idle` mode for them.
- `idle` mode runs for `-Seconds`, then kills the game; it passes if the game was still alive.
- `-Marvin "<cmd>;<cmd>"` passes console commands to the game (see `-marvin` above); a failed command fails the test.
- `-LoadSave <file>` copies a savegame into the run directory as slot 1 and starts from it (`-save 1`). Save/load
  test in two runs (use `idle`; benchmark mode has no player):
  `-Mode idle -RunDir build\smoke-a -Marvin "set time 13 0;zstartrain 0.5;save game"`, then
  `-Mode idle -RunDir build\smoke-b -LoadSave build\smoke-a\save_slot_1.sav -Marvin "weather"` and read the
  `marvin output:` line in `build\smoke-b\log.txt`.
- `-ScreenshotAt "12,20"` saves `shot_<N>s.png` of the game window at those seconds. The benchmark camera path is
  deterministic, so shots at the same second from two runs show the same view: use that for before/after
  comparisons of visual changes. The window must be visible (not minimized or covered).
- The script passes `-novideo` (fork flag: videos started by scripts are skipped), so a new game in `idle` mode
  starts in the world right away; `-marvin` commands wait for a drawn world frame and would otherwise wait out the
  intro. The Xardas dialog still starts a few seconds in. `goto waypoint <name>` moves the player elsewhere, e.g.
  `NW_CITYFOREST_CAVE_06` for a cave; waypoint names can be read from `Data/Worlds.vdf` as plain strings.
- Each run gets a fresh working directory `build/smoke/` with `log.txt`, `crash.log` (only on crash),
  `Gothic.ini` and `summary.json` (pass/fail, FPS, unique warnings). Compare `warnings` in two
  `summary.json` files to spot new log errors from a change.
- Music is disabled (a `Gothic.ini` with `[SOUND] musicEnabled=0` is written to the run directory); pass `-Music`
  to keep it. `GAMESTART.WAV` (the 43 s main menu theme, played as a sound effect, not via the music system)
  is skipped in benchmark mode, so a rain run is quiet apart from sound effects.
- Exit code 0 = pass, 1 = fail. It opens a real game window, so it needs a desktop session and a GPU.

Expected noise in `log.txt`, not regressions: `unable to load archive` for Union DLLs / zipped VDFs in
`Data/`, `not implemented call [...]`, `invalid spawnpoint`, `Accessing member "C_NPC.AIVAR" without an instance set`.

## Code style

- 2-space indentation, LF line endings, max line length 120 (`.editorconfig`).
- Closing braces of blocks are indented to the level of the block body:
  ```cpp
  if(type!=T_Inventory) {
    visual = world.addView(*hitem);
    }
  ```
- No space between `if`/`for`/`while` and `(`; compact operators (`a==b`, `x=0`) are common.
- File names are all lower case (`playercontrol.cpp`), class names PascalCase, methods camelCase.
- Includes: own header first, then `<Tempest/...>` / `<zenkit/...>`, then project headers relative to
  `common/` (for example `"world/objects/npc.h"`).
- Match the surrounding code rather than introducing a new style. No clang-format config exists;
  do not run a formatter over upstream files.

## Key dependencies

- **Tempest** (`lib/Tempest`, by the same author): window, input, GPU abstraction, audio, signals.
  Engine headers are included as `<Tempest/...>`. Upstream regularly bumps this submodule ("Update Tempest").
- **ZenKit** (`lib/ZenKit`, GothicKit): parsers for Gothic formats (ZEN worlds, MDS/MDH/MRM models,
  textures, VDF archives) and the Daedalus VM. Types live in namespace `zenkit` (`zenkit::INpc`, `zenkit::IItem`).
- **bullet3**: physics. **dmusic**: DirectMusic. **TinySoundFont**: MIDI/SF2 synthesis.

## External references

- Upstream issues: https://github.com/Try/OpenGothic/issues
- Feature list wiki: https://github.com/Try/OpenGothic/wiki/Feature-list
- Implemented script API: https://github.com/Try/OpenGothic/wiki/Daedalus-scripts
- Ikarus support tracking: https://github.com/Try/OpenGothic/issues/231
