#include "weather.h"

#include <zenkit/addon/daedalus.hh>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <queue>

#include "graphics/pfx/particlefx.h"
#include "game/gamesession.h"
#include "game/serialize.h"
#include "world/world.h"
#include "gothic.h"

// all rain-drop counts/areas are in Gothic units (cm) and game milliseconds
static const float MaxDrops      = 1024.f; // pool size of zCOutdoorRainFX
static const float DropLifetime  = 1500.f;
static const float MaxDropsPps   = MaxDrops*1000.f/DropLifetime;
static const float AheadOfCamera = 1250.f;

// splashes: one per drop that lands, when it lands. The splash emitter only asks often enough for landed drops
static const float SplashPps     = 2000.f;
static const float SplashLife    = 200.f;
static const size_t MaxPendingHits = 4096;

// the shelter ray of a new drop starts above the world mesh (at least this far above the drop), so drops born
// under a roof or in a cave deep inside a mountain are skipped too
static const float ShelterRayUp  = 5000.f;

static float rand01() {
  return float(std::rand())/float(RAND_MAX);
  }

// where and when (world tick, ms) a drop will land; filled by clipDrop, used by placeSplash
struct DropHit {
  Tempest::Vec3 pos;
  uint64_t      due = 0;
  bool operator < (const DropHit& other) const { return due>other.due; } // min-heap on 'due'
  };
static std::priority_queue<DropHit> pendingHits;

static void clearPendingHits() {
  pendingHits = std::priority_queue<DropHit>();
  }

// ParticleFx::spawnHook for drops: end each drop where it hits the static world (roofs, ground, trees)
static uint16_t clipDrop(Tempest::Vec3& pos, const Tempest::Vec3& dir, uint16_t life) {
  auto  world = Gothic::inst().world();
  auto  phys  = world!=nullptr ? world->physic() : nullptr;
  auto  view  = world!=nullptr ? world->view()   : nullptr;
  const float speed = dir.length();
  if(phys==nullptr || view==nullptr || speed<=0.f)
    return life;

  const float fall = speed*float(life);
  const auto  n    = dir/speed;
  float       back = ShelterRayUp;
  if(n.y<0.f)
    back = std::max(back, (view->bbox().second.y + 100.f - pos.y)/(-n.y));

  const auto  hit  = phys->ray(pos-n*back, pos+dir*float(life));
  if(!hit.hasCol)
    return life;

  const float dist = hit.hitFraction*(back+fall) - back;
  if(dist<=0.f)
    return 0; // under a roof already

  const auto ret = uint16_t(std::max(1.f, dist/speed));
  if(pendingHits.size()<MaxPendingHits)
    pendingHits.push(DropHit{pos + dir*float(ret), world->tickCount() + ret});
  return ret;
  }

// ParticleFx::spawnHook for splashes: put the splash where a drop lands right now. Without a landed drop the
// particle is skipped; landings that waited too long (no free splash slot) are dropped
static uint16_t placeSplash(Tempest::Vec3& pos, const Tempest::Vec3&, uint16_t life) {
  auto world = Gothic::inst().world();
  if(world==nullptr)
    return 0;

  const uint64_t now = world->tickCount();
  while(!pendingHits.empty() && pendingHits.top().due+100<now)
    pendingHits.pop();
  if(pendingHits.empty() || pendingHits.top().due>now)
    return 0;

  pos = pendingHits.top().pos + Tempest::Vec3(0,3,0);
  pendingHits.pop();
  return life;
  }

Weather::Weather(World& owner)
  :owner(owner) {
  auto fx = Gothic::settingsGetS("GAME","skyEffects");
  enabled = (fx.empty() || fx!="0");

  auto ws = Gothic::settingsGetS("SKY_OUTDOOR","zRainWindScale");
  if(!ws.empty())
    windScale = Gothic::settingsGetF("SKY_OUTDOOR","zRainWindScale");

  // landings belong to the previous world's clock
  clearPendingHits();
  }

Weather::~Weather() {
  }

float Weather::skyTime(gtime t) {
  const double dayMs = 24.0*60.0*60.0*1000.0;
  double       v     = double(t.timeInDay().toInt())/dayMs + 0.5;
  if(v>=1.0)
    v -= 1.0;
  return float(v);
  }

