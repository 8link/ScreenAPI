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

// Starts a chime and returns at once; ignored while a chime plays.
void play();

}  // namespace es8311_chime
