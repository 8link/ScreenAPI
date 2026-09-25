#include "particles.h"

#include <math.h>

namespace ui {

namespace {

constexpr float kPi = 3.14159265f;
constexpr int kAreaPerParticle = 8000;  // px^2: 10 particles at 240 x 135, 20 at 368 x 448
constexpr int kMinParticles = 10;
constexpr int kMaxCount = 20;
static_assert(kMaxCount <= kMaxParticles, "particle count exceeds the array");
constexpr float kMaxTurn = 2.5f;     // rad/s
constexpr float kTurnDrift = 6.0f;   // rad/s^2, random walk of the turn rate
constexpr float kMinSpeed = 0.25f;   // screen's shorter side per second
constexpr float kMaxSpeed = 0.5f;
constexpr float kGatherTurn = 6.0f;   // rad/s toward the gather point: circles of 6 to 11 px at 240 x 135
constexpr float kGatherTimeS = 0.4f;  // far particles rush in: speed at least distance / this
constexpr float kBoostDecayS = 0.35f;  // time constant of the burst speed-up
constexpr float kPaceEaseS = 0.3f;     // time constant of a pace change

float clampTo(float value, float low, float high)
{
    return value < low ? low : value > high ? high : value;
}

}  // namespace

float ParticleField::random()
{
    // xorshift32: small, fast, and the same sequence on every platform for a seed.
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return static_cast<float>(state_ >> 8) / static_cast<float>(1 << 24);
}

float ParticleField::randomRange(float low, float high)
{
    return low + (high - low) * random();
}

void ParticleField::start(int width, int height, uint32_t seed)
{
    width_ = width;
    height_ = height;
    state_ = seed != 0 ? seed : 1;  // xorshift never leaves 0
    gathering_ = false;
    boost_ = 1.0f;
    pace_ = 1.0f;
    paceTarget_ = 1.0f;
    const int area = width * height;
    count_ = area / kAreaPerParticle;
    count_ = count_ < kMinParticles ? kMinParticles : count_ > kMaxCount ? kMaxCount : count_;
    const float side = static_cast<float>(width < height ? width : height);
    for (int i = 0; i < count_; ++i) {
        Particle& p = particles_[i];
        p.heading = randomRange(0.0f, 2.0f * kPi);
        p.turn = randomRange(-kMaxTurn, kMaxTurn);
        p.speed = randomRange(kMinSpeed, kMaxSpeed) * side;
        p.hue = static_cast<uint8_t>(random() * kParticleHues) % kParticleHues;
        p.trail[0] = Point{randomRange(0.0f, static_cast<float>(width - 1)),
                           randomRange(0.0f, static_cast<float>(height - 1))};
        p.trailCount = 1;
    }
}

void ParticleField::step(float dtS)
{
    const float maxX = static_cast<float>(width_ - 1);
    const float maxY = static_cast<float>(height_ - 1);
    for (int i = 0; i < count_; ++i) {
        Particle& p = particles_[i];
        float speed = p.speed * boost_;
        if (gathering_) {
            const float dx = center_.x - p.trail[0].x;
            const float dy = center_.y - p.trail[0].y;
            const float diff = remainderf(atan2f(dy, dx) - p.heading, 2.0f * kPi);
            const float maxTurn = kGatherTurn * pace_ * dtS;
            p.heading += clampTo(diff, -maxTurn, maxTurn);
            const float rush = sqrtf(dx * dx + dy * dy) / kGatherTimeS;
            speed = (rush > speed ? rush : speed) * pace_;
        } else {
            p.turn = clampTo(p.turn + randomRange(-kTurnDrift, kTurnDrift) * dtS, -kMaxTurn, kMaxTurn);
            p.heading += p.turn * dtS;
        }
        float vx = cosf(p.heading) * speed;
        float vy = sinf(p.heading) * speed;
        Point next{p.trail[0].x + vx * dtS, p.trail[0].y + vy * dtS};
        if (next.x < 0.0f || next.x > maxX) {
            vx = -vx;
            next.x = clampTo(next.x, 0.0f, maxX);
        }
        if (next.y < 0.0f || next.y > maxY) {
            vy = -vy;
            next.y = clampTo(next.y, 0.0f, maxY);
        }
        p.heading = atan2f(vy, vx);
        const int kept = p.trailCount < kTrailLength ? p.trailCount : kTrailLength - 1;
        for (int j = kept; j > 0; --j) {
            p.trail[j] = p.trail[j - 1];
        }
        p.trail[0] = next;
        p.trailCount = kept + 1;
    }
    boost_ = 1.0f + (boost_ - 1.0f) * expf(-dtS / kBoostDecayS);
    pace_ = paceTarget_ + (pace_ - paceTarget_) * expf(-dtS / kPaceEaseS);
}

void ParticleField::setPace(float pace)
{
    paceTarget_ = pace;
}

void ParticleField::gather(float x, float y)
{
    gathering_ = true;
    center_ = Point{x, y};
}

void ParticleField::burst(float x, float y, float boost)
{
    gathering_ = false;
    boost_ = boost;
    pace_ = 1.0f;
    paceTarget_ = 1.0f;
    for (int i = 0; i < count_; ++i) {
        Particle& p = particles_[i];
        const float dx = p.trail[0].x - x;
        const float dy = p.trail[0].y - y;
        if (dx != 0.0f || dy != 0.0f) {
            p.heading = atan2f(dy, dx);
        }
        p.turn = 0.0f;
    }
}

uint8_t fadeLevel(uint32_t elapsedMs, uint32_t durationMs, uint32_t rampMs, uint8_t maxLevel)
{
    if (elapsedMs >= durationMs) {
        return 0;
    }
    const uint32_t fromEdge = elapsedMs < durationMs - elapsedMs ? elapsedMs : durationMs - elapsedMs;
    if (fromEdge >= rampMs) {
        return maxLevel;
    }
    return static_cast<uint8_t>(static_cast<uint32_t>(maxLevel) * fromEdge / rampMs);
}

}  // namespace ui
