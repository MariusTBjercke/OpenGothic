# Third-person camera collision in the original Gothic II (research notes)

Behavior of the original engine (Gothic II NotR, `Gothic2.exe` 2.6), derived from static analysis
in Ghidra on 2026-10-08. These notes describe behavior and constants in our own words. Do not paste
decompiled code into the repository; implement from this description.

Source functions (Ghidra names, image base 0x400000):

| Function | Address | Role |
| -------- | ------- | ---- |
| `zCAICamera::AI_Normal` | 0x004a4370 | per-frame ideal position, collision, shoulder fallback |
| `zCPathSearch::AdjustCenterSphere` | 0x004afc70 | pulls the camera in along one ray |
| `zCMovementTracker::GetShoulderCamMat` | 0x004ba380 | fallback "shoulder" camera pose |
| `zCAICamera::CalcAziElevRange` | 0x004bd7f0 | azimuth/elevation/range to world position |
| (unnamed) | 0x004aed00 | validity checks for a candidate camera position (rays, frustum, near wall) |
| (unnamed) | 0x004b87c0 | applies the result: line-of-sight fallbacks, interpolation of position and rotation |

## Collision

- The ideal camera position comes from the camera definition (azimuth, elevation, range) around the
  target point.
- `AdjustCenterSphere` casts a **single ray** from the target to the ideal position extended by the
  camera sphere radius (`CAMSPHERE_DIAMETER / 2`). On a hit, the camera is placed on that ray, one radius
  in front of the hit point. If the hit is closer than one radius, the camera sits on the target.
- No elevation change happens here: the camera only moves closer along its own line.

## Shoulder fallback (distance below 80)

- If the adjusted camera is closer than **80** units to the target, the original switches to a "shoulder"
  camera pose:
  - position: azimuth **0**, elevation **85°**, range **150** (plus a per-camera offset field that is 0
    after construction), in the player's movement frame;
  - orientation: looks at a point **200** units in front of the target (along the player's forward
    direction), raised by **10**. It does **not** look at the player. From 85° and 150 range that is an
    over-the-head view, pitched down about 30° toward the ground ahead.
- The shoulder pose is then checked for visibility (rays from points on the player to the camera). If it
  fails, the camera falls back to a point on the player (first-person-like).
- Both position and rotation move toward the result with interpolation (slerp for rotation), so the
  switch is not instant on screen.

## OpenGothic differences (as of 2026-10-08)

- `Camera::tickThirdPerson` (`common/camera.cpp`) also switches to elevation 80 and range 150 below 80
  units, but then looks **at the target**, i.e. straight down at the player. Combined with mouse-driven
  elevation down to -60°, lowering the camera against a slope flips the view from "looking up" to
  "looking down at the player". That is the bug reported on 2026-10-08.
- OpenGothic's collision uses 9 rays to the near-plane corners with 25 units padding, not one ray with
  the camera sphere radius.
