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

- Target volume = rain weight, multiplied by **0.25** when the camera is indoors or under water.
  "Indoors" is the camera location hint that `zCBspTree::Render` (0x00530080) passes to the sky controller every
  frame (`SetCameraLocationHint`, stored at +0x698): 0 = outside all sectors, 1 = inside a sector with no outdoor
  area visible, 2 = inside a sector with outdoor area visible through a portal. Under water is
  `GetUnderwaterFX` (vtable+0x54).
- Drops are created and updated in every case; only hint 1 skips drawing them. Looking out of a cave or a house
  (hint 2) shows the rain outside.
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

## Rain cloud layer and wind

- `zCSkyControler_Outdoor::RenderSky` draws the two normal sky layers, then, while it rains,
  `zCSkyLayer::RenderRainCloudLayer` (0x005e5d00): a dome with `SKYRAINCLOUDS.TGA`, alpha blended over the sky.
  Its alpha is `min(255, round(weight*255)*2)`, so the sky is fully covered from rain weight 0.5. The color comes
  from three floats of the sky controller (+0xa4..+0xac, probably the current upper dome color). The texture
  scrolls slowly (mapping direction about 1.3e-5 / 3e-6 per ms). Not drawn under water.
- Wind: `zCOutdoorRainFX::CreateParticles` (0x005e1c70) takes the global wind vector of the sky controller
  (`GetGlobalWindVec`, enabled by `[ENGINE] zWindEnabled`), scales its x/z by `zRainWindScale` (default 0.003),
  sets y = -1, normalizes and uses that as the fall direction. `CalcGlobalWind` (0x005ea210) varies the wind with
  `[ENGINE] zWindStrength` 70 +- 40, `zWindCycleTime` 4 +- 2 and `zWindAngleVelo` 0.9 +- 0.8; at the average
  strength the tilt is about 12 degrees.
- Splashes: `zCOutdoorRainFX::RenderParticles` (0x005e25d0) draws each splash as a quad of `(1 - t) * 25` cm
  that shrinks over its life.

## Particle collision of script effects (`flyCollDet`)

Not rain-specific, but relevant when copying the drop behavior to other effects. `zCParticleFX::UpdateParticle`
(0x005af500) traces a ray along each particle's step for emitters with `flyCollDet_B >= 1`, on roughly every other
update (a global counter gates it). On a hit, mode 1 and 2 reflect the velocity on the hit normal (with different
damping), mode 3 sets the velocity to zero and any higher value ends the particle. If the emitter has a mark
texture (`mrkTexture_S`), a `zCQuadMark` is placed at the hit. OpenGothic parses `flyCollDet` but ignores it.

## Save data (ZenKit `zenkit::SkyController`)

`master_time, rain_weight, rain_start, rain_stop, rain_sct_timer, rain_snd_vol, day_ctr`,
plus G1-only `fade_scale, render_lightning, is_raining, rain_ctr`.

## Open questions

- Drop speed and lifetime in `zCOutdoorRainFX` (the fall direction is scaled by 1.5 after normalizing; units not
  checked).
- Whether the original changes fog during rain.

## Implementation status in this fork

Implemented in `common/world/weather.{h,cpp}` (class `Weather`, owned by `World`):

- Daily rain window, weight ramp, `rainCtr`/lightning flag, `ZSTARTRAIN [pos]` (`common/marvin.cpp`),
  `Wld_IsRaining` (`common/game/gamescript.cpp`), `[GAME] skyEffects=0` disables rain.
- State is saved per world in the optional save entry `worlds/<zen>/weather`, so the save format version is
  unchanged and older saves load with the default window.
- `rain_01.wav` loop that follows the listener with the original volume slope, x0.25 indoors and under water
  (`Camera::isInWater`). Indoors (our stand-in for location hints 1 and 2): a roof above (upward ray) and either a
  portal room (`World::roomAt`) or a roof within 15 m plus walls within 8 m on three of four sides. The harbour
  huts, for example, are no portal rooms. Checked every 200 ms.