float Weather::rainWeightAt(float skyTime, float rainStart, float rainStop) {
  if(rainStop<=rainStart || skyTime<rainStart || skyTime>=rainStop)
    return 0.f;
  const float t = (skyTime-rainStart)/(rainStop-rainStart);
  if(t<0.2f)
    return t*5.f;
  if(t<0.8f)
    return 1.f;
  return (1.f-t)*5.f;
  }

void Weather::tick(uint64_t dt) {
  if(!enabled) {
    weight = 0;
    tickFx(dt);
    return;
    }

  const float now = skyTime(owner.time());
  // the original rolls only on a continuous wrap at noon; time jumps over noon keep the old window
  if(prevSkyTime>=0.f && prevSkyTime-now>0.95f && now<0.02f)
    rollRain();
  prevSkyTime = now;

  weight = rainWeightAt(now,rainStart,rainStop);
  if(weight>0.f && !rainActive) {
    rainCtr++;
    rainActive = true;
    }
  else if(weight<=0.f) {
    rainActive = false;
    }

  tickFx(dt);
  }

void Weather::startRain(float position, float duration) {
  position = std::clamp(position,0.f,1.f);
  duration = std::clamp(duration,0.f,1.f);

  const float now = skyTime(owner.time());
  rainStart   = std::clamp(now - position*duration,      0.f, 1.f);
  rainStop    = std::clamp(now + (1.f-position)*duration, 0.f, 1.f);
  lightning   = rand01()>0.7f;
  prevSkyTime = now;
  }

void Weather::rollRain() {
  rainStart = std::min(rand01(), 0.958f);
  rainStop  = std::min(rainStart + 0.042f + rand01()*0.06f, 1.f);
  lightning = (rainCtr>=4 && rand01()>0.6f);
  }

bool Weather::isSheltered(const Tempest::Vec3& pos) const {
  // portal rooms (houses, caves with portals) count as indoor, like the camera location hint of the original.
  // roomAt also reports a room when the camera is above a roof next to one, so a roof above is required too.
  // Many houses are not portal rooms: there a low roof plus walls on at least three sides counts as well
  auto phys = owner.physic();
  if(phys==nullptr)
    return !owner.roomAt(pos).empty();

  const auto roof = phys->ray(pos, pos+Tempest::Vec3(0,5000,0));
  if(!roof.hasCol)
    return false;
  if(!owner.roomAt(pos).empty())
    return true;
  if(roof.v.y-pos.y>1500.f)
    return false;

  static const Tempest::Vec3 side[4] = {{800,0,0}, {-800,0,0}, {0,0,800}, {0,0,-800}};
  int walls = 0;
  for(auto& s:side)
    if(phys->ray(pos, pos+s).hasCol)
      ++walls;
  return walls>=3;
  }

std::string Weather::statusLine() const {
  if(!enabled)
    return "rain disabled ([GAME] skyEffects=0)";

  // sky time 0 is noon
  auto clock = [](float sky) {
    const int minutes = int(std::lround(double(sky)*24.0*60.0)) + 12*60;
    return minutes%(24*60);
    };
  const int t0 = clock(rainStart);
  const int t1 = clock(rainStop);

  const bool indoor = isSheltered(owner.gameSession().camera().listenerPosition().pos);

  char buf[128] = {};
  std::snprintf(buf,sizeof(buf),"rain %02d:%02d-%02d:%02d, weight %.2f%s%s",
                t0/60, t0%60, t1/60, t1%60, double(weight),
                isRaining() ? ", raining" : "", indoor ? ", sheltered" : "");
  return buf;
  }

