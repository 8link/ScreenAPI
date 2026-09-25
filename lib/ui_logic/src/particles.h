// Particles with fading trails for the screen saver animation (PROJECT.md F-014).
// Hardware-independent: the caller passes the seed and the time step, and draws
// the trails.
#pragma once

#include <stdint.h>

namespace ui {

constexpr int kMaxParticles = 24;
constexpr int kTrailLength = 20;  // positions per trail, the newest first
constexpr int kParticleHues = 6;

struct Point {
    float x;
    float y;
};

struct Particle {
    float heading;  // radians
    float turn;     // radians per second, drifts at random
    float speed;    // pixels per second
    uint8_t hue;    // 0 .. kParticleHues - 1
    Point trail[kTrailLength];
    int trailCount;  // valid entries in trail
};

class ParticleField {
public:
    // Scatters particles over a width x height area with random headings,
    // speeds, and hues. The count and speed scale with the area, so small and
    // large screens look alike.
    void start(int width, int height, uint32_t seed);

    // Moves every particle by dtS seconds; particles bounce off the edges.
    void step(float dtS);

    // While gathering, every particle turns toward (x, y) instead of drifting
    // at random, far ones faster, so the swarm closes in and circles that point.
    void gather(float x, float y);
    // Ends gathering: every particle heads straight away from (x, y) at
    // `boost` times its speed, easing back to its own speed within about a second.
    void burst(float x, float y, float boost);
    // Scales speed and turning, so a gathered swarm circles at the same radius
    // but more slowly; eases to the new pace within about a second. A burst
    // resets it to 1.
    void setPace(float pace);

    int count() const { return count_; }
    const Particle& particle(int index) const { return particles_[index]; }

private:
    float random();  // 0 .. 1
    float randomRange(float low, float high);

    Particle particles_[kMaxParticles];
    int count_ = 0;
    bool gathering_ = false;
    Point center_{0.0f, 0.0f};
    float boost_ = 1.0f;  // speed factor after a burst, decays to 1
    float pace_ = 1.0f;
    float paceTarget_ = 1.0f;
    int width_ = 0;
    int height_ = 0;
    uint32_t state_ = 1;
};

// Brightness of the whole animation: rises from 0 to maxLevel over rampMs,
// holds, and falls back to 0 over the last rampMs of durationMs.
uint8_t fadeLevel(uint32_t elapsedMs, uint32_t durationMs, uint32_t rampMs, uint8_t maxLevel);

}  // namespace ui
