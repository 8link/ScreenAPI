# BOARDS.md

Hardware reference. Record board specifications, quirks, and usage findings when observed, including negative results.

## Board index

| Board | Revision | MCU | PlatformIO env | Status |
|-------|----------|-----|----------------|--------|
| LilyGO TTGO T-Display | TBD (see Identification) | ESP32 | TBD (board id `lilygo-t-display`) | First target, not yet brought up |

Copy the template at the end of this file once per new board.

---

## LilyGO TTGO T-Display

Sources: vendor repository https://github.com/Xinyuan-LilyGO/TTGO-T-Display (read at commit 6f89cdd, 2025-02-20): README, schematic `ESP32-TFT(6-26).pdf` dated 2019-06-26, `TFT_eSPI/User_Setups/Setup25_TTGO_T_Display.h`, `TFT_eSPI/examples/FactoryTest/FactoryTest.ino`. Also PlatformIO board definition `lilygo-t-display.json` (platform espressif32 7.0.1). Nothing below has been verified on the physical unit yet.

### Identification

- **Name:** LilyGO TTGO T-Display (1.14 inch LCD, ESP32)
- **Revision:** TBD for the unit in hand. The vendor README pinout column is labeled "V18"; the schematic is dated 2019-06-26.
- **Vendor:** LilyGO (Xinyuan)
- **PlatformIO board id:** `lilygo-t-display`
- **Framework:** TBD (the board id supports arduino and espidf)

### MCU

- **Chip:** ESP32, bare QFN48 chip on the schematic. Exact part number TBD.
- **Cores:** 2 (Xtensa LX6)
- **Clock:** 240 MHz (PlatformIO board definition)
- **Flash (size / mode / speed):** 4 MB (W25Q32 on the schematic) / DIO / 40 MHz (PlatformIO board definition)
- **PSRAM:** none. The vendor README says to select PSRAM Disabled; the schematic has no PSRAM.
- **USB type:** USB-C via a USB-UART bridge chip; auto-reset through DTR/RTS transistors. The bridge chip varies by revision, see Q-001.
- **Partition scheme:** TBD
- **Strapping pins:** GPIO0 (BUTTON2), GPIO2, GPIO5 (TFT_CS), GPIO12, GPIO15. See Q-003.

### Pin mapping

No pin header exists in code yet. This table is transcribed from the vendor README and `Setup25_TTGO_T_Display.h`. Once include/pins.h or include/config.h exists, the header is the source of truth and this table mirrors it.

| Signal | GPIO | Direction | Peripheral | Notes |
|--------|------|-----------|------------|-------|
| TFT_MOSI | 19 | Out | SPI (display) | |
| TFT_SCLK | 18 | Out | SPI (display) | |
| TFT_CS | 5 | Out | SPI (display) | Strapping pin |
| TFT_DC | 16 | Out | Display | |
| TFT_RST | 23 | Out | Display | |
| TFT_BL | 4 | Out | Backlight | Active HIGH |
| TFT_MISO | - | - | - | Not connected |
| BUTTON1 | 35 | In | Button | Input-only pin, no internal pull-up. Active LOW per factory test (ext0 wake on LOW). |
| BUTTON2 | 0 | In | Button | Strapping pin (Q-003) |
| ADC_IN (battery) | 34 | In | ADC1 | Input-only. Battery through a 2:1 divider. |
| ADC_EN | 14 | Out | Battery sense enable | Must be HIGH to measure on battery (Q-004) |
| I2C_SDA | 21 | - | I2C | On header; no on-board I2C devices on the schematic |
| I2C_SCL | 22 | - | I2C | On header |

Reserved and unusable pins:

- **Flash lines:** GPIO6 to GPIO11 (SPI flash). Do not use.
- **UART0:** GPIO1 (TX), GPIO3 (RX) go to the USB-UART bridge.
- **Strapping pins:** GPIO0, GPIO2, GPIO5, GPIO12, GPIO15.
- **Input-only pins:** GPIO34 to GPIO39. No output, no internal pull-up or pull-down.

### Connected peripherals

| Peripheral | Bus | Address or CS | Driver | Notes |
|------------|-----|---------------|--------|-------|
| ST7789V 1.14 inch TFT, 135 x 240 | SPI (VSPI pins) | CS GPIO5 | TFT_eSPI (vendor bundles 2.2.20) | See Display |
| Two push buttons | GPIO | GPIO35, GPIO0 | TBD | Plus a reset button on CHIP_PU |

### Power

- **Input:** USB-C 5 V. 3.3 V from an AP2112K-3.3 LDO (schematic).
- **Battery chemistry:** single-cell Li-ion / LiPo, 3.7 to 4.2 V (schematic connector label)
- **Battery capacity:** TBD (none supplied with the board)
- **Charge IC:** TP4054 (schematic), red charge LED
- **Voltage-sense pin and divider:** GPIO34, 100k / 100k divider (2:1). Vendor formula: `raw / 4095.0 * 2.0 * 3.3 * (vref / 1000.0)`, where vref comes from eFuse calibration (default 1100 mV). Needs ADC_EN (GPIO14) HIGH (Q-004).

