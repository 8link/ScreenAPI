// Template board pins (PORT: replace all of them with your board's wiring).
// Example wiring: a generic ESP32 dev board with a 240 x 320 ST7789 SPI display
// and two push buttons to GND. Used by src/boards/template/hal.cpp.
#pragma once

// SPI display
#define LCD_SCLK 18
#define LCD_MOSI 23
#define LCD_CS   15
#define LCD_DC   2
#define LCD_RST  4
#define LCD_BL   32  // backlight, active HIGH; -1 if always on

// Buttons, active LOW with internal pull-ups
#define PIN_BUTTON_DELETE 0   // BOOT on most boards; do not hold it during reset
#define PIN_BUTTON_SCROLL 13
