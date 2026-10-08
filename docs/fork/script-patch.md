# Gothic II Script Patch with OpenGothic

The [Gothic II Script Patch](https://forum.worldofplayers.de/forum/threads/1583819-Release-Gothic-II-Script-Patch)
fixes quests, dialogs and balancing in the original scripts. Its compiled scripts run in OpenGothic as an ordinary
mod. Nothing from it is shipped with this repository: the files contain the game's scripts, dialog text and voice
recordings, which we may not redistribute.

## What works

- The patched scripts (`GOTHIC.DAT`, `MENU.DAT`) and dialog texts (`OU.BIN`). They use exactly the same engine
  externals as the vanilla scripts (checked on version 26 EN, 2026-10-08), so no engine changes are needed.
- The English voice additions (`scriptpatch_speech_en.mod`).

What does not apply: the Union plugins and patches bundled in `scriptpatch.mod` (`*.DLL`, `*.PATCH`, for example
torch control, lock-pick animations, step height). They modify the original `Gothic2.exe`, which OpenGothic does
not use. Most of them fix engine bugs of the original.

## Setup

1. Get the Script Patch:
   - Steam: subscribe to it in the Workshop. The files land in
     `<Steam library>\steamapps\workshop\content\39510\2792250061\`.
   - Otherwise download it from the forum thread linked above.
2. Copy into the Gothic II folder that OpenGothic uses (no conversion needed):
   - `Data\ModVDF\scriptpatch.mod`, `scriptpatch_en.mod`, `scriptpatch_speech_en.mod` → `<Gothic>\Data\ModVDF\`
   - `system\g2a_nr_scriptpatch_en.ini` → `<Gothic>\system\`

   For another language use the matching files (`_de`, `_pl`, `_ru`); the ini lists which `.mod` files it needs
   under `[FILES] VDF=`.
3. Start OpenGothic with the mod ini:

   ```
   Gothic2Notr.exe -g "<Gothic>" -game:g2a_nr_scriptpatch_en.ini
   ```

   Run it from its own working directory (see below).

## Saves

OpenGothic writes saves (`save_slot_N.sav`) and `Gothic.ini` to the working directory, the same for every mod.
A save holds the state of the scripts it was made with, so do not load vanilla saves with the Script Patch or the
other way round. Start a new game with the Script Patch and keep its saves in a separate folder.

Devmode (`-devmode`, marvin console) does not change the save format; a normal and a devmode run of the same
scripts can share saves.

## Play folder (fork helper)

`scripts/deploy-play.ps1` adds two launchers when the Gothic folder has `system\g2a_nr_scriptpatch_*.ini`:

| Launcher | Scripts | Saves in |
| -------- | ------- | -------- |
| `Play.bat`, `Play (devmode).bat` | vanilla | play folder |
| `Play (Script Patch).bat`, `Play (Script Patch, devmode).bat` | Script Patch | `ScriptPatch\` |

The Script Patch launchers start the game in `ScriptPatch\`, which gets its own `Gothic.ini` with default
settings on first start. Interface scale is not affected; it lives in `<Gothic>\system\SystemPack.ini`.
