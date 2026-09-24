// Pin assignments for the M5Stack M5StickS3.
// Single source of truth for this board's pins; the BOARDS.md pin table mirrors this file.
// Used by src/boards/m5sticks3/hal.cpp. Values from M5Stack's StickS3 pin map
// (docs.m5stack.com/en/core/StickS3) and M5GFX / M5Unified.
#pragma once

// ST7789P3 display on SPI. MISO is not connected. The panel's supply is
// switched by the M5PM1 power chip (PM1 GPIO2).
#define TFT_MOSI 39
#define TFT_SCLK 40
#define TFT_DC   45  // strapping pin
#define TFT_CS   41
#define TFT_RST  21
#define TFT_BL   38  // backlight, active HIGH

// Buttons, active LOW, with pull-ups on the board. KEY1 is the large front
// button, KEY2 the side button.
#define PIN_BUTTON_DELETE 11  // KEY1
#define PIN_BUTTON_SCROLL 12  // KEY2

// Internal I2C bus: M5PM1 power chip, ES8311 codec, BMI270 IMU (unused).
#define PIN_I2C_SDA 47
#define PIN_I2C_SCL 48

// Audio: ES8311 codec on I2S (data out to the AW8737 speaker amplifier). The
// amplifier is switched by the M5PM1 (PM1 GPIO3).
#define PIN_I2S_MCLK 18
#define PIN_I2S_BCLK 17
#define PIN_I2S_WS   15
#define PIN_I2S_DOUT 14

// I2C addresses
#define I2C_ADDR_M5PM1  0x6E  // power management: battery, USB, LCD and amplifier power
#define I2C_ADDR_ES8311 0x18  // audio codec
