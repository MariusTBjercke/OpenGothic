# Handoff: mouse-first inventory (branch `feat/inventory-ui`)

> **Partly superseded (2026-10-24):** the inventory was reworked into the broader UI overhaul
> (`docs/fork/plan-ui-overhaul.md`, handoff `docs/fork/handoff-ui.md`). Iterations A and B are done, the
> panel layout/tooltip/drag & drop details in this file are out of date; the keyboard/gotcha notes and
> the test harness section below are still valid. Read the newer handoff first.

Self-contained handoff for a fresh session. Read `AGENTS.md` first, then `docs/fork/plan-inventory.md` (goal,
design decisions, iteration checklist). This file is the current state, the next step and the things that cost time
to rediscover. Session log at the bottom, newest first.

## Where things stand

- Branch `feat/inventory-ui`, HEAD `8e9a208c`, pushed to `origin`, **not merged into `master`**.
- Deployed to `D:\GothicDev\OpenGothic-play` at `8e9a208c` (`BUILD.txt` there), built 2026-10-23.
- Working tree clean. Build current (`scripts/build-windows.ps1 -Target Gothic2Notr`); benchmark smoke test passes:
  ~76 fps, 13 unique warnings (same set as before this work).
- **Iteration A is done and committed. B (drag & drop) and C (tabs in chest/trade, finer categories, polish) are not
  started.** The checklist lives in the plan.

## Code map (what the fork added)

| File | What it does now |
| ---- | ---------------- |
| `common/ui/inventorymenu.{h,cpp}` | The widget. Mouse mode (`isMouseMode`), hit test recorded while drawing (`GridArea`, `selectAt`), tab bar drawing/state (`drawTabs`, `switchTab`, `cycleTab`, `tabAt`), take-all (`drawTakeAll`, `takeAllItems`), scaled layout (`padd()`, `gridTop()`), cursor hint (`updateCursor`). |
| `common/ui/inventorytabs.{h,cpp}` | Category list (All, Weapons, Armor, Potions, Food, Writings, Magic, Other), `matches()`, the `std::function<bool(const Item&)>` filter, and `paintGlyph()` (procedural icons, no assets). |
| `common/game/inventory.{h,cpp}` | `Inventory::ItemFilter` + optional filter on `Inventory::iterator(t, filter)`. Small upstream edit; the rest is untouched. |
| `common/gothic.cpp` | Defaults `[GAME] invMouse=1`, `[GAME] invTabs=1`. With both 0 the inventory behaves like upstream. |

Pages in `inventorymenu.cpp` (`InvPage`, `TradePage`, `RansackPage`) capture `tabs.filter()` (stable address, so
switching tabs re-filters without recreating pages). Only the player side is filtered so far.

## Verified / not verified

Verified by the agent with screenshots + synthetic input:

- hover selects the slot and updates the info box; click acts (apple stack 20 -> 19); the old code made a click on
  the hovered slot a no-op;
- tabs filter the grid; `PageDown` and clicking a tab both work; active tab = bordered slot, empty tab = dimmed
  glyph; the bar is exactly grid-wide; every tab has the same `INV_BACK` + `INV_SLOT` background;
- right click closes; build + benchmark clean.

**Not verified by the agent** (needs the user or a container): take-all in a chest / on a body (no console command
opens one), wheel scrolling with more items than fit, Shift+wheel, fullscreen, chest/trade/ransack interactions,
and `interfaceScale` / `inventoryCellSize` variations.

## Next step: iteration B, drag & drop

Interaction state machine (design decision 7 in the plan):

1. `mouseDown` on a slot only **arms** the interaction (remember side + `clsId`; never keep an `Item*` across a
   move). Release without movement = click: use/equip/take/sell one, keep the "hold still repeats" timer (~350 ms).
2. Movement past ~8 px while held = drag; the repeat timer must not run.
3. Drop on the other grid: player -> chest/body = move stack, chest/body -> player = take stack, trade:
   player -> trader grid = sell, trader -> player = buy. Drop anywhere else = cancel.
