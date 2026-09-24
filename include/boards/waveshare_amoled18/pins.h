// Pin assignments for the Waveshare ESP32-S3-Touch-AMOLED-1.8.
// Single source of truth for this board's pins; the BOARDS.md pin table mirrors this file.
// Used by src/boards/waveshare_amoled18/hal.cpp. Values from Waveshare's pin_config.h
// (identical for the original and the V2 revision).
#pragma once

// QSPI AMOLED (SH8601 original, CO5300 V2). No reset pin: the reset line is on
// the I2C expander.
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK  11
#define LCD_CS    12

// I2C bus: expander, touch, power chip (also RTC and IMU, unused).
#define PIN_I2C_SDA   15
#define PIN_I2C_SCL   14
#define PIN_TOUCH_INT 21  // unused: touch is polled

// BOOT button, active LOW. Strapping pin: held LOW at reset enters download mode.
#define PIN_BUTTON_DELETE 0

// I2C addresses
#define I2C_ADDR_EXPANDER 0x20  // TCA9554: pins 0 to 2 drive the display and touch resets
#define I2C_ADDR_FT3168   0x38  // touch, original revision
#define I2C_ADDR_CST820   0x15  // touch, V2 revision
#define I2C_ADDR_AXP2101  0x34  // power management
