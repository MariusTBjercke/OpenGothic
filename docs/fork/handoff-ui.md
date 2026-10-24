# Handoff: modernised UI v1

Self-contained handoff for a fresh session. Read `AGENTS.md`, then `docs/fork/plan-ui-overhaul.md`
(goal and phases) and `docs/fork/plan-inventory.md` (the inventory iterations A/B/C this build
started from). This file is the state, what is verified and what is not, and the things that cost
time to rediscover.

## Where things stand (2026-10-24, night session)

Branch `feat/inventory-ui`, working tree has the UI overhaul, **not committed yet** at the time of
writing (the session log at the bottom says what landed). Build is green
(`scripts/build-windows.ps1 -Target Gothic2Notr`), benchmark smoke test passes with the same warning
set as before (see `build/smoke/summary.json`).

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

- inventory: hover selects and shows the tooltip (`build/ui-inv5/shot_26s.png`), single click uses one
  item (apple 20 -> 19, `shot_37s.png`), drag shows the ghost with the stack count and cancels when
  released outside a grid (`drag_37.png`), tab click filters the grid, tab hover shows the label,
  keyboard navigation still works with `invMouse=0 invTabs=0` (`build/ui-kbd/shot_27s.png`, info box
  with the dark plate, no tabs).
- dialogs: hovering a choice selects it and shows the accent band; clicking a choice takes it
  (`build/ui-dlg4/`), the choice panel no longer clips its last row.
- main menu: hover changes the selection (the help line at the bottom follows), clicking "Options"
  opens the options menu (`build/ui-menu2/shot_16s.png`).
- build + benchmark smoke test.

**Not verified by the agent** (needs the user, a container, or a console command that opens one):

- the actual chest/trade/ransack drops (drag onto the other grid) — there is no console command to
  open a container or a trader, so only the cancel path and the equip-state ghost could be exercised;
- double click on a stack inside a container (the code path is shared with drag);
- `Shift+A` take all (button path was verified in an earlier session);
- fullscreen cursor behaviour (the harness runs `-window`);
- the fine category mapping for keys/trophies against real game data (no dump of the instance names
  was made; flags decide first, prefixes only for `ItAt_`/`ItKe_`).

## Next steps (suggested order)

1. Play-test: inventory in a chest and with a trader, drag both ways, double click a stack, `Shift+A`.
2. If a stack move is wrong, look at `InventoryMenu::resolveDrop` (it re-resolves by `clsId` and moves
   the whole current stack) and the per-state branches there.
3. Save/load menu: verify the gold underline does not fight the savegame slot textures
   (`GameMenu::drawItem`, the marker is skipped when the item has an image).
4. Backlog from the plan: numeric HUD values and a damage trail on the bars, dialog page numbers for
   long choice lists, tooltip for the equipped slot, item icons for the tabs (needs an asset pipeline).

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

### 2026-10-24: UI overhaul v1

- Wrote `docs/fork/plan-ui-overhaul.md`, then the kit, then the windows one by one, building and
  screenshot-testing after each step.
- Inventory layout was rebuilt around frames: tab strip (full content width), per-side panels with
  header/footer, 10 rows at 225 % (was 8), tooltip at the cursor, drag & drop, click on release.
- Added the second click of a double click problem and its resolution (see gotchas).
- Dialog and scripted-menu mouse support; document click-to-close; HUD plates; fullscreen cursor gate.
- Verified with the harness described above; wrote this file.
