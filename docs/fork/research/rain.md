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

`ZSTARTRAIN [STRENGTH]` ("starts outdoor rain effect") forces the effect via `SetRainFXWeight`.

## Save data (ZenKit `zenkit::SkyController`)

`master_time, rain_weight, rain_start, rain_stop, rain_sct_timer, rain_snd_vol, day_ctr`,
plus G1-only `fade_scale, render_lightning, is_raining, rain_ctr`.

## Open questions

- What the 0.25 volume condition checks exactly (virtual at vtable+0x54, flag at +0x698).
- Particle parameters (count, area around camera, speed, splash ray-tests) in `zCOutdoorRainFX`.
- How much `RenderRainCloudLayer` darkens the sky, and fog changes during rain.
- `zRainWindScale` (`[SKY_OUTDOOR]` ini) effect on drop direction.