4. Ghost of the dragged item at the cursor (`InventoryRenderer::drawItem`, the info box already does this), target
   grid highlighted; right click or `Esc` during a drag cancels instead of closing.
5. Double click (`mouseDoubleClickEvent`) = take/use the whole stack.
6. Re-resolve the dragged item by `clsId` from the source inventory when needed; the inventory can change under the
   drag.

All of it fits in `common/ui/inventorymenu.{h,cpp}`; a `DragState` struct next to `GridArea` is enough. Verify with
the screenshot harness (move with the button held, release over the other grid), then let the user test.

## Open questions for the user (defaults in parentheses)

- Drag moves the whole stack (yes); double click takes the stack (yes).
- Right click keeps closing the inventory (yes); only during a drag it cancels.
- Chest/trade tab sync and a shared tab come in C (yes).
- Merge `feat/inventory-ui` into `master` after the user's test of A (pending).

## Test harness (works, use it)

The computer-use tool cannot move the mouse into the game, but synthetic input works:

1. `powershell -ExecutionPolicy Bypass -File scripts/smoke-test.ps1 -Mode idle -Seconds 34 -RunDir build\smoke-x
   -Marvin "goto waypoint NW_CITY_HABOUR_05;give ItFo_Apple 20;give ItPo_Health_01 5;give ItMw_1h_Bau_Mace;give
   ItRw_Arrow 50" -ScreenshotAt "24"`
2. In a second background job, sleep ~18 s (12 s is sometimes too early: the world may still be loading and the
   inventory key is ignored), find `Get-Process Gothic2Notr` -> `MainWindowHandle`, then **activate the window
   first**: `AttachThreadInput(GetCurrentThreadId(), GetWindowThreadProcessId(GetForegroundWindow()), true)` +
   `ShowWindow(hwnd, SW_RESTORE)` + `BringWindowToTop` + `SetForegroundWindow` + `SetActiveWindow`, or click into
   the window with `mouse_event`. Keys are ignored while the window is inactive (Tempest clears widget focus),
   from `keybd_event` and `PostMessage` alike.
3. Keys: `PostMessage(WM_KEYDOWN/UP = 0x0100/0x0101, lParam = 1 | (scancode<<16))` or `keybd_event`. Mouse:
   `SetCursorPos` (client -> `ClientToScreen`) for hover, `mouse_event(2/4)` for clicks, `MOUSEEVENTF_WHEEL` for the
   wheel. Tab (VK 0x09) opens the inventory, `PageDown` (VK 0x22) cycles tabs.
4. Read the result from `shot_Ns.png` in the run directory. Keep inputs >= 3 s apart.

Layout on the user's machine (4K, `Gothic::interfaceScale` = 2.25): cell 157, padding 96, grid top 301, player grid
at x = 2959, width 785, tab bar y = 237, height 60 (8 tabs, gap 6). General formulas in the plan. For exact rects,
temporarily log them with `Log::e` in `drawAll` (flushes; `log.txt` is locked while the game runs).

## Gotchas learned the hard way

- **Cursor shape:** only the hovered widget's shape reaches the OS cursor (`EventDispatcher::implSetMouseOver`);
  `SystemApi::showCursor` and `Window::hwnd` are not reachable from a widget. The arrow shows up on the first mouse
  move after opening; do not fight the engine over it.
- **Painter culls back-facing triangles.** Emit glyph polygons with a consistent winding, or use the normalised
  `fillFan` in `inventorytabs.cpp`. This is how the right page of the book glyph silently disappeared.
- **`INV_SLOT` is translucent in the middle.** Anything drawn with only `INV_SLOT` shows the world through it; draw
  `INV_BACK` behind it first (what the grid does per cell).
- **Layout offsets** (`43` padding, `134` grid top, `70` header row) used to be raw pixels while cells and fonts
  scale with `interfaceScale`; they are scaled now. At 225% the inventory shows 8 rows; `invMaxColumns` /
  `inventoryCellSize` in `Gothic.ini` change the grid.
