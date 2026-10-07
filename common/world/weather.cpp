#include "weather.h"

#include <zenkit/addon/daedalus.hh>

#include <algorithm>
#include <cstdlib>
#include <memory>

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

static float rand01() {
  return float(std::rand())/float(RAND_MAX);
  }

Weather::Weather(World& owner)
  :owner(owner) {
  auto fx = Gothic::settingsGetS("GAME","skyEffects");
  enabled = (fx.empty() || fx!="0");
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
  // portal rooms (houses, caves with portals) count as indoor
  return !owner.roomAt(pos).empty();
  }

void Weather::tickFx(uint64_t dt) {
  const auto lp        = owner.gameSession().camera().listenerPosition();
  const bool sheltered = isSheltered(lp.pos);

  // sound: follows the rain weight with a fixed slope, quieter when sheltered
  const float target = weight*(sheltered ? 0.25f : 1.f);
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

  // drops: box of particles in front of the camera, density scaled by weight
  auto& fx = rainParticles();
  if(weight>0.f && !sheltered) {
    if(drops.isEmpty()) {
      fx.ppsValue = MaxDropsPps; // bucket sizes itself on creation
      drops = PfxEmitter(owner,&fx);
      drops.setLooped(true);
      }
    Tempest::Vec3 front = Tempest::Vec3(lp.front.x,0,lp.front.z);
    if(front.quadLength()>0.f)
      front = front/front.length();
    fx.ppsValue = MaxDropsPps*weight;
    drops.setPosition(lp.pos + front*AheadOfCamera);
    drops.setActive(true);
    }
  else if(!drops.isEmpty()) {
    drops.setActive(false);
    }
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
  return *fx;
  }

void Weather::save(Serialize& fout) const {
  fout.write(prevSkyTime,rainStart,rainStop,rainCtr,lightning,rainActive);
  }

void Weather::load(Serialize& fin) {
  fin.read(prevSkyTime,rainStart,rainStop,rainCtr,lightning,rainActive);
  weight = rainWeightAt(skyTime(owner.time()),rainStart,rainStop);
  }
