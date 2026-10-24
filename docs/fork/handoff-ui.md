# Handoff: modernised UI v1

Self-contained handoff for a fresh session. Read `AGENTS.md`, then `docs/fork/plan-ui-overhaul.md`
(goal and phases) and `docs/fork/plan-inventory.md` (the inventory iterations A/B/C this build
started from). This file is the state, what is verified and what is not, and the things that cost
time to rediscover.

## Where things stand (2026-10-24, night session)

Branch `feat/inventory-ui`, **committed** (`cd9f0f78` is the docs commit on top; the feature commits
are `b686bfb1`, `168a087f`, `c3535664`, `0b79df26`, `f454a21a`, `d5a7fc1f`, `c8b6f96b`), working tree
clean. Build is green (`scripts/build-windows.ps1 -Target Gothic2Notr`), benchmark smoke test passes:
76 fps, 13 unique warnings, the same set as before this work. Deployed to `D:\GothicDev\OpenGothic-play`
with `scripts/deploy-play.ps1` at the end of the session (see `BUILD.txt` there).

Shipped in this pass:

1. **`common/ui/uidraw.{h,cpp}`** (new): the fork's immediate-mode drawing kit — dim backdrop, framed
   panels, hover/selection accents, floating tooltips, scroll bars, a hit list, text helpers.
2. **Inventory** (`inventorymenu.*`, `inventorytabs.*`): the window is now a framed panel with a
   header (title + gold), an icon tab strip above it, a hint footer, a dark plate under everything, a
   tooltip at the cursor (name, count, stat rows, value/sell price), drag & drop with a ghost and a
   target highlight, double click for whole stacks, scroll-wheel scrolling, `Shift+A` take all.
   Eleven categories matched from flags plus instance-name prefixes (trophies/keys), one filter for
   both grids, empty tabs dimmed, hovered tab shows its name.
3. **Dialogs** (`dialogmenu.*`): choice rows can be hovered (the row under the cursor is selected and
   highlighted with an accent band + bar) and clicked; the choice panel got a dark plate, a stable
   height and a selection accent.
4. **Scripted menus** (`gamemenu.*`, `menuroot.cpp`): every item records its rect while painting;
   hover selects (with `MENU_BROWSE`), click activates (`MENU_SELECT`), sliders can be dragged, choice
   boxes step by clicking their left/right third. The selected item gets a gold underline, because the
   highlight font alone is invisible in some menus (the main menu renders both fonts nearly alike).
5. **Documents** (`documentmenu.*`): dim backdrop, click (left or right) closes, click hint at the
   bottom.
6. **HUD** (`mainwindow.cpp`): soft plate behind the health/mana bars, dark name plate behind the
   focus label.
7. **Fullscreen cursor gate**: `MainWindow::uiOwnsMouse()` stops the engine from re-centering the
   cursor (and from orbiting the camera) while a mouse-driven window is open. Without it every mouse
   menu is unusable in fullscreen.

Settings, all in `[GAME]` with defaults in `common/gothic.cpp`:

| key | default | effect |
| --- | ------- | ------ |
| `invMouse` | 1 | hover/click/drag in the inventory |
| `invTabs` | 1 | category tab strip |
| `invScrim` | 1 | dim the world behind the inventory |
| `invTooltip` | 1 | item tooltip at the cursor |
| `dlgMouse` | 1 | hover/click dialog choices |
| `menuMouse` | 1 | hover/click scripted menus |
| `docMouse` | 1 | click a document to close it |

## Verified / not verified

Verified by the agent with synthetic input and screenshots (run dirs under `build/`, kept locally):

- inventory: hover selects and shows the tooltip (`build/ui-final/shot_36s.png`), single click uses one
  item (apple 20 -> 19, `build/ui-inv5/shot_37s.png`), drag shows the ghost with the live stack count and
  cancels when released outside a grid (`build/ui-inv5/drag_37.png`), tab click filters the grid, tab
  hover shows the label, keyboard navigation still works with `invMouse=0 invTabs=0`
  (`build/ui-kbd/shot_27s.png`).
