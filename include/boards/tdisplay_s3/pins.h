// Pin assignments for the LilyGO T-Display-S3 (non-touch).
// Single source of truth for this board's pins; the BOARDS.md pin table mirrors this file.
// Used by src/boards/tdisplay_s3/hal.cpp. Values from LilyGO's pin_config.h
// (github.com/Xinyuan-LilyGO/T-Display-S3, examples/*/pin_config.h).
#pragma once

// ST7789 display on an 8-bit parallel (i80) bus, driven by the ESP32-S3 LCD
// peripheral. RD is held high; the firmware never reads the panel. The names
// avoid LCD_*, which the Arduino core's board variant already defines.
#define TFT_D0  39
#define TFT_D1  40
#define TFT_D2  41
#define TFT_D3  42
#define TFT_D4  45  // strapping pin
#define TFT_D5  46  // strapping pin
#define TFT_D6  47
#define TFT_D7  48
#define TFT_WR  8
#define TFT_RD  9
#define TFT_DC  7
#define TFT_CS  6
#define TFT_RST 5
#define TFT_BL  38  // backlight, active HIGH

// Switches the 3.3 V rail for the display and the header (V3V). Must be HIGH
// on battery power, or the screen stays dark (BOARDS.md Q-013).
#define PIN_POWER_ON 15

// Buttons, active LOW, with pull-ups on the board. GPIO14 is the KEY button,
// GPIO0 the BOOT button (the same roles as on the T-Display).
#define PIN_BUTTON_DELETE 14
#define PIN_BUTTON_SCROLL 0   // strapping pin: held LOW at reset enters download mode

// Battery sense: 2:1 divider on GPIO4 (ADC1). Only reads the cell when USB is
// unplugged (LilyGO README).
#define PIN_BATTERY_ADC 4

// I2C on the header and the touch connector; unused on the non-touch board.
#define PIN_I2C_SDA 18
#define PIN_I2C_SCL 17
