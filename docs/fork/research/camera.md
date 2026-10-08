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

## Position easing (how fast the camera follows)

`zCMovementTracker` (0x004b87c0, and `Update` at 0x004b73f0) moves the camera from its current position toward
the ideal position in two steps per frame. `dt` is the frame time in seconds (`ztimer` frame time divided by the
motion factor). `frac` below is the distance from current to ideal position divided by 500, clamped to 0..1.

1. **Lerp** toward the ideal position with factor `rate * dt`, clamped to 0..1. With line of sight to the player,
   outside first person and without the look-around key held:
   - most modes: `rate = 2 * veloTrans` (veloTrans below 10 is raised toward 10 with `frac`);
   - `CAMMODMELEE` (and one more mode): `rate = 2 * (veloTrans + 20 * frac)`.
   - First person or look-around key held: `rate = veloTrans * mouseSensX * (0.3 + 0.7 * frac)`, where the
     sensitivity is `mouseSensitivity * 0.5 + 0.3` from `Gothic.ini` (`[GAME]`).
   - Without line of sight: `rate = 3 * (veloTrans + (min(veloTrans + 10, 30) - veloTrans) * frac)`.
2. **Weighted average** of the old position and the step-1 result: `new = (old + step1 * k) / (1 + k)` with
   `k = dt / 0.05 * (4e + 1) / (e + 1)`, `e = |step1 - old|^2 * 1e-5` (0.05 comes from an int field = 2,
   times 0.025). Small moves get `k = 20 * dt`, large ones up to four times that.

Camera definitions: `CAMMODNORMAL` uses the prototype's `veloTrans = 40`, `veloRot = 2`.

At 60 fps and veloTrans 40, step 1 is a full snap and step 2 moves 25 % of the way for small offsets and up to
57 % for large ones (about 17 to 50 per second). The result depends on frame rate: at 144 fps it drops to
about 10 per second for small offsets.

The look-at target is the player position without smoothing (`UpdatePlayerPos` stores it directly).

Mouse turning of the player (`oCAIHuman::PC_Turnings`, contains 0x0069a9ad) scales mouse X by
`zMouseRotationScale` (default 2.0) and turns the model directly through the animation controller.

## Look key (`keyLook`, default R and Numpad 0)

`oCAIHuman::ChangeCamModeBySituation` (0x0069cd60) checks the look key (logical key 0x10) each frame:

- **Player standing** (`IsStanding`): stops turn animations, switches to `CAMMODLOOK` and sets the head look
  target from the movement keys: left/right keys turn the head fully to that side, up/down keys tilt it, no key
  looks straight ahead. The mouse does not drive the head.
- **Player moving**: switches to `CAMMODLOOKBACK` (a fixed camera looking back at the player).

`zCAICamera::CheckKeys` (writes the look-around flag at 0x004a4f8e) moves the camera with
the mouse while the key is held, in a set of modes that includes `CAMMODLOOK` and `CAMMODLOOKBACK`: mouse Y
changes elevation (clamped -60..85), mouse X changes azimuth (clamped -80..90 for these modes, ±180 for
inventory, death and mob cameras). The look-around flag also makes the position easing use mouse sensitivity
(see above).

`CAMMODLOOK` (CamInst.d): range 1.5..6.5 (best 3.0), elevation -55..80 (best 30), azimuth -90..90,
veloTrans 35.

## OpenGothic differences (as of 2026-10-08)

- The look key only switched to `CAMMODLOOKBACK`, also while standing, and the mouse kept turning the player.
  Now standing gives `CAMMODLOOK`: the mouse orbits the camera within its azimuth limits, movement keys turn
  the head (±60° sideways, ±20° up/down), and walking is blocked while the key is held. The camera keeps its
  current distance instead of switching to `CAMMODLOOK`'s range limits.

- `Camera::followTrans` used a single lerp at `0.25 * veloTrans` per second (10 per second for the normal
  camera, about 100 ms lag), so turning with the mouse felt delayed. Changed to the two-step vanilla factor,
  evaluated at 60 fps and applied as a frame-rate independent rate.

- `Camera::tickThirdPerson` (`common/camera.cpp`) also switches to elevation 80 and range 150 below 80
  units, but then looks **at the target**, i.e. straight down at the player. Combined with mouse-driven
  elevation down to -60°, lowering the camera against a slope flips the view from "looking up" to
  "looking down at the player". That is the bug reported on 2026-10-08.
- OpenGothic's collision uses 9 rays to the near-plane corners with 25 units padding, not one ray with
  the camera sphere radius.
