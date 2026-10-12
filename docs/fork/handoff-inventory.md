# Handoff: modern inventory (mouse and category tabs)

Status per session, newest first. Read `AGENTS.md`, then `docs/fork/plan-inventory.md` (goal, design, phase
checklist). Keep the checklist in the plan up to date; this file says where things stand and what is next.

## Current status

- Phase 1 (mouse control) implemented on branch `feat/inventory-ui`, not merged into `master` yet.
  Commits: `01d5d61e` (mouse control), `2f145968` (`give` console command for tests).
- Verified by the agent: build, benchmark smoke test (76 fps, same warnings as before), and in a windowed test run
  a left click on a slot used exactly that item (cheese eaten, list moved up).
- **Not verified by the agent: hover and wheel.** The computer-use tool blocks real mouse moves to other windows
  while it controls the screen (no `WM_MOUSEMOVE` reached the game, not even `MainWindow`, also not from
  `SetCursorPos`/`mouse_event`), so hover can only be tested by the user. Clicks did arrive.

## Next step

1. User tests phase 1 in the play folder (fullscreen): cursor appears when moving the mouse with the inventory open,
   hover selects, click equips/uses, wheel scrolls with many items, chest/trade/ransack, cursor hides and camera
   works again after closing.
2. Fix what comes up, merge `feat/inventory-ui` into `master`.
3. Phase 2 (tabs).

## Test setup

Fill the inventory and skip the Xardas dialog (it starts at the start point, so move away first):

```powershell
$items = "ItMw_1h_Bau_Mace","ItRw_Arrow 50","ItPo_Health_01 5","ItFo_Apple 7"   # ... ~40 items to need scrolling
$cmds  = "goto waypoint NW_CITY_HABOUR_05;" + (($items | % { "give $_" }) -join ";")
powershell -ExecutionPolicy Bypass -File scripts/smoke-test.ps1 -Mode idle -Seconds 900 -RunDir build\smoke-inv -Marvin $cmds
```

At the default window size about 39 items fit without scrolling. `log.txt` is only flushed on errors
(`game/main.cpp`), so temporary debug lines need `Log::e` to survive killing the game.

## Session log

### 2026-10-12: phase 1

- Mouse control in `InventoryMenu` (hover, click, wheel, cursor), `[GAME] invMouse`, `give` console command.
- Gotcha: computer-use cannot send mouse moves to the game (see above); clicks work.

### 2026-10-12: plan

- Looked at `common/ui/inventorymenu.cpp`, `MainWindow` mouse handling and `Inventory` sorting; wrote the plan.
- Reference plugin: Union Advanced Inventory; the user's settings for it are in `[ADV_INVENTORY]` in
  `D:\GothicDev\Gothic II OpenGothic\system\Gothic.ini`.