void Weather::tickFx(uint64_t dt) {
  auto&      camera = owner.gameSession().camera();
  const auto lp     = camera.listenerPosition();
  if(shelterTimer<=dt) {
    // roomAt scans all BSP sectors; a few checks per second are enough
    sheltered    = isSheltered(lp.pos);
    shelterTimer = 200;
    } else {
    shelterTimer -= dt;
    }

  // overcast sky, haze and hidden sun in the sky/fog shaders (scene.rain)
  if(auto view = owner.view())
    view->setRainWeight(weight);
  tickWetness(lp.pos, dt);

  // sound: follows the rain weight with a fixed slope, quieter indoors and under water (as the original)
  const float target = weight*(sheltered || camera.isInWater() ? 0.25f : 1.f);
  const float step   = 0.0005f*float(dt);
  if(sndVolume<target)
    sndVolume = std::min(target, sndVolume+step); else
    sndVolume = std::max(target, sndVolume-step);

  // own sound source that follows the listener; WorldSound range checks don't fit a non-positional loop
  if(sndVolume>0.f) {
    if(sound.isEmpty()) {
      if(auto sfx = Gothic::inst().loadSoundWavFx("RAIN_01.WAV")) {
        bool loop = false;
        sound   = owner.gameSession().loadSound(*sfx,loop);
        sndBase = sound.volume();
        }
      }
    sound.setPosition(lp.pos);
    sound.setVolume(sndBase*sndVolume);
    if(sound.isFinished())
      sound.play();
    }
  else if(!sound.isEmpty()) {
    sound = Tempest::SoundEffect();
    }

  // drops: box of particles in front of the camera, density scaled by weight. They stay on indoors, so rain
  // can be seen from inside a cave or house; drops under cover are skipped per drop (clipDrop)
  auto& fx = rainParticles();
  if(weight>0.f) {
    if(drops.isEmpty()) {
      fx.ppsValue = MaxDropsPps; // bucket sizes itself on creation
      drops = PfxEmitter(owner,&fx);
      drops.setLooped(true);
      }
    Tempest::Vec3 front = Tempest::Vec3(lp.front.x,0,lp.front.z);
    if(front.quadLength()>0.f)
      front = front/front.length();
    fx.ppsValue = MaxDropsPps*weight;
    tickWind(fx,dt);
    drops.setPosition(lp.pos + front*AheadOfCamera);
    drops.setActive(true);
    }
  else if(!drops.isEmpty()) {
    drops.setActive(false);
    }

  // splashes where and when drops land (placeSplash); the emitter only has to stay near the camera
  auto& sfx = splashParticles();
  if(weight>0.f) {
    if(splashes.isEmpty()) {
      splashes = PfxEmitter(owner,&sfx);
      splashes.setLooped(true);
      }
    splashes.setPosition(lp.pos);
    splashes.setActive(true);
    }
  else if(!splashes.isEmpty()) {
    splashes.setActive(false);
    }
  }

void Weather::tickWetness(const Tempest::Vec3& camera, uint64_t dt) {
  // wet in about 20 s of full rain, dry about 3 minutes after it stops
  const float target = weight;
  if(wet<target)
    wet = std::min(target, wet + float(dt)/20000.f); else
    wet = std::max(target, wet - float(dt)/180000.f);
  if(wet<=0.f)
    return;

  auto phys = owner.physic();
  auto view = owner.view();
  if(phys==nullptr || view==nullptr)
    return;

  // follow the camera in whole cells, keep what was traced already
  const int32_t n  = RainMapSize;
  const int32_t ox = int32_t(std::floor(camera.x/RainMapCell)) - n/2;
  const int32_t oz = int32_t(std::floor(camera.z/RainMapCell)) - n/2;
  if(map.height.size()!=size_t(n*n)) {
    map.height.assign(size_t(n*n), Unknown);
    map.ox = ox;
    map.oz = oz;
    }
  if(ox!=map.ox || oz!=map.oz) {
    std::vector<float> h(size_t(n*n), Unknown);
    for(int32_t z=0; z<n; ++z)
      for(int32_t x=0; x<n; ++x) {
        const int32_t sx = x + ox - map.ox;
        const int32_t sz = z + oz - map.oz;
        if(0<=sx && sx<n && 0<=sz && sz<n)
          h[size_t(z*n+x)] = map.height[size_t(sz*n+sx)];
        }
    map.height = std::move(h);
    map.ox     = ox;
    map.oz     = oz;
    }

  // trace a few cells per tick: straight down from above the world mesh, the first hit is where rain lands
  const float top = view->bbox().second.y + 100.f;
  for(int i=0; i<256; ++i) {
    const size_t  id = (mapNext++)%size_t(n*n);
    const int32_t x  = int32_t(id%size_t(n));
    const int32_t z  = int32_t(id/size_t(n));
    const float   wx = (float(map.ox + x) + 0.5f)*RainMapCell;
    const float   wz = (float(map.oz + z) + 0.5f)*RainMapCell;
    const auto    hit = phys->ray(Tempest::Vec3(wx,top,wz), Tempest::Vec3(wx,camera.y-5000.f,wz));
    map.height[id] = hit.hasCol ? hit.v.y : Open;
    }
  }

