// Template board hardware layer (PROJECT.md "Adding a board", D-032).
// Implements src/hal.h for a generic ESP32 with an SPI display and two
// buttons. Change the parts marked PORT; keep the function signatures.
#include "hal.h"

#include <U8g2lib.h>

#include "board.h"

namespace hal {

void begin()
{
    // PORT: configure your inputs. Touchscreens, I/O expanders, and power chips
    // are started here too (see src/boards/waveshare_amoled18/hal.cpp).
    pinMode(PIN_BUTTON_DELETE, INPUT_PULLUP);
    pinMode(PIN_BUTTON_SCROLL, INPUT_PULLUP);
    if (LCD_BL >= 0) {
        pinMode(LCD_BL, OUTPUT);
        digitalWrite(LCD_BL, HIGH);
    }
}

// PORT: nothing to do on a UART with a USB bridge. On native USB serial
// (ESP32-S2, S3, C3), set the TX timeout as the Waveshare AMOLED board does.
void setSerialBlocking(bool) {}

const char* description()
{
    return "template: ST7789 320 x 240, 2 buttons, no battery";  // PORT
}

// PORT: your display's bus and driver. Arduino_GFX supports many controllers
// (ST7789, ILI9341, GC9A01, SH8601 via lib/sh8601_display, CO5300, ...) and
// buses (SPI, QSPI, parallel, RGB). The driver gets the rotation from board.h.
Arduino_GFX* display()
{
    static Arduino_DataBus* bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, GFX_NOT_DEFINED);
    static Arduino_GFX* panel = new Arduino_ST7789(bus, LCD_RST, board::kScreenRotation, true, 240, 320);
    return panel;
}

int32_t displaySpeedHz()
{
    return 40000000;  // PORT: the fastest clock your wiring allows
}

// PORT: fonts for your pixel density. u8g2 names: helvR08 to helvR24, ncenR,
// timR, fur (Free Universal), and many more; use the "_tr" (ASCII) variants.
Fonts fonts()
{
    return Fonts{u8g2_font_helvR14_tr, u8g2_font_helvR14_tr, u8g2_font_helvR14_tr, u8g2_font_helvR24_tr};
}

void setDisplayOn(bool on)
{
    // PORT: panel sleep and backlight. For an AMOLED without a backlight, the
    // panel's displayOff() and displayOn() alone (see waveshare_amoled18).
    if (!on) {
        if (LCD_BL >= 0) {
            digitalWrite(LCD_BL, LOW);
        }
        display()->displayOff();
    } else {
        display()->displayOn();
        if (LCD_BL >= 0) {
            digitalWrite(LCD_BL, HIGH);
        }
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

// PORT: return true and implement playChime() if the board has a speaker (I2S
// codec as on the AMOLED); it must not block the caller.
bool hasSpeaker()
{
    return false;
}

void playChime() {}

// PORT: return true and implement readBattery() if the board can measure its
// battery (ADC divider as on the T-Display, or a power chip as on the AMOLED).
bool hasBattery()
{
    return false;
}

Battery readBattery()
{
    return Battery{true, 0, 0};
}

}  // namespace hal
