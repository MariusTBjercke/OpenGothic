# Rain in the original Gothic II (research notes)

Behavior of the original engine (Gothic II NotR, `Gothic2.exe` 2.6), derived from static analysis
in Ghidra on 2026-10-07. These notes describe behavior and constants in our own words. Do not paste
decompiled code into the repository; implement from this description.

Source functions (Ghidra names, image base 0x400000):

| Function | Address | Role |
| -------- | ------- | ---- |
| `zCSkyControler_Outdoor::RenderSkyPre` | 0x005ea850 | rolls the daily rain window |
| `zCSkyControler_Outdoor::ProcessRainFX` | 0x005eaf30 | rain weight, sound volume, particles |
| `zCSkyControler_Outdoor::SetRainFXWeight` | 0x005eb230 | console/script override (`ZSTARTRAIN`) |
| `zCOutdoorRainFX` (ctor) | 0x005e10a0 | particle system, uses `SKYRAIN.TGA`, `SKYRAINSPLASH.TGA`, `rain_01.wav` |
| `RenderRainCloudLayer` | 0x005e5d00 | darker cloud layer (`SKYRAINCLOUDS.TGA`) |
| `Wld_IsRaining` handler | 0x006fb480 | script external |
| `oCWorldTimer::GetSkyTime` | 0x00781240 | world time to sky time |

## Sky time

`skyTime = frac(worldTimeOfDay / 6e6 + 0.5)`, so sky time 0.0 is **12:00 noon**, 0.5 is midnight,
and the day "wraps" at noon. One game day = 1.0 sky time; 1 game hour = 1/24 = 0.0417.

## Daily rain window

When sky time wraps (previous - current > 0.95 and current < 0.02, i.e. at noon):

- `rainStart = rand01()`, clamped to at most 0.958
- `rainStop  = rainStart + 0.042 + rand01() * 0.06`, clamped to at most 1.0
- So it rains **once per game day**, starting at a uniformly random time between 12:00 and
  about 11:00 the next morning, for **about 1.0 to 2.45 game hours**.
- `rainCtr` counts rain periods. A lightning/thunder flag is set when `rainCtr >= 4` and
  `rand01() > 0.6` (stored in the save; G2 seems not to render it, ZenKit marks it "G1 only").

`rand01()` is MSVC `rand() / 32767`.

The roll only happens on a continuous wrap. Jumping over noon (e.g. sleeping from 8:00 to 14:00) does not
roll, and the previous window simply repeats at the same sky time the next day.

Defaults for a new sky controller (constructor at 0x005e6220): `rainStart = 0.187` (bits `0x3e3f7cee`),
`rainStop = 0.25`, i.e. about 16:30 to 18:00 on the first day if no roll happened before.

## Rain weight (0..1)

With `t = (skyTime - rainStart) / (rainStop - rainStart)` inside the window, otherwise 0:

- `t < 0.2`: ramp up, `weight = t * 5`
- `0.2 <= t < 0.8`: full, `weight = 1`
- `t >= 0.8`: ramp down, `weight = (1 - t) * 5`

Rain is disabled entirely if `skyEffects` is off in Gothic.ini (`[GAME] skyEffects`) or the
controller is flagged as not outdoor.

`isRaining` becomes 1 when weight > 0 and the particle system is active; it resets to 0 when
the weight drops to 0.

## Sound volume

- Target volume = rain weight, multiplied by **0.25** when the camera is in a sheltered state
  (an indoor/underwater check via a virtual method; exact meaning to be confirmed).
- Actual volume moves toward the target by `0.0005 * frameTimeMs` per frame (0 to 1 in 2 s),
  clamped to 0..1.
- Sound file: `rain_01.wav`.

## Script external

`Wld_IsRaining()` returns true when the outdoor sky controller's **rain weight > 0.3**.
Used by `B_Say_GuildGreetings` (NPCs comment on the weather).

## Console

`ZSTARTRAIN [STRENGTH]` ("starts outdoor rain effect") calls `SetRainFXWeight(strength, 0.1)`.
Despite the name, `strength` is the position inside a new window of length `d = 0.1` sky days (2.4 h):

- `rainStart = now - strength * d`, `rainStop = now + (1 - strength) * d`, both clamped to 0..1
- `strength = 0` (no argument) starts a fresh window, rain fades in over the next ~29 game minutes
- `strength = 0.5` puts you in the middle, i.e. full rain right away
- `strength >= 1` ends the window immediately
- also re-rolls the lightning flag (70% chance of none)

## Particles (`zCOutdoorRainFX`)

- Pool of 1024 drops; active drop count = `round(weight * 1024)`.
- Spawn area: box of +-1750 x +-800 y +-1750 (cm) around a point 1250 in front of the camera; drops start
  500 to 1500 above that point. If the camera moved more than 3750 since the last frame, all drops respawn.
- Each new drop ray-traces down to find where it lands (splash position); no rain under roofs that way.
- Fall direction is straight down, tilted by the global wind scaled with `zRainWindScale`.
- Textures: `SKYRAIN.TGA` (drops), `SKYRAINSPLASH.TGA` (splashes); `rain_01.wav` loop.

## Save data (ZenKit `zenkit::SkyController`)

`master_time, rain_weight, rain_start, rain_stop, rain_sct_timer, rain_snd_vol, day_ctr`,
plus G1-only `fade_scale, render_lightning, is_raining, rain_ctr`.

## Open questions

- What the 0.25 volume condition checks exactly (virtual at vtable+0x54, flag at +0x698).
- Particle parameters (count, area around camera, speed, splash ray-tests) in `zCOutdoorRainFX`.
- How much `RenderRainCloudLayer` darkens the sky, and fog changes during rain.
- `zRainWindScale` (`[SKY_OUTDOOR]` ini) effect on drop direction.

## Implementation status in this fork

Implemented in `common/world/weather.{h,cpp}` (class `Weather`, owned by `World`):

- Daily rain window, weight ramp, `rainCtr`/lightning flag, `ZSTARTRAIN [pos]` (`common/marvin.cpp`),
  `Wld_IsRaining` (`common/game/gamescript.cpp`), `[GAME] skyEffects=0` disables rain.
- State is saved per world in the optional save entry `worlds/<zen>/weather`, so the save format version is
  unchanged and older saves load with the default window.
- `rain_01.wav` loop that follows the listener with the original volume slope, x0.25 inside portal rooms
  (`World::roomAt` is our stand-in for the unknown "sheltered" check).
- Drops: a world-space box particle emitter (`SKYRAIN.TGA`, velocity aligned) 1250 in front of the camera,
  density scaled with the weight, disabled inside portal rooms. No ray-tested splashes yet.
- Lighting: `shader/lighting/sky_exposure.comp` dims direct sun (-80%) and ambient (-40%) by the rain weight
  after exposure is computed, the same way the cloud factor is applied, so the scene gets darker instead of the
  auto exposure brightening it.

Not done yet: grey/overcast sky and rain cloud layer (`SKYRAINCLOUDS.TGA`), splashes, wind tilt
(`zRainWindScale`), occlusion under roofs outside portal rooms, lightning.

Verify with:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/smoke-test.ps1 -Marvin "set time 13 0;zstartrain 0.5" -ScreenshotAt "12,20"
```

Avoid `set time 12 0` when testing: noon is where the sky day wraps, so a forced window gets clipped to zero.
