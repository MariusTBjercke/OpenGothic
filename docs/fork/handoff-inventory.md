# Handoff: modern inventory (mouse and category tabs)

Status per session, newest first. Read `AGENTS.md`, then `docs/fork/plan-inventory.md` (goal, design, phase
checklist). Keep the checklist in the plan up to date; this file says where things stand and what is next.

## Current status

- Nothing implemented yet. Plan written on 2026-10-12.
- Branch: `feat/inventory-ui` (from `master`).

## Next step

Phase 1 (mouse control): first verify the two Tempest questions in the plan (does `InventoryMenu` get
`mouseMoveEvent`, does its cursor shape win over `MainWindow`'s), then add `[GAME] invMouse`, stop cursor warping
and camera orbit while the inventory is open, and add hover/click on slots via the rects recorded in `drawItems`.

## Session log

### 2026-10-12: plan

- Looked at `common/ui/inventorymenu.cpp`, `MainWindow` mouse handling and `Inventory` sorting; wrote the plan.
- Reference plugin: Union Advanced Inventory; the user's settings for it are in `[ADV_INVENTORY]` in
  `D:\GothicDev\Gothic II OpenGothic\system\Gothic.ini`.
