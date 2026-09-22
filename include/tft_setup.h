// TFT_eSPI configuration. TFT_eSPI loads this file instead of its own
// User_Setup.h when tft_setup.h is on the include path, so the library
// files stay unmodified. Values follow the vendor Setup25_TTGO_T_Display.h;
// pins come from pins.h.
#pragma once

#include "pins.h"

#define ST7789_DRIVER
#define TFT_WIDTH  135
#define TFT_HEIGHT 240
// The 135 x 240 panel sits off-center in the ST7789 240 x 320 frame memory;
// this makes the library apply the offset for each rotation.
#define CGRAM_OFFSET
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4

#define SPI_FREQUENCY      40000000
#define SPI_READ_FREQUENCY 6000000
