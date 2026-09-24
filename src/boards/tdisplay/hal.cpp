// LilyGO TTGO T-Display (BOARDS.md, PROJECT.md D-032).
#include "hal.h"

#include <U8g2lib.h>
#include <battery_level.h>

#include "board.h"

namespace hal {

namespace {

constexpr int kBatterySamples = 16;

}  // namespace

void begin()
{
    // GPIO35 is input-only and has no internal pull-up; the board provides an external one.
    pinMode(PIN_BUTTON_DELETE, INPUT);
    pinMode(PIN_BUTTON_SCROLL, INPUT_PULLUP);
    // The divider on GPIO34 only conducts on battery power when ADC_EN is high (Q-004).
    pinMode(PIN_ADC_EN, OUTPUT);
    digitalWrite(PIN_ADC_EN, HIGH);
    analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
    // The backlight is a plain GPIO, active HIGH.
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
}

// UART0 through the USB bridge: writes wait for the FIFO anyway.
void setSerialBlocking(bool) {}

const char* description()
{
    return "ST7789 240 x 135, 2 buttons, battery ADC";
}

// ST7789V, 135 x 240, on SPI. The panel sits off-center in the controller's
// 240 x 320 memory; the offsets (52, 40 and 53, 40) place it for each rotation.
Arduino_GFX* display()
{
    static Arduino_DataBus* bus =
        new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, GFX_NOT_DEFINED, VSPI, false);
    static Arduino_GFX* panel =
        new Arduino_ST7789(bus, TFT_RST, board::kScreenRotation, true, 135, 240, 52, 40, 53, 40);
    return panel;
}

int32_t displaySpeedHz()
{
    return 40000000;
}

// ASCII u8g2 fonts, sized for 240 x 135 on 1.14 inch.
Fonts fonts()
{
    return Fonts{u8g2_font_helvR12_tr, u8g2_font_helvR12_tr, u8g2_font_helvR12_tr, u8g2_font_helvR18_tr};
}

void setDisplayOn(bool on)
{
    // Backlight off before the panel sleeps and on only after it has woken
    // (SLPIN and SLPOUT take 120 ms each), so the transition is not visible.
    if (!on) {
        digitalWrite(TFT_BL, LOW);
        display()->displayOff();
    } else {
        display()->displayOn();
        digitalWrite(TFT_BL, HIGH);
    }
}

bool deletePressed()
{
    return digitalRead(PIN_BUTTON_DELETE) == LOW;
}

bool scrollPressed()
{
    return digitalRead(PIN_BUTTON_SCROLL) == LOW;
}

// No speaker on the T-Display.
bool hasSpeaker()
{
    return false;
}

void playChime() {}

bool hasBattery()
{
    return true;
}

Battery readBattery()
{
    uint32_t sum = 0;
    for (int i = 0; i < kBatterySamples; i++) {
        sum += analogReadMilliVolts(PIN_BATTERY_ADC);
    }
    // 100k / 100k divider: the pin sees half the battery voltage.
    const uint32_t millivolts = 2 * sum / kBatterySamples;
    const ui::BatteryLevel level = ui::batteryLevel(millivolts);
    return Battery{level.external, level.percent, millivolts};
}

}  // namespace hal
