// Pin assignments for the LilyGO TTGO T-Display.
// Single source of truth for this board's pins; the BOARDS.md pin table mirrors this file.
// Every board provides its display pins (used by display.h), PIN_BUTTON_DELETE,
// and PIN_BUTTON_SCROLL; PIN_BATTERY_ADC and PIN_ADC_EN only with battery sense.
#pragma once

// ST7789V display on SPI (display.h). MISO is not connected on this board.
#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS   5   // strapping pin
#define TFT_DC   16
#define TFT_RST  23
#define TFT_BL   4   // backlight, active HIGH

// Buttons, active LOW.
#define PIN_BUTTON_DELETE 35  // input-only, relies on the board's external pull-up
#define PIN_BUTTON_SCROLL 0   // strapping pin: held LOW at reset enters download mode (BOARDS.md Q-003)

// Battery sense: 2:1 divider on GPIO34, enabled by driving ADC_EN HIGH (BOARDS.md Q-004).
#define PIN_BATTERY_ADC 34
#define PIN_ADC_EN      14

// I2C on the header; no on-board devices.
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
