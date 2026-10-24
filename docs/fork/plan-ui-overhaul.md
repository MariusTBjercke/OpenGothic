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
   `docMouse`, `hudModern`.
2. **One drawing kit** (`common/ui/uidraw.{h,cpp}`) for panels, scrims, hover states, tooltips, scroll
   bars and shadowed text. New file, so upstream edits stay small. Everything is immediate-mode: the
   widgets already paint themselves every frame and hit-test what they drew.
3. **Hit test what was drawn.** Same rule as iteration A of the inventory: each window records
   `{rect, id}` while painting and the mouse handlers look it up. No second copy of the layout math.
   Shared helper `UiHitList` in the kit.
4. **Fullscreen is not an afterthought.** In fullscreen the engine warps the cursor to the window center
   on every mouse move (camera look). While a mouse-driven window is open the warp must stop, otherwise
   the pointer jumps away and the UI is unusable. `MainWindow::processMouse` gets a single gate rather
   than each widget fighting for the cursor.
5. **Accent colour instead of new art.** Selected/hover/active states use one accent (warm gold), drawn
   as a 2 px border and a low-alpha fill. Vanilla textures stay underneath for the Gothic material look.
6. **Drag & drop re-resolves items by class id** (`clsId`) whenever it needs the item; the inventory can
   change under a drag. Never keep an `Item*` across a move.
7. **No savegame changes in v1.** All options are ini only.

## Phases

### 1. Inventory (the centerpiece)

- [ ] Dim backdrop behind the window (`invScrim`).
- [ ] Frames: player panel (header + tabs + grid + hint line) and the container/trader panel (header
      with name + take-all button, grid). Panels use `INV_BACK` + a dark fill so the world no longer
      shows through the slot grid.
- [ ] Header: title left (player name / chest / trader), gold right on the player side.
- [ ] Hover: slot ring, item name tooltip at the cursor (`invTooltip`) with the stat rows that the info
      box shows; the info box stays for keyboard-only play.
- [ ] Tabs: 10 categories (melee, ranged, armor, potions, food, magic, writings, trophies, keys, other)
      matched from `mainFlag`/`itemFlag` with an instance-name fallback; one active tab filters **both**
      sides; tab tooltip with the label; empty tabs dimmed; accent on the active tab.
- [ ] Drag & drop: press arms, move past ~8 px drags, release on the other grid moves the stack
      (chest: take/put, trade: buy/sell, ransack: take), release elsewhere cancels, right click / `Esc`
      cancels a drag instead of closing, double click takes the stack.
- [ ] Hover-and-hold repeat still works for taking one item at a time.
- [ ] Scroll indicator when a grid has more rows than fit; wheel scrolls the grid under the cursor.
- [ ] Hint line under the player grid: drag / double click / right click, dim font.
- [ ] Take-all key (`Shift+A`) in addition to the button.
- [ ] Sounds: `INV_CHANGE` when switching tabs, `INV_OPEN`/`INV_CLOSE` as today.

### 2. Dialogs, documents, scripted menus

- [ ] Dialog choices: hover highlights and selects the row under the cursor, click picks it, wheel
      scrolls; the choice panel gets the same frame treatment (`dlgMouse`).
- [ ] Documents (books/letters/notes): dim backdrop, click closes (`docMouse`), so a note can be
      dismissed with either mouse button.
- [ ] Scripted menus (`MENU_MAIN`, options, save/load, status): hover selects, click executes, wheel
      moves the selection, sliders can be dragged to a value (`menuMouse`). The item hit-test rects are
      recorded while painting.
- [ ] MainWindow: stop warping the cursor while a mouse-driven window is open; show the arrow cursor.

### 3. HUD and polish

- [ ] Health/mana bars: soft shadow under the frame, smoothed value decay, optional numeric values
      (`hudModern`).
- [ ] Focus label: a subtle plate behind the focused object's name so it stays readable against bright
      backgrounds.
- [ ] Shared hover/selection feedback in all windows.

## Verification

No unit tests. Per phase: build (`scripts/build-windows.ps1 -Target Gothic2Notr`), benchmark smoke test
(`scripts/smoke-test.ps1`), then `idle` runs with synthetic input and screenshots (see
`docs/fork/handoff-inventory.md` for the recipe). Every phase ends with the user-facing deploy
(`scripts/deploy-play.ps1`) only at the end of the session, so intermediate builds stay in `build/`.

Keyboard regression is the main risk: every mouse handler must let the keyboard path through untouched.