- Drops: a world-space box particle emitter (`SKYRAIN.TGA`, velocity aligned, additive) 1250 in front of the camera,
  density scaled with the weight. It stays on indoors, so rain outside is visible from caves and houses.
- Drops stop at roofs and the ground: each new drop gets one `DynamicWorld::ray` from above the top of the world
  mesh (at least 50 m above the drop) down to where its lifetime would end (`ParticleFx::spawnHook`, called from `PfxBucket::init`). The lifetime ends at the
  first hit; drops whose ray hits above their spawn point (born under a roof) are not spawned. Measured with a
  temporary counter: from the benchmark camera above the city about 16 % of drops are shortened and almost none
  skipped; inside Xardas' tower 22 % are skipped and 15 % shortened; at two waypoints inside the CITYFOREST cave
  all drops are skipped. Benchmark FPS unchanged (76). The ray starts above the world mesh because a start point
  only 50 m above the drop can lie inside the rock above a deep cave; back faces are filtered, so such a ray would
  miss the mountain surface and let drops into the cave. That case was reasoned about, not observed.
- Splashes: `clipDrop` puts each landing (position, world tick when it lands) into a queue; a second emitter
  (`SKYRAINSPLASH.TGA`, additive billboards 6 to 12 cm, alpha 130, 200 ms) takes the due landings in its
  `spawnHook`, so a splash appears where and when a drop disappears: on ground, roofs and barrels, never indoors.
  About 275 splashes/s at full rain at the harbour. Drops that end under a water surface end and splash on it.
- Wind: the drop direction is tilted by a slowly varying wind in the original's strength range times
  `zRainWindScale` (read from `[SKY_OUTDOOR]`, default 0.003). Not linked to the vegetation wind in the renderer.
- Overcast: `SceneDesc.rain` (set via `WorldView::setRainWeight`) drives the sky shaders with the original's
  coverage `min(1, 2*weight)`: a grey layer over the sky in `clouds.glsl` (lit by the average horizon radiance,
  varied by the day cloud texture), more haze and less blue in the atmosphere and fog LUTs (`rainClouds` in
  `sky_common.glsl`), sun and moon discs hidden (`sun.frag`), Mie phase isotropic under the overcast (no sun halo). Stars
  disappear at night. The overcast is shaped by `SKYRAINCLOUDS.TGA` in two layers of different scale and drift,
  normalized to the texture's average so only the structure changes, not the brightness.
- Lighting: `shader/lighting/sky_exposure.comp` dims direct sun (-80%) and ambient (-40%) by the rain weight
  after exposure is computed, the same way the cloud factor is applied, so the scene gets darker instead of the
  auto exposure brightening it.

- Wet surfaces (fork only, the original has none): `Weather` keeps a wetness value on the game clock (wet after 5
  game minutes of full rain, dry 45 game minutes after the rain fully stopped; time jumps such as sleeping are
  stepped minute by minute with the rain window), saved in the optional entry `worlds/<zen>/wetness`, and a rain map, 128 x 128 cells of 1 m around the camera with the height of the topmost static
  surface. Each cell is traced once (static world); cells that enter at the edge are traced first, up to 512 rays
  per tick. The shader averages exposure over nine points in a 1.2 m circle for soft edges. `shader/lighting/rain_wet.frag` darkens the albedo of surfaces that are that
  topmost surface (right after the G-buffer, multiplied in) and adds a faint Fresnel sky reflection on up-facing
  ones after the lights (`-DSHEEN`), only under the overcast and not on foliage. Floors under roofs stay dry.

Not done yet (list in `docs/fork/handoff-rain.md`): lightning.

Verify with:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/smoke-test.ps1 -Marvin "set time 13 0;zstartrain 0.5;weather" -ScreenshotAt "12,20"
```

The `weather` console command prints the window as clock times, e.g. `rain 12:00-14:12, weight 1.00, raining`
(logged as `marvin output: ...` when run through `-marvin`). Save/load of the weather entry: see the two-run
`save game` / `-LoadSave` recipe in `AGENTS.md` (smoke test section).

Avoid `set time 12 0` when testing: noon is where the sky day wraps, so a forced window gets clipped to zero.
