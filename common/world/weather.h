#pragma once

#include <Tempest/SoundEffect>
#include <Tempest/Vec>

#include <cstdint>
#include <string>

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

    void  save(Serialize& fout) const;
    void  load(Serialize& fin);

    // pure helpers, sky time: 0 = noon, 0.5 = midnight
    static float skyTime(gtime t);
    static float rainWeightAt(float skyTime, float rainStart, float rainStop);

  private:
    void  rollRain();
    void  tickFx(uint64_t dt);
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
    float         sndVolume   = 0.f;
    float         sndBase     = 1.f;
    Tempest::SoundEffect sound;
    PfxEmitter    drops;
    PfxEmitter    splashes;
  };
