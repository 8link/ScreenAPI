// Display driver and fonts for the LilyGO TTGO T-Display (PROJECT.md D-031).
// Included only by src/screen.cpp.
#pragma once

#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>

#include "board.h"

namespace board {

// ST7789V, 135 x 240, on SPI. The panel sits off-center in the controller's
// 240 x 320 memory; the offsets (52, 40 and 53, 40) place it for each rotation.
inline Arduino_GFX* createDisplay()
{
    static Arduino_DataBus* bus =
        new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, GFX_NOT_DEFINED, VSPI, false);
    static Arduino_GFX* panel = new Arduino_ST7789(bus, TFT_RST, kScreenRotation, true, 135, 240, 52, 40, 53, 40);
    return panel;
}

constexpr int32_t kDisplaySpeedHz = 40000000;

// Called before the panel starts: the backlight is a plain GPIO, active HIGH.
inline void powerOnDisplay()
{
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
}

// ASCII u8g2 fonts, sized for 240 x 135 on 1.14 inch.
struct Fonts {
    const uint8_t* bar;
    const uint8_t* title;
    const uint8_t* small;
    const uint8_t* large;
};

inline Fonts fonts()
{
    return Fonts{u8g2_font_helvR12_tr, u8g2_font_helvR12_tr, u8g2_font_helvR12_tr, u8g2_font_helvR18_tr};
}

}  // namespace board
