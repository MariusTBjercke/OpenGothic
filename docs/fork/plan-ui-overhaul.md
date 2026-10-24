# Plan: modernised UI v1 (inventory, dialogs, menus, HUD)

Fork feature, started 2026-10-24. Broadens `docs/fork/plan-inventory.md` from "inventory only" into a
general UI pass: the inventory stays the centerpiece, and every other window that a player touches with
the mouse gets the same treatment. Session state, what is verified and what is not:
`docs/fork/handoff-ui.md`.

## Goal

Make the UI feel like a modern game while keeping the Gothic look:

- every menu can be used with the mouse alone (hover feedback, click to act, wheel to scroll);
- the inventory is pleasant: category tabs on both sides, drag & drop between player and container /
  trader, a tooltip at the cursor instead of a distant info box, a take-all action;
- modal windows dim the world behind them so text and frames stay readable;
- consistent frames, spacing and emphasis across windows (one small drawing kit, not five dialects);
- no new art assets: everything is drawn from the game's own textures (`INV_BACK`, `INV_SLOT`,
  `DLG_CHOICE`, ...) plus painter primitives.

Explicitly not in v1: a list-style inventory, custom per-item categories, favourites/history tabs, search
fields, item icons for tabs, a re-layout of the scripted main menu geometry.

## Design decisions

1. **Mouse support is additive and switchable.** Every window keeps its keyboard path exactly as it was.
   New behaviour sits behind `[GAME]` flags with defaults in `common/gothic.cpp`, so a player can go back
   to vanilla behaviour without a rebuild:
   `invMouse` (exists), `invTabs` (exists), `invTooltip`, `invScrim`, `dlgMouse`, `menuMouse`,
   `docMouse`.
2. **One drawing kit** (`common/ui/uidraw.{h,cpp}`) for panels, scrims, hover states, tooltips, scroll
   bars and text helpers. New file, so upstream edits stay small. Everything is immediate-mode: the
   widgets already paint themselves every frame and hit-test what they drew.
3. **Hit test what was drawn.** Same rule as iteration A of the inventory: each window records
   `{rect, id}` while painting and the mouse handlers look it up. No second copy of the layout math.
   Shared helper `UiDraw::HitList` in the kit; the scripted menus store the rect on their `Item`
   because the slider and choice-box handlers need the geometry too.
4. **Fullscreen is not an afterthought.** In fullscreen the engine warps the cursor to the window center
   on every mouse move (camera look). While a mouse-driven window is open the warp stops, otherwise the
   pointer jumps away and the UI is unusable: `MainWindow::uiOwnsMouse()`.
5. **Accent colour instead of new art.** Selected/hover/active states use one accent (warm gold), drawn
   as a 2 px border/bar and a low-alpha fill. Vanilla textures stay underneath for the Gothic material
   look.
6. **Drag & drop re-resolves items by class id** (`clsId`) whenever it needs the item; the inventory can
   change under a drag. Never keep an `Item*` across a move.
7. **No savegame changes in v1.** All options are ini only.

## Phases

### 1. Inventory (the centerpiece)

- [x] Dim backdrop behind the window (`invScrim`).
- [x] Frames: player panel (header with title + gold, grid) and the container/trader panel (header with
      name, take-all button or item count), both with a dark fill so the world no longer shows through
      the slot grid. The tab bar is flush on top of the panels and exactly as wide as the content.
- [x] Hover: the selected slot follows the cursor, item tooltip at the cursor (`invTooltip`) with the
      stat rows and the sell price in trade; the info box stays for keyboard-only play. The tooltip is
      drawn in the front overlay pass, so the 3D item models do not float over its text.
- [x] Tabs: 11 categories (melee, ranged, armor, potions, food, magic, writings, trophies, keys, other)
      matched from `mainFlag` with an instance-name fallback for `ItAt_`/`ItKe_`; one active tab filters
      **both** sides; tab tooltip with the label; empty tabs dimmed; accent underline on the active tab.
- [x] Drag & drop: press arms, movement past ~8 px drags, release on the other grid moves the stack
      (chest: take/put, trade: buy/sell, ransack: take), release elsewhere cancels, right click / `Esc`
      cancels a drag instead of closing.
- [x] Hold-to-repeat still works for taking one item at a time; a small movement restarts its delay so
      starting a drag never takes an item by accident.
- [x] Hint line at the bottom of the screen: drag / double click / right click, dim font.
- [x] Take-all key (`Shift+A`) in addition to the button.
- [x] Sounds: `INV_CHANGE` when switching tabs and when a drag moves a stack, `INV_OPEN`/`INV_CLOSE` as
      before.
- [ ] Scroll indicator when a grid has more rows than fit (`UiDraw::scrollBar` exists, not wired up:
      the wheel works, the indicator does not).

### 2. Dialogs, documents, scripted menus

- [x] Dialog choices: hover selects and highlights the row under the cursor, click takes it; stable
      panel height measured with the drawing font, dark plate under the character texture.
- [x] Documents (books/letters/notes): dim backdrop, click closes (`docMouse`), hint line.
- [x] Scripted menus (`MENU_MAIN`, options, save/load, status): hover selects (with the browse sound),
      click activates, sliders drag, choice boxes step by clicking their sides (`menuMouse`), gold
      marker on the selected item.
- [x] MainWindow: stop warping the cursor while a mouse-driven window is open; arrow cursor in menus.

### 3. HUD and polish

- [x] Soft plate behind the health/mana bars and behind the focus name.
- [ ] Numeric health/mana values and a damage trail on the bars.
- [ ] Item icons for the tabs (needs an asset pipeline and a licence decision).

## Open questions for the user

- Does the drag need to move only one item (modifier key) in addition to the whole stack? (v1 moves the
  whole stack, double click moves the stack too.)
- Should the tab strip also appear above the container/trader grid, or is one shared strip above the
  panels fine? (v1: one shared strip, capped width and centred.)
- Is the hint line at the bottom wanted permanently, or should it fade after the first few seconds?

## Verification

No unit tests. Per phase: build (`scripts/build-windows.ps1 -Target Gothic2Notr`), benchmark smoke test
(`scripts/smoke-test.ps1`), then `build/ui-run.ps1` (fork-local harness) with synthetic input and
screenshots. Recipe and gotchas in `docs/fork/handoff-ui.md`.

Keyboard regression is the main risk: every mouse handler must let the keyboard path through untouched.
Run the inventory with `-Ini "[GAME]`r`ninvMouse=0`r`ninvTabs=0"` after touching it.
