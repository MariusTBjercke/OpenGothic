# Plan: modern inventory (mouse-first UI, category tabs)

Fork feature. Goal: an inventory that is pleasant to use with the mouse for nearly every operation, split
into category tabs, inspired by the Union plugin the user plays with in Steam Gothic II. Status per session
goes in `docs/fork/handoff-inventory.md`; this file holds the goal, the design and the iteration checklist.
Update both when something changes.

## Reference: Union Advanced Inventory

The user's Steam Gothic II runs "Union Advanced Inventory" by Хедин
(https://steamcommunity.com/sharedfiles/filedetails/?id=2787311989). It is a Union DLL that patches `Gothic2.exe`,
so it never runs in OpenGothic; we build the ideas natively. Do not copy its textures (license unknown).

Features it has: mouse support with a visible cursor, category tabs (custom categories, an item can be moved to
any category), a favorites tab, a "recently received" tab, a history of items in and out, search, take all,
a compare page, tips on the active item.

Its settings live in `[ADV_INVENTORY]` in `Gothic.ini` (also present in `D:\GothicDev\Gothic II OpenGothic\system`,
copied from the Steam install). Useful hints about how the user has it set up:

- `categoriesOrder=CAT_LAST|ALL|FAVORITE|MAGIC_RUNES|WEAPON_1H|WEAPON|BOWS|ARMOR|TROPHY|DOC|MAPS|KEYS|BOXES|POTIONS|FOOD|RUNE|MAGIC|OTHER|CUSTOM_1|CUSTOM_2`
- `bVerticalLayoutCategories=0`, `bNeedShowIconsCategories=1`: horizontal tab bar with icons.
- `keySwitchCatBase=KEY_LSHIFT` + `keySwitchCatSecond=MOUSE_WHEELDOWN` (and `..Back` = wheel up): Shift+wheel
  cycles tabs; `bAllowCyclingCategories=1` wraps around.
- `bNeedSyncPlayerAndTradeCategories=1`: in trade/chest both sides show the same tab.
- `bAllowShowMouseCursor=1`, `invAdvCntRows=9`, `invAdvCntCol=6`.
- `keyTakeAllMain=KEY_RSHIFT` + `keyTakeAllSecond=KEY_A`: take all.
- `needShowActiveItemTips=1`: tooltip for the hovered item.

## How the inventory works today

All of it is OpenGothic C++, not Daedalus script (in the original it is hardcoded in the exe as well):

- `common/ui/inventorymenu.{h,cpp}` (~880 lines): one widget for equip, chest, trade, ransack and lock picking
  (`State`). A `Page` wraps an `Inventory::Iterator` (`T_Inventory`, `T_Trade`, `T_Ransack`); `pagePl` is the player,
  `pageOth` the chest/trader. `pageLocal[2]` holds selection and scroll per side, `page` says which side is active.
- Drawing: `drawAll` -> `drawItems` (grid of `columsCount` x `rowsCount()` slots) -> `drawSlot`. The 3D item models
  are queued with `renderer.drawItem(...)` during the paint pass and drawn later by `InventoryRenderer`, so a slot's
  2D rect and its model must come from the same coordinates.
- Order: `Inventory::sortItems` / `Inventory::less` sort by `[GAME] invCatOrder`
  (default `COMBAT,POTION,FOOD,ARMOR,MAGIC,RUNE,DOCS,OTHER,NONE`), then flags, damage, cost.
- Categories available from game data: `Item::mainFlag()` = `ITM_CAT_*` in `common/game/constants.h`
  (`NF`, `FF`, `MUN`, `ARMOR`, `FOOD`, `DOCS`, `POTION`, `LIGHT`, `RUNE`, `MAGIC`, `NONE`).
- Finer categories need `Item::itemFlag()` = `ITM_*` in the same file (`DAG`, `SWD`, `AXE`, `2HD_SWD`, `2HD_AXE`,
  `BOW`, `CROSSBOW`, `SHIELD`, `RING`, `AMULET`, `BELT`, `TORCH`, `MISSION`, `MULTI`) and/or the instance-name
  prefix (`ItKe_`, `ItAt_`, ...) via `GameScript::findSymbol(item.clsId())`; cache per `clsId`.
- Mouse in phase 1 (`[GAME] invMouse=1`): `drawItems` records `{rect, side, wcount, hcount}` for each grid, mouse
  events hit-test that list (`selectAt`, `gridAt`). Hover selects the slot, click acts on the selection, wheel
  scrolls. `isMouseMode()` gates all of it; when the inventory ignores an event, `MainWindow` gets it back
  (cursor warp, camera orbit).

## Design decisions

1. **Keep `InventoryMenu`, add to it in small steps; put new logic in new files.** A separate copy of the widget
   would miss upstream fixes. Edits in `inventorymenu.*` stay small hooks (hit test, filter in `Page::iterator`,
   draw the tab bar); tab definitions, the filter predicate and the tab-bar layout go in a new
   `common/ui/inventorytabs.{h,cpp}` (globbed, re-run CMake configure after adding).
2. **Switchable, on by default in the fork.** `[GAME] invMouse=1` and `[GAME] invTabs=1` in `Gothic.ini`
   (defaults in `Gothic::Gothic`, next to `invCatOrder`). With both 0 the inventory behaves like upstream.
3. **System cursor first** (`CursorShape::Arrow` while the inventory is open in mouse mode). A drawn Gothic-style
   cursor can come later; vanilla data has no cursor texture to reuse.
4. **Hit test from what was drawn.** `drawItems` records `{page, index, rect}` for each visible slot during the
   `Back` pass; mouse events look up that list. No second copy of the layout math.
5. **Tabs from `ITM_CAT_*` first.** Finer categories (keys, maps, trophies, 1H/2H, bows) need item flags or
   instance-name heuristics and come later (backlog).
6. **No savegame changes in iterations A-C.** Favorites, custom categories and history need saved state: when
   that comes, follow the fork rule in `AGENTS.md` (new optional save entry, never bump
   `Serialize::Version::Current`).
7. **Click acts on release once drag & drop exists.** A drag must be able to start from a slot, so `mouseDown`
   only arms the interaction: release without movement = click (use/take/sell one), movement past a threshold
   (~8 px) = drag, holding still for ~350 ms = the old "keep taking" repeat. Until iteration B, click keeps
   acting on press (no drag yet).
8. **Drag moves the whole stack.** Single click takes one (as today, with the repeat timer), double click takes
   the stack, drag takes the stack to the drop target. Modifier-based amounts are a later refinement.
9. **`Page::iterator()` gets an optional filter** (`const std::function<bool(const Item&)>*`, default `nullptr`)
   so `size()`/`get()`/`drawItems` work unchanged. Small upstream edit in `game/inventory.*`, the predicate and
   tab metadata stay in the fork file.

## Iterations

### A: mouse basics, take all, category tabs (next)

- [x] Option `[GAME] invMouse` (default 1), hover, click, wheel, cursor (phase 1, branch `feat/inventory-ui`).
- [x] Fix: clicking an already selected slot does nothing, so with hover enabled a click on the hovered slot is
      a no-op. `selectAt` now reports "there is an item under the cursor" instead of "the selection changed".
      Verified with a synthetic click: apple count went 20 -> 19 after hovering that slot first.
- [x] Cursor shape is set from the inventory state (`Arrow` while mouse mode is active, `Hidden` on close).
      Tempest only applies the shape of the hovered widget, so the arrow appears on the first mouse move after
      opening, not before; there is no public API to force it (`SystemApi::showCursor` and `Window::hwnd` are
      protected/private, `Widget::setCursorShape` applies on hover only).
- [x] "Take all" button in chest and ransack (move every item from the other side to the player); drawn in the
      header row, right-aligned to the other grid, hit-tested like the grids. **Not verified in game yet** (needs
      a chest or a body to loot; there is no console command to open one).
- [x] Tabs: All, Weapons, Armor, Potions, Food, Writings, Magic, Other from `mainFlag` (Weapons = `NF|FF|MUN`,
      Magic = `RUNE|MAGIC`, Other = everything else). Default All, so the first view matches vanilla.
- [x] Tab bar above the player grid, exactly as wide as the grid: one tab per category, **icon-only with
      procedural glyphs** (2x2 grid, sword, shield, flask, apple, open book, star, three dots) drawn from the
      painter's rect/triangle primitives in `InventoryTabs::paintGlyph`, no art assets. Empty categories show the
      glyph dimmed (0.28 alpha) instead of a dark hole; active/hover use `INV_SLOT_HIGHLIGHTED`.
      Chosen 2026-10-23 after item-model icons looked inconsistent (icons changed with the inventory content and
      empty categories looked broken); the user approved a mockup first.
- [x] Switch tabs: click a tab, wheel over the tab bar, Shift+wheel anywhere, `PageUp` / `PageDown`; cycling wraps.
      Verified: `PageDown` and a click on the Food tab both filter the grid (screenshots); wheel/Shift+wheel share
      the same code path but were not exercised separately.
- [x] Selection and scroll remembered per tab and per side; the last tab is kept while the game runs.
- [x] Layout fix that the tab bar needed: the fixed offsets (43 px padding, 134 px grid top, 70 px header row) were
      raw pixels while the cells and fonts scale with `Gothic::interfaceScale`. At 225% DPI the header overlapped
      the top slot row. They are scaled now, which frees the band the tab bar lives in.
- [x] Info box: the first line already shows the item name (`Item::description()` is the visible name in the game
      data, e.g. "Heavy Branch"), followed by the stat rows and the long text. No change needed.
- [x] Equipped/number-slot overlays and the info box keep working in filtered views (they follow the filtered
      iterator; an equipped item outside the active tab shows no overlay, which is expected).
- [ ] Test in game by the user: fullscreen and `-window`, chest, trade, ransack, take all, wheel, different
      `interfaceScale` / `inventoryCellSize`. Trade layout and drag were verified by the agent in the UI-overhaul
      session (`docs/fork/handoff-ui.md`); chest/ransack still need a real container.

### B: drag & drop and direct manipulation

> Done in the UI overhaul (`docs/fork/plan-ui-overhaul.md`, handoff `docs/fork/handoff-ui.md`); the
> checkboxes below are kept for the original wording.

- [x] Interaction state machine: press arms, release without movement acts, movement past the threshold drags,
      holding still repeats (decision 7).
- [x] Drag within the player inventory does nothing for now (order is sorted, not manual).
- [x] Drag player -> chest: move the stack; chest -> player: take the stack; trade: player item onto the trader
      grid sells, trader item onto the player grid buys; ransack: body -> player takes.
- [x] Ghost of the dragged item at the cursor (`InventoryRenderer::drawItem`), target grid highlighted, drop on
      an invalid target cancels.
- [x] Double click takes/uses the stack (`mouseDoubleClickEvent` already exists in Tempest).
- [x] Do not let a drag trigger the take-repeat timer, and do not act twice (on press and on release).
- [x] Cancellation: right click or `Esc` during a drag cancels instead of closing; closing a container mid-drag
      must not crash (item lookup by `clsId`, never a raw `Item*` kept across a move).

### C: tabs everywhere and polish

- [x] One shared active tab for both sides (the filter applies to the player and the chest/trader grid); the
      trade tooltip shows the sell price for player items.
- [x] Finer categories: melee/ranged, keys and trophies via instance-name prefixes (`ItKe_`, `ItAt_`), eleven
      tabs in total. Bows/crossbows and jewelry stay in their coarse tabs for now.
- [x] Tooltip at the cursor with the item name, count and stat rows; the info box stays for keyboard play.
- [ ] Scroll indicators when the grid has more rows than fit; empty-state text per tab.
- [x] Click sounds (`INV_CHANGE` on tab switch and on a drag move).

### Backlog (not planned yet)

- Take all from a body/chest via a key (RSHIFT+A like the plugin) in addition to the button.
- Favorites tab; "recently received" tab; history of items in/out (needs saved state, see decision 6).
- Search by name (needs a text field; `SavNameDialog` in `gamemenu.cpp` shows the manual key handling it takes).
- Drag and drop between the player and the world (drop an item on the ground).
- A list-based layout (the plugin author's newer "list inventory").
- Icons for tabs: **procedural glyphs** are the current look (`InventoryTabs::paintGlyph`). Optional upgrade:
  hand-made or AI-generated art (for example through a local Gemini/OpenAI image tool the user mentioned), which
  needs an asset pipeline (embed at build like the shaders, or ship a `ui/` folder next to the exe and copy it in
  `deploy-play.ps1`) and a license decision (CC0 set or generated art).
- Mouse for dialog choices and the scripted main menu (they still require the keyboard / current selection).
- Custom per-item categories (needs saved state, decision 6).

## Decided

- 2026-10-12: Keyboard keys for switching tabs: `PageUp` / `PageDown` (plus Shift+wheel and clicking a tab).
  Not `Tab`: it is the default inventory key, and `InventoryMenu::keyDownEvent` closes on `KeyCodec::Inventory`.
  Not `Q` / `E`: they are bound to `keyLeft` / `keyRight` and move the selection in the inventory.
- 2026-10-12: Tab labels in English. English is the focus; other languages only if ever needed.
- 2026-10-12: Right click keeps closing the inventory (vanilla).
- 2026-10-23: Iteration order A -> B -> C as listed above (user choice). Defaults unless the user says
  otherwise: coarse tabs first and finer ones in C; drag takes the whole stack; right click still closes;
  tab bar horizontal and compact first; merge to `master` after the user tested phase 1.
- 2026-10-23: Tabs are icon-only, exactly as wide as the grid, with procedural glyphs and dimmed glyphs for empty
  categories (user picked this over text labels and over item-model icons). A generated/hand-made icon set can
  replace the glyphs later; the drawing entry point is `InventoryTabs::paintGlyph`.

## Testability and verification

No unit tests. Per change: build (`scripts/build-windows.ps1`), `scripts/smoke-test.ps1` (benchmark) still passes
with no new warnings, then the user tests in game via `scripts/deploy-play.ps1`.

The agent cannot move the real mouse into the game window with the computer-use tool, but **synthetic input works**
(verified 2026-10-23): `SetForegroundWindow` on the game window, then `keybd_event` for keys, `SetCursorPos` for
moves and `mouse_event` for clicks, all from a helper process. With `scripts/smoke-test.ps1 -Mode idle
-ScreenshotAt` the whole inventory can be driven and screenshotted: `give` items via `-Marvin`, send `Tab` to open
the inventory, move/click slots, click tabs, press `PageDown`, and read the result from the PNGs.

Coordinates follow the layout code (`Gothic::interfaceScale` is 2.25 on the user's 4K/225% setup, so cell = 157):

- `cell = int(70 * interfaceScale)`, `padd = int(43 * interfaceScale)`, `gridTop = int(134 * interfaceScale)`
- player grid: `x = w - padd - 5*cell`, `y = gridTop`; slot (col,row) center = `x + col*cell + cell/2`, same for y
- tab bar: right-aligned to the player grid, `y = int(70*scale) + int(34*scale) + int(2*scale)`,
  `h = gridTop - y - int(2*scale)`; tab widths depend on the font, so for exact rects log them temporarily
  (`Log::e` in `drawAll`) and remove the logging afterwards.

Item lookup for screenshots: `goto waypoint NW_CITY_HABOUR_05` first (moves away from the Xardas dialog, which
blocks the inventory key), then `give` items. A chest or a body is needed to see take-all; there is no console
command for that yet.

Branch: `feat/inventory-ui`, one self-contained commit per concern (`feat(ui): ...`).