- **`drawAll` has a local `padd`** that shadows the `padd()` method; use `this->padd()` there.
- **Save format:** do not bump `Serialize::Version::Current`. Fork-only state goes into a new optional entry.
- **No assets:** the repo ships none. A generated/hand-made icon set (the user floated a local Gemini/OpenAI tool)
  needs an asset pipeline (embed at build, or a `ui/` folder copied by `deploy-play.ps1`) and a license decision;
  see the plan backlog.
- Keep edits in upstream files small and localized; new logic goes into new files.

## First 15 minutes for the next agent

1. `git log --oneline -6`, `git status`; confirm HEAD `8e9a208c` on `feat/inventory-ui` and a clean tree.
2. Build + benchmark: `scripts/build-windows.ps1 -Target Gothic2Notr`, `scripts/smoke-test.ps1` (~76 fps, 13
   warnings).
3. Read `docs/fork/plan-inventory.md` (iterations A/B/C and the design decisions).
4. Ask the user: did the tab-bar fixes (background, book glyph) look right, and does take-all work in a chest? Fix
   that first if not.
5. Then start iteration B at `InventoryMenu::mouseDownEvent` / `mouseUpEvent` / `mouseDragEvent` per the state
   machine above.

## Session log

### 2026-10-23: icon tabs

- User feedback on the first play test: the text tab bar was ~1.6x wider than the grid (8 labels vs 5 columns, same
  ratio at any resolution), so it floated over the world.
- Options discussed: icon tabs (what the Union plugin does, the only thing that scales to 15-20 categories later),
  a vertical text rail (roomy, bigger change, also solves the shared tab for chest/trade), or a clipped/scrolling
  bar. Mocked icon bar and rail in Pillow (`build/mock_tabs.py`, throwaway); the user picked fixed **glyphs**.
- Implemented `InventoryTabs::paintGlyph` with `drawRect` + triangle fans (`p.drawTriangle` with a color brush); no
  textures, no asset pipeline.
- Two follow-up fixes from the user's second look:
  - **Transparency:** `INV_SLOT` is translucent in the middle; tabs that only had `INV_SLOT` let the world show
    through, while the empty tabs (only `INV_BACK`) showed its fine grid pattern. Now `INV_BACK` is drawn once
    behind the whole bar and every tab draws `INV_SLOT` on top, like a grid cell.
  - **Winding:** the painter culls back-facing triangles, and the right page of the book was authored clockwise, so
    it vanished. `fillFan` now normalises the winding from the polygon's signed area.
- Harness: `keybd_event` and `PostMessage` both need an active window (see the harness section); sleeping 12 s is
  sometimes too early.

### 2026-10-23: iteration A

- Fixed the click-after-hover bug: the old `selectAt` reported "selection changed", and hover had already set it, so
  a click on the hovered slot did nothing. It now reports "there is an item under the cursor".
- Implemented `common/ui/inventorytabs.{h,cpp}` and an optional `Inventory::ItemFilter` on `Inventory::iterator()`.
  Pages capture the filter pointer; its address is stable, so switching tabs re-filters without recreating pages.
- Tab bar is drawn in the `Back` pass in the band between the header row and the grid (grid-wide, 8 tabs).
- Take-all collects `(clsId, count)` first and then moves them, because moving items invalidates the iterator.
- Scaled `padd`/`gridTop`/header row with `Gothic::interfaceScale` (the header used to overlap the top row at 225%).
- Cursor: the inventory sets its shape on open/close; Tempest only applies the hovered widget's shape.

### 2026-10-12: phase 1

- Mouse control in `InventoryMenu` (hover, click, wheel, cursor), `[GAME] invMouse`, `give` console command.
- Gotcha: computer-use cannot send mouse moves to the game; clicks work.

### 2026-10-12: plan

- Looked at `common/ui/inventorymenu.cpp`, `MainWindow` mouse handling and `Inventory` sorting; wrote the plan.
- Reference plugin: Union Advanced Inventory; the user's settings are in `[ADV_INVENTORY]` in
  `D:\GothicDev\Gothic II OpenGothic\system\Gothic.ini`.