Measured current per state:

| State | Current | Conditions | Measured (date) |
|-------|---------|------------|-----------------|
| Active with display | TBD | TBD | TBD |
| Radio on | TBD | TBD | TBD |
| Light sleep | TBD | TBD | TBD |
| Deep sleep | TBD | TBD | TBD |

- **Brown-out behavior:** TBD
- **Wake sources:** the factory test uses ext0 wake on GPIO35 LOW from deep sleep. Before sleeping it turns the backlight off and sends DISPOFF and SLPIN to the panel.

### Display

- **Controller:** ST7789V
- **Resolution:** 135 x 240 (240 x 135 in landscape, rotation 1)
- **Color depth:** 16 bit RGB565
- **Rotation:** TBD for this project. The factory test uses rotation 1 (landscape) for the splash and rotation 0 for color fills.
- **Bus and speed:** SPI write 40 MHz, read 6 MHz (Setup25). MISO not connected, so reads are not possible in practice.
- **Backlight:** GPIO4, active HIGH. PWM dimming TBD.
- **Refresh and flicker behavior:** TBD. A full-screen 16 bit sprite takes 240 x 135 x 2 = 64,800 bytes of internal RAM, since there is no PSRAM (calculation, not measured).
- **Offsets and init quirks:** the 135 x 240 panel sits off-center in the ST7789 240 x 320 frame memory. TFT_eSPI handles this with `CGRAM_OFFSET` in Setup25. A custom driver must apply the offset itself.

### Sensors

None on board.

| Sensor | Bus | Range | Sampling | Observed behavior |
|--------|-----|-------|----------|-------------------|
| - | - | - | - | - |

### Storage

- **NVS namespaces:** TBD
- **Filesystem:** TBD
- **SD card:** none on board. The vendor factory test shows an external SD card on HSPI: CS 33, SCLK 25, MISO 27, MOSI 26.

### BLE

- **Stack (NimBLE / Bluedroid):** TBD
- **Role:** peripheral during Wi-Fi provisioning (PROJECT.md F-002)
- **Services and UUIDs:** TBD (defined by the ESP BLE Provisioning protocol)
- **MTU:** TBD
- **Connection interval:** TBD
- **Observed quirks:** TBD
- **Wi-Fi coexistence:** TBD

### Wi-Fi

- **Modes:** station (PROJECT.md F-002)
- **Antenna:** on-board antenna (schematic); type TBD
- **Observed RSSI:** TBD
- **Quirks:** TBD

### UART

| Port | TX | RX | Baud | Purpose |
|------|----|----|------|---------|
| UART0 | GPIO1 | GPIO3 | 115200 (factory test) | USB serial, logs, flashing |

- **Framing:** 8N1 (default)
- **Boot-time noise:** TBD

### I2C / SPI / GPIO

- **Buses and pins:** display SPI on GPIO18 / 19 / 5. I2C SDA 21, SCL 22 on the header. A second SPI (HSPI) can use free header pins (factory test SD card example).
- **Clock:** display SPI 40 MHz
- **Pull-ups:** TBD
- **Addresses:** no on-board I2C devices
- **Free GPIO:** TBD, check against the unit's silkscreen
- **Interrupt notes:** TBD
- **Known hangs and recovery:** TBD

### Quirks and workarounds

| ID | Finding | Impact | Workaround | Verified (yes / no, date) |
|----|---------|--------|------------|---------------------------|
| Q-001 | The USB-UART bridge differs by revision. The 2019 schematic shows a CP2104. The PlatformIO board definition lists USB hwid 0x1A86:0x55D4 (WCH CH9102). The vendor README links both WCH and Silicon Labs drivers. | Wrong driver or no upload port on the host. | Check the chip on the unit or its USB VID:PID; install the matching driver if the OS lacks one. | no |
| Q-002 | The vendor README says their bundled TFT_eSPI compiles only up to arduino-esp32 2.0.14. The installed PlatformIO espressif32 7.0.1 ships framework-arduinoespressif32 3.20017 (arduino-esp32 2.0.17). | Possible build errors with the vendor library copy. | TBD: pin the platform version or use upstream TFT_eSPI. Decide at first build. | no |
| Q-003 | GPIO0 is BUTTON2 and a strapping pin. Held LOW during reset, the chip enters download mode. | Holding that button while powering on or resetting stops normal boot. | Do not rely on GPIO0 being held at boot. Prefer GPIO35 for the more frequent action (open question in PROJECT.md). | no |
| Q-004 | Battery voltage divider on GPIO34 is enabled by ADC_EN (GPIO14). Per the factory test comment, it is on by default with USB power, but GPIO14 must be driven HIGH on battery. | Battery reads wrong when GPIO14 is not HIGH. | Drive GPIO14 HIGH before sampling GPIO34. | no |

