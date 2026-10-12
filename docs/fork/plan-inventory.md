# Plan: modern inventory (mouse and category tabs)

Fork feature. Goal: make the inventory nicer to use with the mouse and split it into category tabs, inspired by
the Union plugin the user plays with in Steam Gothic II. Status per session goes in `docs/fork/handoff-inventory.md`;
this file holds the goal, the design and the phase checklist. Update both when something changes.

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

## How the inventory works today

All of it is OpenGothic C++, not Daedalus script (in the original it is hardcoded in the exe as well):

- `common/ui/inventorymenu.{h,cpp}` (~760 lines): one widget for equip, chest, trade, ransack and lock picking
  (`State`). A `Page` wraps an `Inventory::Iterator` (`T_Inventory`, `T_Trade`, `T_Ransack`); `pagePl` is the player,
  `pageOth` the chest/trader. `pageLocal[2]` holds selection and scroll per side, `page` says which side is active.
- Drawing: `drawAll` -> `drawItems` (grid of `columsCount` x `rowsCount()` slots) -> `drawSlot`. The 3D item models
  are queued with `renderer.drawItem(...)` during the paint pass and drawn later by `InventoryRenderer`, so a slot's
  2D rect and its model must come from the same coordinates.
- Order: `Inventory::sortItems` / `Inventory::less` sort by `[GAME] invCatOrder`
  (default `COMBAT,POTION,FOOD,ARMOR,MAGIC,RUNE,DOCS,OTHER,NONE`), then flags, damage, cost.
- Categories available from game data: `Item::mainFlag()` = `ITM_CAT_*` in `common/game/constants.h`
  (`NF`, `FF`, `MUN`, `ARMOR`, `FOOD`, `DOCS`, `POTION`, `LIGHT`, `RUNE`, `MAGIC`, `NONE`).
- Mouse today (`InventoryMenu::mouseDownEvent`): left click acts on the *selected* slot wherever you click,
  right click closes, wheel moves the selection by a row. No hit test, cursor hidden (`CursorShape::Hidden`).
- `MainWindow::processMouse` warps the cursor to the window center in fullscreen and feeds the delta to
  `tickMouse`, which orbits the camera around the player while the inventory is open (vanilla behavior).

## Design decisions

1. **Keep `InventoryMenu`, add to it in small steps; put new logic in new files.** A separate copy of the widget
   would miss upstream fixes. Edits in `inventorymenu.*` stay small hooks (hit test, filter in `Page::iterator`,
   draw the tab bar); the tab definitions and filter go in a new `common/ui/inventorytabs.{h,cpp}`
   (globbed, re-run CMake configure after adding).
2. **Switchable, on by default in the fork.** `[GAME] invMouse=1` and `[GAME] invTabs=1` in `Gothic.ini`
   (defaults in `Gothic::Gothic`, next to `invCatOrder`). With both 0 the inventory behaves like upstream.
3. **System cursor first** (`CursorShape::Arrow` while the inventory is open in mouse mode). A drawn Gothic-style
   cursor can come later; vanilla data has no cursor texture to reuse.
4. **Hit test from what was drawn.** `drawItems` records `{page, index, rect}` for each visible slot during the
   `Back` pass; mouse events look up that list. No second copy of the layout math.
5. **Tabs from `ITM_CAT_*` first.** Finer categories (keys, maps, trophies, 1H/2H, bows) need item flags or
   instance-name heuristics and come later (backlog).
6. **No savegame changes in phases 1-3.** Favorites, custom categories and history need saved state: when that
   comes, follow the fork rule in `AGENTS.md` (new optional save entry, never bump `Serialize::Version::Current`).

## Phases

### Phase 1: mouse control (equip, chest, trade, ransack)

- [ ] Option `[GAME] invMouse` (default 1).
- [ ] While the inventory is open in mouse mode: cursor visible, `MainWindow::processMouse` does not warp it, the
      camera does not orbit. Restore on close (also on world change, death, dialog start).
- [ ] Hover selects the slot under the cursor; in chest/trade/ransack hovering the other grid switches `page`.
      Only on mouse move, so keyboard navigation keeps working.
- [ ] Left click on a slot = `onItemAction` for that slot (equip/use, take/sell one; holding repeats via
      `takeTimer` as now). Click outside any slot does nothing.
- [ ] Right click: keep "close" (vanilla).
- [ ] Wheel scrolls the grid (`PageLocal::scroll`) instead of moving the selection; the slot under the cursor
      becomes the selection.
- [ ] Check fullscreen and `-window`, and different `interfaceScale` / `inventoryCellSize`.

Things to verify first: whether `InventoryMenu` receives `mouseMoveEvent` / `mouseDragEvent` once it overrides
them (Tempest routes to the child widget under the cursor), and whether its cursor shape wins over the
`MainWindow` one set in `MainWindow::setFullscreen`.

### Phase 2: category tabs in the player inventory (equip mode)

- [ ] Option `[GAME] invTabs` (default 1).
- [ ] Tabs: All, Weapons (`NF|FF|MUN`), Armor, Magic (`RUNE|MAGIC`), Potions, Food, Writings (`DOCS`),
      Other (everything else). Default tab All, so the first view matches vanilla.
- [ ] `Page` filters its iterator by the active tab (`Page::size`/`get` already walk the iterator, so they follow).
- [ ] Tab bar above the player grid, drawn with `INV_BACK.TGA`/`INV_SLOT.TGA` and the Gothic font; the active tab
      highlighted, empty tabs dimmed.
- [ ] Switch tabs: click a tab, Shift+wheel (as the Union plugin), `PageUp` / `PageDown`.
      Cycling wraps.
- [ ] Selection and scroll remembered per tab; the last tab kept while the game runs.
- [ ] Keep the equipped/number-slot overlays and the info box working in filtered views.

### Phase 3: tabs in chest, trade and ransack

- [ ] Tab bar for the other side; one shared active tab for both sides (`bNeedSyncPlayerAndTradeCategories`).
- [ ] Trade info box still shows the sell price for player items.

### Backlog (not planned yet)

- Finer categories (keys, maps, trophies, 1H/2H/bows, light sources).
- Take all from a chest or a body.
- Tooltip at the cursor instead of the bottom info box; compare with the equipped item.
- Favorites tab; "recently received" tab; history of items in/out (needs saved state, see decision 6).
- Search by name.
- Drag and drop between chest/trade and player.
- A list-based layout (the plugin author's newer "list inventory").
- Icons for tabs (own textures, not the plugin's).

## Decided (2026-10-12, with the user)

- Keyboard keys for switching tabs: `PageUp` / `PageDown` (plus Shift+wheel and clicking a tab). Not `Tab`: it
  is the default inventory key, and `InventoryMenu::keyDownEvent` closes on `KeyCodec::Inventory`. Not `Q` / `E`:
  they are bound to `keyLeft` / `keyRight` and move the selection in the inventory.
- Tab labels in English. English is the focus; other languages only if ever needed.
- Right click keeps closing the inventory (vanilla).

## Verification

No unit tests. Per change: build (`scripts/build-windows.ps1`), `scripts/smoke-test.ps1` (benchmark) still passes
with no new warnings, then the user tests in game via `scripts/deploy-play.ps1`. The smoke test cannot open the
inventory or move the mouse yet; a small fork console command that opens the inventory would allow
`-Marvin` + `-ScreenshotAt` screenshots of the layout (add if layout work gets fiddly).

Branch: `feat/inventory-ui`, one self-contained commit per concern (`feat(ui): ...`).