- **trade layout and drag both ways** (`build/ui-trade4/`): two panels ("Kardif" with its item count,
  "Inventory" with gold), one shared tab strip, hint line, tooltip with the sell price; dragging the
  apple stack from the player grid onto the trader grid sells the stack (the trader's stack grew from 1
  to 20 while the player's gold went up), dragging the trader's stack back buys (clamped by the player's
  gold). This used a temporary `F8` test hook (nearest NPC -> `inventory.trade`), removed again; the
  recipe is at the bottom of the harness section.
- dialogs: hovering a choice selects it and shows the accent band; clicking a choice takes it
  (`build/ui-dlg4/`), the choice panel no longer clips its last row.
- main menu: hover changes the selection (the help line at the bottom follows) and shows the gold
  marker, clicking "Options" opens the options menu (`build/ui-menu4/shot_12.png`); the load menu opens
  with a click, hovering a slot moves the marker and the info line together (`build/ui-load/shot_11s.png`).
- equip: a single left click on the weapon equips it (the slot shows the red equipped overlay and the
  number-slot "1", `build/ui-equip/shot_30s.png`).
- build + benchmark smoke test (twice, before and after the final fixes).

**Not verified by the agent** (needs the user or a specific spot in the world):

- chest and ransack drops (the trade path is verified; chest/ransack use the same `resolveDrop` branches
  but no container was opened) and the "Take all" button in a real chest;
- double click for whole stacks inside a container (the code path is shared with the drop);
- `Shift+A` take all;
- fullscreen cursor behaviour (the harness runs `-window`);
- the fine category mapping for keys/trophies against real game data (no dump of the instance names was
  made; flags decide first, prefixes only for `ItAt_`/`ItKe_`).

## Next steps (suggested order)

1. Play-test: inventory in a chest and with a trader, drag both ways, double click a stack, `Shift+A`.
2. If a stack move is wrong, look at `InventoryMenu::resolveDrop` (it re-resolves by `clsId` and moves
   the whole current stack) and the per-state branches there.
3. Wire `UiDraw::scrollBar` into the grids for tall inventories (the wheel works, the indicator is not
   drawn yet), and consider numeric values + a damage trail on the HUD bars (phase 3 leftovers).
4. Backlog from the plan: dialog page numbers for long choice lists, tooltip for the equipped slot, item
   icons for the tabs (needs an asset pipeline).

## Code map (what is new or changed in this pass)

| File | What it does now |
| ---- | ---------------- |
| `common/ui/uidraw.*` | drawing kit: `panel`, `scrim`, `selection`, `hover`, `tooltipPlane`, `scrollBar`, `HitList`, `textCenter/Left/Right`, `Theme`, `Metrics` |
| `common/ui/inventorymenu.*` | layout (tab strip, per-side panels, footer), tooltip, drag state machine, click-on-release, hold-repeat timer, keyboard path unchanged |
| `common/ui/inventorytabs.*` | 11 categories, `category()` with a class-id cache, `categories()` for dimming, new glyphs (bow, fang, key) |
| `common/ui/dialogmenu.*` | choice hit list, hover selection, accent band, dark plate under the choice texture |
| `common/ui/gamemenu.*` | per-item rects, mouse hover/click/drag, slider drag, choicebox edges, selection underline |
| `common/ui/documentmenu.*` | backdrop, click-to-close, hint |
| `common/ui/menuroot.cpp` | cursor shape follows `menuMouse` |
| `common/mainwindow.*` | `uiOwnsMouse()`, bar plate, focus name plate |
| `common/gothic.cpp` | new defaults |
| `docs/fork/plan-ui-overhaul.md` | plan, phase checkboxes |

## Test harness (works, use it)

`build/ui-run.ps1` is a fork-local harness (throwaway, lives in `build/`): starts the game in its own
working directory with `-window -novideo` (optionally `-nomenu`, `-marvin`, extra `Gothic.ini` lines)
and saves `shot_<N>s.png` at the given seconds. Two examples:

```powershell
# inventory: open it via synthetic Tab, then hover / click / drag
powershell -ExecutionPolicy Bypass -File build/ui-run.ps1 -RunDir build/ui-inv5 -NoMenu `
  -Marvin "goto waypoint NW_CITY_HABOUR_05;give ItFo_Apple 20;give ItMw_1h_Bau_Mace" `
  -ScreenshotAt "26,37" -Seconds 48
powershell -ExecutionPolicy Bypass -File build/ui-inv4-input.ps1      # runs in parallel

# main menu (no -nomenu)
powershell -ExecutionPolicy Bypass -File build/ui-run.ps1 -RunDir build/ui-menu2 -ScreenshotAt "9,13" -Seconds 20
powershell -ExecutionPolicy Bypass -File build/ui-menu-input.ps1
```

Keyboard-mode regression: `-Ini "[GAME]`r`ninvMouse=0`r`ninvTabs=0"`.

Opening a trade window for a UI test (no console command exists for it; this is the recipe that worked,
remove it again before committing): add a temporary block at the top of `MainWindow::keyDownEvent` bound
to a key without a `Shortcut` (F8 or F10; F10 was taken by `funcKey`), find the nearest NPC and open the
window:

```cpp
  if(event.key==Event::K_F8) {
    auto pl = Gothic::inst().player();
    Npc* other = nullptr;
    if(auto w = Gothic::inst().world())
      w->detectNpcNear([&](Npc& n){ if(other==nullptr && &n!=pl) other=&n; });
    if(pl!=nullptr && other!=nullptr)
      inventory.trade(*pl,*other);
    event.accept();
    return;
    }
```

Then drive it like the inventory (left panel = trader grid at x 96..881, right = player grid).

Gotchas:

- **The input script's clock is ~3 s behind the screenshot clock** (it starts after the harness), so a
  hover at input t=28 shows up in `shot_31s.png`. Plan for the offset or the screenshots will look
  like the feature does not work.
- Synthetic input drops events now and then: repeat a click if a screenshot shows nothing happened.
  `keybd_event`/`mouse_event` need the game window to be foreground (the harness brings it up; the
  input scripts call `Focus()` + a click into the world).
- In-game client coordinates: **window/screenshot y = client y + ~14 px** on this 4K setup. Layout
  formulas (scale 2.25): grid x = `w()-96-785`, grid y = 301, cell 157, tab strip y 97..187, tab width
  65 with gap 6, first tab centre x ≈ 2959+32 (+71 per tab). Header y 220..301, footer under the grid.
- Dialog choice rows in the intro dialog sit at client y ≈ 1835..1947 (row 1 ~1860, row 2 ~1915).
- A double click arrives as `mouseDoubleClickEvent` *instead of* a second `mouseDownEvent`, and the
  first click already acted on release. Do not act on the second click in the same way (that is why
  double click is a no-op in the equip state and "whole stack" in containers).
- **Draw order of the inventory:** `Renderer::draw` paints the UI layer, then the 3D item models, then
  the *front overlay* (`InventoryMenu::paintNumOverlay`, `DrawPass::Front`). Anything drawn in the
  normal UI layer ends up *under* the item models — that is why the tooltip lives in the front pass now
  and uses a near-opaque plate (`UiDraw::tooltipPlane(..., 0.98f)`). Keep it there, and keep the plate
  opaque, or the icons will float over the tooltip text again.
- **Tempest calls `mouseDoubleClickEvent` for *any* two presses on the same widget within 1000 ms**, and
  it keeps doing so (every press updates the timer). A player clicking tabs or two different items at
  normal speed therefore never reaches `mouseDownEvent` after the first click. A widget that overrides
  `mouseDoubleClickEvent` must behave exactly like a single click unless the double click really targets
  the same thing: `InventoryMenu::mouseDoubleClickEvent` routes tabs, the take-all button and a
  *different* slot to the normal click path, and only a second click on the *same* slot takes the whole
  stack (`lastClickSide`/`lastClickIndex`). This was the "tabs stop reacting after a couple of clicks"
  bug — see the session log.
- A second item click while the first item's use animation is still running is refused by the game
  (`Npc::setAnimItem` needs `BS_STAND` and resets the player to idle), so a fast two-slot click test in
  the harness looks like "nothing happened" even though the click was dispatched. Not a UI bug.
- The hold-to-repeat timer starts on press. A player who presses, hesitates and then starts to drag
  used to take one item before the drag began; a movement of more than 1 px now restarts the delay
  (`mouseDragEvent`). Keep that if you touch the timer.
- The in-game client area and the screenshot differ by ~14 px at the top (window frame). All mouse
  coordinates in the input scripts are client coordinates, `PrintWindow` captures the whole window.

## Code map additions after the first pass

- `InventoryMenu::drawHints` draws the screen-level hint line; there is no per-panel footer any more
  (it clipped the text on 4K). The trader header shows `N items` when there is no "Take all" button.
- The tab strip is flush on top of the panels and exactly as wide as the content (one panel in equip
  mode, both in a trade window). Its cells are **contiguous** and fill the bar exactly: the leftover
  pixels of the integer division go to the first cells, so there is no gap at the right end and no dead
  pixel between two tabs. `InventoryMenu::tabAt` is a *geometric* lookup over `tabBar` (`band`, `cellW`,
  `extra`, `strip`) instead of per-cell hit rects — a click anywhere in the bar (including its padding)
  maps to a tab. That replaced the old 6 px gaps, which were click-through dead zones and the reason
  tabs sometimes "did not react".
- `mouseDownEvent` cancels an armed/active drag before anything else, and `mouseLeaveEvent` cancels a
  drag: a mouse-up that never reached the window (released outside, alt-tab) used to leave a stale drag
  that ate the next click on a tab and acted on the old slot.
- The grid gets an explicit 2 px frame (`UiDraw::Theme::gridLine`) on all four sides; the cell textures
  have no border on their outer left/top edge, which made the table look clipped against the panel.
- The item tooltip is drawn in the front overlay pass (see the draw-order gotcha above).

## Gotchas learned the hard way

- **The stock UI textures are translucent** (`INV_BACK`, `INV_SLOT`, `DLG_CHOICE`). A panel that only
  draws the texture lets the world shine through; the dark fill in `UiDraw::panel` is what makes the
  window readable. Keep the fill >= 0.8 for the inventory.
- **Measure dialog rows with the font that draws them.** The selected row uses the `Hi` font; measuring
  with the normal font made the panel one row short and clipped the last choice.
- **Painter culls back-facing triangles**, `fillFan` normalises the winding (see the earlier note about
  the book glyph). New glyphs use it.
- **Only the hovered widget's cursor shape reaches the OS cursor**; a menu that wants the arrow must set
  it on itself *and* on its root (`MenuRoot`), otherwise the cursor disappears over the background.
- `Gothic::settingsSetF/I` emit `onSettingsChanged` synchronously; do not touch menu items after the
  call (they can be re-initialised). Slider drag only writes the setting and repaints.
- Keep `inventorymenu.cpp`'s keyboard path untouched when editing the mouse path; the user may run with
  `invMouse=0` and expects vanilla keys to work.

## Session log

### 2026-10-24 (afternoon, second round): loot window

- User report: switching tabs while looting an NPC "triggered looting like in vanilla". Root cause was
  not the click routing: `InventoryMenu::tick()` closes the ransack window when the ransack iterator is
  empty, and the iterator used the **category filter**, so clicking a tab whose category the corpse has
  no items in made the window vanish mid-click — the rest of the click / the next click then landed in
  the world and did a vanilla loot click. The "anything left to loot" check is unfiltered again; the
  filter only decides what the tab shows.
- Also fixed: in the loot window the bar belongs to the *corpse* grid (ransack only draws that side), so
  `switchTab` now remembers/restores that side instead of pointing the active page at the invisible
  player grid.
- Verified with a dead NPC (temporary F8 hook that killed the nearest NPC and opened the loot window,
  removed again): the bar sits above the corpse grid, switching to a category the corpse has nothing in
  keeps the window open, switching back shows the items, and clicking a corpse slot still takes that
  stack (vanilla). Note that "Take all" in the loot window moves what the active tab shows.
- Two harness gotchas seen again: clicks near the left/top of the screen can land in the terminal window
  that sits in front (the game must be foreground), and the input clock is ~3 s behind the screenshots.

### 2026-10-24 (afternoon): tab bar polish and the double-click trap

- User report 1: item models floated over the tooltip text. The item models are drawn *between* the UI
  layer and the front overlay (`Renderer::draw`), so the tooltip moved to `paintNumOverlay`
  (`DrawPass::Front`) with a near-opaque plate.
- User report 2: the tab bar was not the same width as the panel and did not fill its width evenly. The
  strip is now flush on the panel(s), the cells are contiguous, the remainder is spread over the first
  cells, and the grid got an explicit 4-sided frame (the cell textures have no border on their outer
  left/top edge).
- User report 3: tabs "locked" after a click or two, no fixed pattern. Root cause found with logging:
  Tempest reports any two presses within a second as a double click, and `mouseDoubleClickEvent` only
  handled grid slots, so from the second tab click on nothing happened (the pattern depended on click
  speed). Fixed by routing double clicks by position; a second click on a *different* slot is a normal
  click, only the same slot twice takes the whole stack. Verified with a 5-click rapid sequence ending
  on the fifth tab.
- Also hardened while there: a new press or leaving the window cancels a stale drag (a lost mouse-up
  used to eat the next click), and the tab hit test is a geometric lookup so no pixel of the bar is
  dead.

### 2026-10-24: UI overhaul v1

- Wrote `docs/fork/plan-ui-overhaul.md`, then the kit, then the windows one by one, building and
  screenshot-testing after each step.
- Inventory layout was rebuilt around frames: tab strip (full content width), per-side panels with
  header/footer, 10 rows at 225 % (was 8), tooltip at the cursor, drag & drop, click on release.
- Added the second click of a double click problem and its resolution (see gotchas).
- Dialog and scripted-menu mouse support; document click-to-close; HUD plates; fullscreen cursor gate.
- Verified with the harness described above; wrote this file.
