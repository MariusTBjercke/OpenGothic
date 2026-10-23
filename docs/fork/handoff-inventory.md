# Handoff: modern inventory (mouse-first UI, category tabs)

Status per session, newest first. Read `AGENTS.md`, then `docs/fork/plan-inventory.md` (goal, design,
iteration checklist). Keep the checklist in the plan up to date; this file says where things stand and what is
next.

## Current status

- Iteration A is implemented on branch `feat/inventory-ui` (not merged into `master`):
  - click after hover works (the old `selectAt` reported "selection changed", so a click on the hovered slot did
    nothing; it now reports "there is an item under the cursor"),
  - category tabs: All / Weapons / Armor / Potions / Food / Writings / Magic / Other, clickable tab bar above the
    player grid, `PageUp`/`PageDown`, wheel over the bar, Shift+wheel anywhere, per-tab selection/scroll memory,
    empty tabs dimmed, `[GAME] invTabs=1`,
  - "Take all" button in chest/ransack (header row, right-aligned to the other grid),
  - layout offsets (padding, grid top, header row) now scale with `Gothic::interfaceScale`; at 225% DPI the header
    used to overlap the top slot row, and the tab bar needs that band.
- Verified by the agent (screenshots + synthetic input, see below):
  - hover selects the slot and updates the info box,
  - a click on the hovered slot acts: apple stack went 20 -> 19,
  - `PageDown` and a click on the "Food" tab both filter the grid to one stack,
  - right click closes the inventory,
  - build clean, benchmark smoke test passes (76.5 fps, 13 unique warnings, same set as before).
- **Not verified by the agent:** "Take all" in game (needs a chest or a body; no console command opens one),
  wheel scrolling with more items than fit (the grid has ~8 rows here, which needs >40 distinct items), Shift+wheel,
  fullscreen, and everything in chest/trade/ransack.

## Next step

1. User tests in the play folder (deployed from this branch): hover, click, tabs (click, `PageDown`, wheel over the
   bar), take all from a chest, trade, ransack, fullscreen. Fix what comes up.
2. Merge `feat/inventory-ui` into `master` once the user test is fine.
3. Iteration B: drag & drop (press arms, release acts, movement past ~8 px drags, hold still repeats), double click
   takes the stack, ghost item at the cursor, target grid highlighted. See the plan for the details.

## Test harness (works, use it)

Synthetic input reaches the game when the window is in the foreground, even though computer-use cannot move the
mouse into it. Recipe that worked on 2026-10-23:

1. Start the game with `scripts/smoke-test.ps1 -Mode idle -Seconds N -RunDir build\smoke-x -Marvin "goto waypoint
   NW_CITY_HABOUR_05;give ItFo_Apple 20;..." -ScreenshotAt "17,23,31"`.
2. In a second background job, sleep ~12 s, find the window (`Get-Process Gothic2Notr` -> `MainWindowHandle`),
   `SetForegroundWindow`, then `keybd_event(0x09)` = Tab to open the inventory, `SetCursorPos` (convert client
   coordinates with `ClientToScreen`) to hover, `mouse_event(2/4)` to click, `keybd_event(0x22)` = PageDown,
   `MOUSEEVENTF_WHEEL` for the wheel. Read the result from the `shot_Ns.png` files.
3. Keep timings >= 3 s apart so a screenshot falls between two inputs.

Layout on the user's machine: 4K window, `Gothic::interfaceScale` = 2.25 -> cell 157, padding 96, grid top 301,
player grid at x = 2959, width 785, tab bar at y = 237, height 60. General formulas are in the plan
("Testability and verification"). For exact tab rects, temporarily add `Log::e` lines in `drawAll` and remove them
again (done once; the code is clean now).

## Session log

### 2026-10-23: iteration A

- Found and fixed the click-after-hover bug. The old code only acted when `selectAt` changed the selection, and
  hover had already set it; a synthetic click on the hovered apple slot did nothing, after the fix it eats one.
- Implemented `common/ui/inventorytabs.{h,cpp}` (tab list, predicates, active tab) and an optional
  `Inventory::ItemFilter` on `Inventory::iterator()` (`game/inventory.*`, small upstream edit) so `Page::size/get`,
  `drawItems` and the info box keep working unchanged. Pages capture the filter pointer; its address is stable,
  so switching tabs re-filters without recreating the pages. The other side (chest/trader) has no filter yet.
- Tab bar is drawn in the `Back` pass into the band between the header row and the grid, right-aligned to the player
  grid and clamped to the left padding; widths come from the font, so 8 tabs fit at 4K/225%.
- Take-all collects `(clsId, count)` first and then moves them (`player->addItem(cls, *chest/trader, count)`),
  because moving items invalidates the iterator.
- Scaled `padd`/`gridTop`/header row with `Gothic::interfaceScale`. Verified visually: the header no longer
  overlaps the top row and the tab bar has room.
- Cursor: Tempest applies only the hovered widget's shape, and `SystemApi::showCursor`/`Window::hwnd` are not
  reachable from a widget. The inventory sets its shape on open/close; the arrow appears on the first mouse move.
  Good enough, do not fight the engine over it.
- Gotcha: `drawAll` has a local `padd` variable that shadows the new `padd()` method (a temporary log call failed
  on `padd()`); if you add code there, use `this->padd()` or rename the local.

### 2026-10-12: phase 1

- Mouse control in `InventoryMenu` (hover, click, wheel, cursor), `[GAME] invMouse`, `give` console command.
- Gotcha: computer-use cannot send mouse moves to the game (see above); clicks work.

### 2026-10-12: plan

- Looked at `common/ui/inventorymenu.cpp`, `MainWindow` mouse handling and `Inventory` sorting; wrote the plan.
- Reference plugin: Union Advanced Inventory; the user's settings for it are in `[ADV_INVENTORY]` in
  `D:\GothicDev\Gothic II OpenGothic\system\Gothic.ini`.