### Protocol findings

- **Frame layouts:** none yet
- **Checksums:** none yet
- **Timing windows:** none yet
- **Capture storage location:** TBD (captures are not committed to Git unless requested)

### References

- **Official docs:** https://github.com/Xinyuan-LilyGO/TTGO-T-Display
- **Schematic:** `schematic/ESP32-TFT(6-26).pdf` in the vendor repository
- **Datasheets:** TBD (ST7789V, TP4054, AP2112K, ESP32)
- **Vendor examples:** `TFT_eSPI/examples/FactoryTest/FactoryTest.ino` (buttons, battery voltage, Wi-Fi scan, deep sleep, SD card on HSPI); vendor test firmware `firmware/firmware.bin`

### On-device verification

- **Flash command:** `pio run -e <env> -t upload`
- **Expected boot serial output:** TBD
- **Smoke test:** TBD
- **Known-good firmware version:** none yet

---

## Template: <Board name>

### Identification

- **Name:** TBD
- **Revision:** TBD
- **Vendor:** TBD
- **PlatformIO board id:** TBD
- **Framework:** TBD

### MCU

- **Chip:** TBD
- **Cores:** TBD
- **Clock:** TBD
- **Flash (size / mode / speed):** TBD
- **PSRAM:** TBD
- **USB type (native CDC / JTAG / bridge chip):** TBD
- **Partition scheme:** TBD
- **Strapping pins:** TBD

### Pin mapping

This table mirrors the pin header in code (include/pins.h or include/config.h). The header is the source of truth; keep this table in sync.

| Signal | GPIO | Direction | Peripheral | Notes |
|--------|------|-----------|------------|-------|
| TBD | TBD | TBD | TBD | TBD |

Reserved and unusable pins:

- **Flash / PSRAM lines:** TBD
- **Strapping pins:** TBD
- **Input-only pins:** TBD

### Connected peripherals

| Peripheral | Bus | Address or CS | Driver | Notes |
|------------|-----|---------------|--------|-------|
| TBD | TBD | TBD | TBD | TBD |

### Power

- **Input:** TBD
- **Battery chemistry:** TBD
- **Battery capacity:** TBD
- **Charge IC:** TBD
- **Voltage-sense pin and divider:** TBD

Measured current per state:

| State | Current | Conditions | Measured (date) |
|-------|---------|------------|-----------------|
| Active with display | TBD | TBD | TBD |
| Radio on | TBD | TBD | TBD |
| Light sleep | TBD | TBD | TBD |
| Deep sleep | TBD | TBD | TBD |

- **Brown-out behavior:** TBD
- **Wake sources:** TBD

### Display

- **Controller:** TBD
- **Resolution:** TBD
- **Color depth:** TBD
- **Rotation:** TBD
- **Bus and speed:** TBD
- **Backlight:** TBD
- **Refresh and flicker behavior:** TBD
- **Offsets and init quirks:** TBD

### Sensors

| Sensor | Bus | Range | Sampling | Observed behavior |
|--------|-----|-------|----------|-------------------|
| TBD | TBD | TBD | TBD | TBD |

### Storage

- **NVS namespaces:** TBD
- **Filesystem:** TBD
- **SD card:** TBD

### BLE

- **Stack (NimBLE / Bluedroid):** TBD
- **Role:** TBD
- **Services and UUIDs:** TBD
- **MTU:** TBD
- **Connection interval:** TBD
- **Observed quirks:** TBD
- **Wi-Fi coexistence:** TBD

### Wi-Fi

- **Modes:** TBD
- **Antenna:** TBD
- **Observed RSSI:** TBD
- **Quirks:** TBD

### UART

| Port | TX | RX | Baud | Purpose |
|------|----|----|------|---------|
| TBD | TBD | TBD | TBD | TBD |

- **Framing:** TBD
- **Boot-time noise:** TBD

### I2C / SPI / GPIO

- **Buses and pins:** TBD
- **Clock:** TBD
- **Pull-ups:** TBD
- **Addresses:** TBD
- **Free GPIO:** TBD
- **Interrupt notes:** TBD
- **Known hangs and recovery:** TBD

### Quirks and workarounds

| ID | Finding | Impact | Workaround | Verified (yes / no, date) |
|----|---------|--------|------------|---------------------------|
| Q-001 | TBD | TBD | TBD | TBD |

### Protocol findings

- **Frame layouts:** TBD
- **Checksums:** TBD
- **Timing windows:** TBD
- **Capture storage location:** TBD (captures are not committed to Git unless requested)

### References

- **Official docs:** TBD
- **Schematic:** TBD
- **Datasheets:** TBD
- **Vendor examples:** TBD

### On-device verification

- **Flash command:** `pio run -e <env> -t upload`
- **Expected boot serial output:** TBD
- **Smoke test:** TBD
- **Known-good firmware version:** TBD
