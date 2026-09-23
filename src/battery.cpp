#include "battery.h"

#include <Arduino.h>

#include "pins.h"

namespace battery {

namespace {

constexpr int kSamples = 16;

}  // namespace

void begin()
{
    // The divider on GPIO34 only conducts on battery power when ADC_EN is high (Q-004).
    pinMode(PIN_ADC_EN, OUTPUT);
    digitalWrite(PIN_ADC_EN, HIGH);
    analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
}

uint32_t readMillivolts()
{
    uint32_t sum = 0;
    for (int i = 0; i < kSamples; i++) {
        sum += analogReadMilliVolts(PIN_BATTERY_ADC);
    }
    // 100k / 100k divider: the pin sees half the battery voltage.
    return 2 * sum / kSamples;
}

}  // namespace battery
