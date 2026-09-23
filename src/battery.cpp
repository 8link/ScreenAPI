#include "battery.h"

#include <Arduino.h>

#include "board.h"

namespace battery {

namespace {

constexpr int kSamples = 16;

}  // namespace

bool available()
{
    return board::kHasBatterySense;
}

#if defined(PIN_BATTERY_ADC)
static_assert(board::kHasBatterySense, "pins.h defines PIN_BATTERY_ADC, so board.h must set kHasBatterySense");
#else
static_assert(!board::kHasBatterySense, "kHasBatterySense needs PIN_BATTERY_ADC and PIN_ADC_EN in pins.h");
#endif

#if defined(PIN_BATTERY_ADC)

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

#else

void begin() {}

uint32_t readMillivolts()
{
    return 0;
}

#endif

}  // namespace battery
