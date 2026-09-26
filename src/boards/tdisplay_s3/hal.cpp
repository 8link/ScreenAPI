// LilyGO T-Display-S3, non-touch (BOARDS.md, PROJECT.md D-032). The display
// is on an 8-bit parallel bus, driven by the ESP32-S3 LCD peripheral with DMA.
#include "hal.h"

#include <U8g2lib.h>
#include <battery_level.h>

#include "board.h"

namespace hal {

namespace {

// USB serial write timeout; must not be 0 (BOARDS.md Q-008).
constexpr uint32_t kSerialTimeoutMs = 10;
constexpr uint32_t kSerialBulkTimeoutMs = 1000;

constexpr int kBatterySamples = 16;

// Arduino_GFX 1.6.0's Arduino_ESP32LCD8::writeIndexedPixels sends a wrong color
// for the second pixel of every 2,046-pixel DMA chunk: the pixel it holds back
// for the command phase is overwritten by its packing loop (BOARDS.md Q-016).
// Its writePixels is correct, so the canvas flush goes through the palette
// into RGB565 here and uses that instead.
class Lcd8Bus : public Arduino_ESP32LCD8 {
public:
    using Arduino_ESP32LCD8::Arduino_ESP32LCD8;

    void writeIndexedPixels(uint8_t* data, uint16_t* idx, uint32_t len) override
    {
        static uint16_t pixels[LCD_MAX_PIXELS_AT_ONCE];
        while (len > 0) {
            const uint32_t count = len < LCD_MAX_PIXELS_AT_ONCE ? len : LCD_MAX_PIXELS_AT_ONCE;
            for (uint32_t i = 0; i < count; ++i) {
                pixels[i] = idx[data[i]];
            }
            writePixels(pixels, count);
            data += count;
            len -= count;
        }
    }
};

}  // namespace

void begin()
{
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(kSerialTimeoutMs);
#endif
    // GPIO15 switches the 3.3 V rail that feeds the display; on USB it is fed
    // anyway, on battery the screen stays dark without it (Q-013).
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);
    pinMode(PIN_BUTTON_DELETE, INPUT_PULLUP);
    pinMode(PIN_BUTTON_SCROLL, INPUT_PULLUP);
    analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
    // The backlight is a plain GPIO, active HIGH.
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
}

// Native USB serial (HWCDC): with the short timeout, output that does not fit
// the 256-byte transmit buffer is dropped; a screenshot waits for the host.
void setSerialBlocking(bool blocking)
{
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(blocking ? kSerialBulkTimeoutMs : kSerialTimeoutMs);
#else
    (void)blocking;
#endif
}

const char* description()
{
    return "ST7789 320 x 170 (8-bit parallel), 2 buttons, battery ADC";
}

// ST7789, 170 x 320, on the i80 bus. The panel sits in columns 35 to 204 of
// the controller's 240 x 320 memory; settings from LilyGO's Arduino_GFX examples.
Arduino_GFX* display()
{
    static Arduino_DataBus* bus = new Lcd8Bus(TFT_DC, TFT_CS, TFT_WR, TFT_RD, TFT_D0, TFT_D1, TFT_D2,
                                              TFT_D3, TFT_D4, TFT_D5, TFT_D6, TFT_D7);
    static Arduino_GFX* panel =
        new Arduino_ST7789(bus, TFT_RST, board::kScreenRotation, true, 170, 320, 35, 0, 35, 0);
    return panel;
}

// Write clock of the i80 bus (one byte per cycle); the library's default. The
// library clocks the LCD peripheral from the 240 MHz PLL, not the CPU clock.
int32_t displaySpeedHz()
{
    return 20000000;
}

// ASCII u8g2 fonts, sized for 320 x 170 on 1.9 inch (about 190 ppi).
Fonts fonts()
{
    return Fonts{u8g2_font_helvR14_tr, u8g2_font_helvB14_tr, u8g2_font_helvR14_tr, u8g2_font_helvR24_tr};
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

// No speaker on the T-Display-S3.
bool hasSpeaker()
{
    return false;
}

void playChime(Chime) {}

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
    // 2:1 divider: the pin sees half the battery voltage (LilyGO's examples double it).
    const uint32_t millivolts = 2 * sum / kBatterySamples;
    const ui::BatteryLevel level = ui::batteryLevel(millivolts);
    return Battery{level.external, level.percent, millivolts};
}

}  // namespace hal