void Weather::tickWind(ParticleFx& fx, uint64_t dt) {
  // the original tilts the fall direction by the global wind (strength 70 +- 40, slowly turning; [ENGINE] zWind*)
  // scaled with [SKY_OUTDOOR] zRainWindScale: about 12 degrees at the average strength
  windTime += dt;
  const float t        = float(windTime%(1000u*1000u*1000u))/1000.f;
  const float strength = 70.f + 40.f*std::sin(t*0.4f) * std::sin(t*0.13f + 1.f);
  const float heading  = 2.0f*std::sin(t*0.021f) + 0.6f*std::sin(t*0.17f);
  const float tilt     = std::atan(strength*windScale)*180.f/float(M_PI);

  fx.dirAngleElev = -90.f + tilt;
  fx.dirAngleHead = heading*180.f/float(M_PI);
  }

ParticleFx& Weather::splashParticles() {
  // NOTE: lives for the whole program, see rainParticles
  static std::unique_ptr<ParticleFx> fx;
  if(fx!=nullptr)
    return *fx;

  zenkit::IParticleEffect src;
  src.pps_value             = SplashPps;
  src.pps_is_looping        = 1;
  src.pps_fps               = 1;
  src.shp_type_s            = "POINT";
  src.shp_for_s             = "WORLD";
  src.dir_mode_s            = "NONE";
  src.vel_avg               = 0;
  src.lsp_part_avg          = SplashLife;
  src.lsp_part_var          = 40;
  src.vis_name_s            = "SKYRAINSPLASH.TGA";
  src.vis_orientation_s     = "NONE";
  src.vis_tex_is_quadpoly   = 1;
  src.vis_tex_color_start_s = "190 200 220";
  src.vis_tex_color_end_s   = "190 200 220";
  src.vis_size_start_s      = "6 6";  // grows to 12 cm, about the drop width; the original shrinks a 25 cm quad
  src.vis_size_end_scale    = 2;
  src.vis_alpha_func_s      = "ADD";
  src.vis_alpha_start       = 130;
  src.vis_alpha_end         = 0;
  src.use_emitters_for      = 0;

  fx.reset(new ParticleFx(src,"OG_WEATHER_SPLASH"));
  fx->spawnHook = placeSplash;
  return *fx;
  }

ParticleFx& Weather::rainParticles() {
  // NOTE: lives for the whole program, since particle buckets keep a reference to their declaration
  static std::unique_ptr<ParticleFx> fx;
  if(fx!=nullptr)
    return *fx;

  zenkit::IParticleEffect src;
  src.pps_value             = MaxDropsPps;
  src.pps_is_looping        = 1;
  src.pps_fps               = 1;
  src.shp_type_s            = "BOX";
  src.shp_for_s             = "WORLD";
  src.shp_offset_vec_s      = "0 1000 0";
  src.shp_dim_s             = "1750 500 1750";
  src.shp_is_volume         = 1;
  src.dir_mode_s            = "DIR";
  src.dir_for_s             = "WORLD";
  src.dir_angle_elev        = -90;
  src.vel_avg               = 0.9f;
  src.vel_var               = 0.1f;
  src.lsp_part_avg          = DropLifetime;
  src.lsp_part_var          = 200;
  src.vis_name_s            = "SKYRAIN.TGA";
  src.vis_orientation_s     = "VELO";
  src.vis_tex_is_quadpoly   = 1;
  src.vis_tex_color_start_s = "190 200 220";
  src.vis_tex_color_end_s   = "190 200 220";
  src.vis_size_start_s      = "6 100";
  src.vis_size_end_scale    = 1;
  src.vis_alpha_func_s      = "ADD"; // unlit, so drops stay visible in the dimmed rain light
  src.vis_alpha_start       = 160;
  src.vis_alpha_end         = 160;
  src.use_emitters_for      = 0;

  fx.reset(new ParticleFx(src,"OG_WEATHER_RAIN"));
  fx->spawnHook = clipDrop;
  return *fx;
  }

void Weather::save(Serialize& fout) const {
  fout.write(prevSkyTime,rainStart,rainStop,rainCtr,lightning,rainActive);
  }

void Weather::load(Serialize& fin) {
  fin.read(prevSkyTime,rainStart,rainStop,rainCtr,lightning,rainActive);
  weight = rainWeightAt(skyTime(owner.time()),rainStart,rainStop);
  wet    = weight;
  }
