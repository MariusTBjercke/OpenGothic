#pragma once

#include <Tempest/SoundEffect>
#include <Tempest/Vec>

#include <cstdint>
#include <string>
#include <vector>

#include "world/objects/pfxemitter.h"
#include "game/gametime.h"

class World;
class Serialize;
class ParticleFx;

// Outdoor rain, modeled after zCSkyControler_Outdoor of the original game.
// See docs/fork/research/rain.md for the reference behavior.
class Weather final {
  public:
    explicit Weather(World& owner);
    ~Weather();

    void  tick(uint64_t dt);

    // zstartrain: position inside a fresh rain window of length 'duration' (sky days)
    void  startRain(float position, float duration = 0.1f);

    float rainWeight() const { return weight; }
    bool  isRaining()  const { return weight>0.3f; } // Wld_IsRaining threshold of the original

    // one line for the 'weather' console command: today's window as clock times, weight, shelter
    std::string statusLine() const;

    // wet surfaces: wetness 0..1 (follows the rain, dries slowly) and the rain map, the height of the topmost
    // static surface per cell around the camera (shader/lighting/rain_wet.frag)
    struct RainMap {
      int32_t            ox = 0, oz = 0;  // cell index of the (0,0) corner
      std::vector<float> height;          // size*size; Unknown = not traced yet, Open = nothing hit
      };
    static constexpr int32_t RainMapSize = 128;
    static constexpr float   RainMapCell = 100.f;
    static constexpr float   Unknown     = 1e8f;
    static constexpr float   Open        = -1e8f;
    float          wetness() const { return wet; }
    const RainMap& rainMap() const { return map; }

    void  save(Serialize& fout) const;
    void  load(Serialize& fin);

    // pure helpers, sky time: 0 = noon, 0.5 = midnight
    static float skyTime(gtime t);
    static float rainWeightAt(float skyTime, float rainStart, float rainStop);

  private:
    void  rollRain();
    void  tickFx(uint64_t dt);
    void  tickWind(ParticleFx& fx, uint64_t dt);
    void  tickWetness(const Tempest::Vec3& camera, uint64_t dt);
    bool  isSheltered(const Tempest::Vec3& pos) const;

    static ParticleFx& rainParticles();
    static ParticleFx& splashParticles();

    World&        owner;
    bool          enabled     = true;

    float         prevSkyTime = -1.f;
    float         rainStart   = 0.187f;
    float         rainStop    = 0.25f;
    float         weight      = 0.f;
    int32_t       rainCtr     = 0;
    bool          lightning   = false;
    bool          rainActive  = false;

    bool          sheltered   = false;
    uint64_t      shelterTimer = 0;
    float         sndVolume   = 0.f;
    float         sndBase     = 1.f;
    float         windScale   = 0.003f; // [SKY_OUTDOOR] zRainWindScale
    uint64_t      windTime    = 0;
    Tempest::SoundEffect sound;
    PfxEmitter    drops;
    PfxEmitter    splashes;

    float         wet         = 0.f;
    RainMap       map;
    size_t        mapNext     = 0; // scan position for cells not traced yet
  };
