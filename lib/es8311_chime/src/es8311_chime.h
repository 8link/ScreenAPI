// Message chime through an ES8311 audio codec (PROJECT.md F-013). Shared by
// boards with an ES8311 and a speaker; each board passes its pins and how to
// switch its speaker amplifier (a GPIO, or a power chip over I2C).
#pragma once

#include <Wire.h>
#include <stdint.h>

namespace es8311_chime {

struct Config {
    TwoWire* wire;    // the codec's I2C bus, already started
    uint8_t address;  // 0x18 with the codec's CE pin low
    int pinMclk;
    int pinBclk;
    int pinWs;
    int pinDout;
    void (*setAmplifier)(bool on);  // called from the chime task
};

// Sets up I2S and the codec and starts the chime task. false if I2S or the
// codec fails; play() then does nothing.
bool begin(const Config& config);

enum class Tune : uint8_t {
    NewMessage,  // E6 then C6, bell-like
    Queued,      // two quick C6 taps
    Timed,       // one short G6 ping
    Rejected,    // low C5 then A4
};

// Queues a tune and returns at once. Tunes requested while one plays follow it
// in order; a request beyond kMaxPending waiting tunes is dropped.
constexpr int kMaxPending = 8;
void play(Tune tune);

}  // namespace es8311_chime
