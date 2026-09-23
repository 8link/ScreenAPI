# BOARDS.md

Hardware reference. Record board specifications, quirks, and usage findings when observed, including negative results.

## Board index

| Board | Revision | MCU | PlatformIO env | Status |
|-------|----------|-----|----------------|--------|
| LilyGO TTGO T-Display | TBD (see Identification) | ESP32 | `tdisplay` (board id `lilygo-t-display`) | Brought up with 0.0.1 (verified on device 2026-09-22) |

Copy the template at the end of this file once per new board.

---

## LilyGO TTGO T-Display

Sources: vendor repository https://github.com/Xinyuan-LilyGO/TTGO-T-Display (read at commit 6f89cdd, 2025-02-20): README, schematic `ESP32-TFT(6-26).pdf` dated 2019-06-26, `TFT_eSPI/User_Setups/Setup25_TTGO_T_Display.h`, `TFT_eSPI/examples/FactoryTest/FactoryTest.ino`. Also PlatformIO board definition `lilygo-t-display.json` (platform espressif32 7.0.1). Values come from these sources unless marked as observed or verified on the unit in hand (a T-Display with ESP32-D0WDQ6 and CP2104, used since 2026-09-22).

### Identification

- **Name:** LilyGO TTGO T-Display (1.14 inch LCD, ESP32)
- **Revision:** TBD for the unit in hand. The vendor README pinout column is labeled "V18"; the schematic is dated 2019-06-26.
- **Vendor:** LilyGO (Xinyuan)
- **PlatformIO board id:** `lilygo-t-display`
- **Framework:** Arduino (arduino-esp32), PROJECT.md D-006

### MCU

- **Chip:** ESP32-D0WDQ6, revision v1.0 (reported by esptool on the unit in hand, 2026-09-22). MAC 24:6f:28:25:36:e8.
- **Cores:** 2 (Xtensa LX6)
- **Clock:** 240 MHz (PlatformIO board definition)
- **Flash (size / mode / speed):** 4 MB (W25Q32 on the schematic) / DIO / 40 MHz (PlatformIO board definition)
- **PSRAM:** none. The vendor README says to select PSRAM Disabled; the schematic has no PSRAM.
- **USB type:** USB-C via a USB-UART bridge chip; auto-reset through DTR/RTS transistors. The unit in hand has a Silicon Labs CP2104 (USB 10c4:ea60). The bridge chip varies by revision, see Q-001.
- **Partition scheme:** `min_spiffs.csv` since 0.0.7 (PROJECT.md D-024):

| Name | Type | SubType | Offset | Size |
|------|------|---------|--------|------|
| nvs | data | nvs | 0x9000 | 20K |
| otadata | data | ota | 0xe000 | 8K |
| app0 | app | ota_0 | 0x10000 | 1920K |
| app1 | app | ota_1 | 0x1F0000 | 1920K |
| spiffs | data | spiffs | 0x3D0000 | 128K (used as LittleFS) |
| coredump | data | coredump | 0x3F0000 | 64K |

  Before 0.0.7: Arduino default (app0 and app1 1280K each, spiffs 0x290000 1408K).
- **Strapping pins:** GPIO0 (BUTTON2), GPIO2, GPIO5 (TFT_CS), GPIO12, GPIO15. See Q-003.

### Pin mapping

Mirrors `include/pins.h`, which is the source of truth. Values come from the vendor README and `Setup25_TTGO_T_Display.h`.

| Signal | GPIO | Direction | Peripheral | Notes |
|--------|------|-----------|------------|-------|
| TFT_MOSI | 19 | Out | SPI (display) | |
| TFT_SCLK | 18 | Out | SPI (display) | |
| TFT_CS | 5 | Out | SPI (display) | Strapping pin |
| TFT_DC | 16 | Out | Display | |
| TFT_RST | 23 | Out | Display | |
| TFT_BL | 4 | Out | Backlight | Active HIGH |
| TFT_MISO | - | - | - | Not connected; not defined in pins.h |
| PIN_BUTTON_DELETE | 35 | In | Button | Input-only pin, no internal pull-up; the board has an external one. Active LOW per factory test (ext0 wake on LOW). With `pinMode(INPUT)` the pin read idle HIGH: no phantom presses in 68 s untouched (0.0.3, 2026-09-22). |
| PIN_BUTTON_SCROLL | 0 | In | Button | Strapping pin (Q-003) |
| PIN_BATTERY_ADC | 34 | In | ADC1 | Input-only. Battery through a 2:1 divider. |
| PIN_ADC_EN | 14 | Out | Battery sense enable | Must be HIGH to measure on battery (Q-004) |
| PIN_I2C_SDA | 21 | - | I2C | On header; no on-board I2C devices on the schematic |
| PIN_I2C_SCL | 22 | - | I2C | On header |

Reserved and unusable pins:

- **Flash lines:** GPIO6 to GPIO11 (SPI flash). Do not use.
- **UART0:** GPIO1 (TX), GPIO3 (RX) go to the USB-UART bridge.
- **Strapping pins:** GPIO0, GPIO2, GPIO5, GPIO12, GPIO15.
- **Input-only pins:** GPIO34 to GPIO39. No output, no internal pull-up or pull-down.

### Connected peripherals

| Peripheral | Bus | Address or CS | Driver | Notes |
|------------|-----|---------------|--------|-------|
| ST7789V 1.14 inch TFT, 135 x 240 | SPI (VSPI pins) | CS GPIO5 | TFT_eSPI 2.5.43 (upstream; vendor bundles 2.2.20) | See Display |
| Two push buttons | GPIO | GPIO35 (delete), GPIO0 (scroll) | TBD | Plus a reset button on CHIP_PU. Mapping per PROJECT.md D-007. |

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
- **Rotation:** 1 (landscape, 240 x 135) in this project since 0.0.1. The factory test uses rotation 1 for the splash and rotation 0 for color fills.
- **Bus and speed:** SPI write 40 MHz, read 6 MHz (Setup25). MISO not connected, so reads are not possible in practice.
- **Backlight:** GPIO4, active HIGH. PWM dimming TBD.
- **Refresh and flicker behavior:** since 0.0.3 every frame is drawn into a full-screen 8-bit sprite (32,400 bytes) and pushed at once, up to every 33 ms while text scrolls (PROJECT.md D-019). A 16-bit sprite would take 64,800 bytes. No visible flicker (checked by the user, 2026-09-23).
- **Offsets and init quirks:** the 135 x 240 panel sits off-center in the ST7789 240 x 320 frame memory. TFT_eSPI handles this with `CGRAM_OFFSET` in Setup25. A custom driver must apply the offset itself.

### Sensors

None on board.

| Sensor | Bus | Range | Sampling | Observed behavior |
|--------|-----|-------|----------|-------------------|
| - | - | - | - | - |

### Storage

- **NVS namespaces:** the ESP-IDF Wi-Fi driver keeps the station settings in NVS (per ESP-IDF; not inspected).
- **Filesystem:** LittleFS on the `spiffs` partition. Since 0.0.7: 131,072 bytes at 0x3D0000, formatted on the first 0.0.7 boot. Before: 1,441,792 bytes at 0x290000; blank (all 0xFF, read back on 2026-09-23) and formatted by 0.0.6. Observed with 0.0.6:
  - The first mount of the blank partition logs `Corrupted dir pair at {0x0, 0x1}` and `mount failed, (-84)`, then formats and mounts. Expected once; harmless.
  - `LittleFS.exists()` on a missing file logs `open(): ... does not exist, no permits for creation` as an error (arduino-esp32 behavior). Harmless.
  - `LittleFS.rename()` replaces an existing file; no remove needed.
  - Writing about 1 KB (temporary file plus rename) takes 65 to 70 ms and blocks the loop for that time.
  - A fresh file system uses 8,192 bytes; with `/queue.bin` 12,288 bytes.
- **SD card:** none on board. The vendor factory test shows an external SD card on HSPI: CS 33, SCLK 25, MISO 27, MOSI 26.

### BLE

- **Stack (NimBLE / Bluedroid):** Bluedroid, from the precompiled Arduino core, used through ESP-IDF `wifi_provisioning` (Arduino `WiFiProv`).
- **Role:** peripheral during Wi-Fi provisioning (PROJECT.md F-002). The stack's memory is released after setup, or right at boot when already provisioned; BLE is off in normal operation. Needs the `btInUse()` override (Q-005).
- **Services and UUIDs:** ESP BLE Provisioning protocol; service UUID from the Arduino `WiFiProv` wrapper (`custom_service_uuid`). Advertised name `PROV_` + last 3 MAC bytes (`PROV_2536E8` on the unit in hand).
- **MTU:** TBD
- **Connection interval:** TBD
- **Observed quirks:** TBD
- **Wi-Fi coexistence:** TBD

### Wi-Fi

- **Modes:** station (PROJECT.md F-002)
- **Saved settings found:** the unit in hand came with a saved network, `WLAN3`, not written by this project; with 0.0.7 it connected and got 192.168.10.122 by DHCP about 0.9 s after boot (2026-09-23).
- **Antenna:** on-board antenna (schematic); type TBD
- **Observed RSSI:** TBD
- **Quirks:** TBD

### UART

| Port | TX | RX | Baud | Purpose |
|------|----|----|------|---------|
| UART0 | GPIO1 | GPIO3 | 115200 (factory test) | USB serial, logs, flashing |

- **Framing:** 8N1 (default)
- **Boot-time noise:** none observed at 115200. The ROM boot log is readable at the same baud rate as the firmware. Observed with 0.0.1, 2026-09-22 (power-on reset):

```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1184
load:0x40078000,len:13232
load:0x40080400,len:3028
entry 0x400805e4
ScreenAPI v0.0.1
```

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
| Q-001 | The USB-UART bridge differs by revision. The 2019 schematic shows a CP2104. The PlatformIO board definition lists USB hwid 0x1A86:0x55D4 (WCH CH9102). The vendor README links both WCH and Silicon Labs drivers. | Wrong driver or no upload port on the host. | Check the chip on the unit or its USB VID:PID; install the matching driver if the OS lacks one. | yes, 2026-09-22: the unit in hand enumerates as 10c4:ea60 (CP2104); upload works with `--upload-port /dev/ttyUSB0`. |
| Q-002 | The vendor README says their bundled TFT_eSPI compiles only up to arduino-esp32 2.0.14. The installed PlatformIO espressif32 7.0.1 ships framework-arduinoespressif32 3.20017 (arduino-esp32 2.0.17). | Possible build errors with the vendor library copy. | Use upstream TFT_eSPI 2.5.43 from the PlatformIO registry, not the vendor copy (PROJECT.md D-013). Builds with espressif32 7.0.1. | yes, 2026-09-22 (build and display output with 0.0.1) |
| Q-003 | GPIO0 is BUTTON2 and a strapping pin. Held LOW during reset, the chip enters download mode. | Holding that button while powering on or resetting stops normal boot. | Do not rely on GPIO0 being held at boot. GPIO0 is used only for short presses (scroll); the hold action (clear all) is on GPIO35 (PROJECT.md D-007). | no |
| Q-004 | Battery voltage divider on GPIO34 is enabled by ADC_EN (GPIO14). Per the factory test comment, it is on by default with USB power, but GPIO14 must be driven HIGH on battery. | Battery reads wrong when GPIO14 is not HIGH. | Drive GPIO14 HIGH before sampling GPIO34. | no |
| Q-005 | arduino-esp32 2.0.17 has a weak `btInUse()` returning false; the strong one (true) is only linked with the core's BT helpers, which `WiFiProv` does not use. `initArduino()` then releases the BLE controller memory at boot. Seen as `bt_mem_release of classic BT failed 259` and `... BTDM failed 259` at boot; the built firmware's `btInUse` disassembled to `movi a2, 0`. | BLE provisioning cannot start on an unprovisioned board; with saved settings the BLE memory is never returned to the heap (153,084 bytes free instead of 164,836). | Define `extern "C" bool btInUse() { return true; }` in the firmware (`src/network.cpp`). | yes, 2026-09-23: errors gone, `btInUse` returns 1, heap up by 11.7 KB. The setup path itself is not yet tested. |

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

- **Flash command:** `pio run -e tdisplay -t upload`
- **Expected boot serial output:** after the ROM boot log (see UART, Boot-time noise), observed with 0.0.3 on 2026-09-22:

```
ScreenAPI v0.0.3                                              (0.2 s after reset)
Queue: 5 messages                                             (2.3 s)
Free heap: 295564 bytes, largest block: 110580 bytes          (2.3 s)
```

- **Serial capture without a terminal program:** opening the port can leave the chip in download mode (`boot:0x3 ... waiting for download`). Release DTR and RTS, then pulse RTS (EN) for 100 ms to reset into a normal boot.
- **Smoke test:** backlight on; "ScreenAPI" and the version centered in landscape, not shifted, clipped, or mirrored (F-010)
- **Known-good firmware version:** 0.0.3 (2026-09-23)

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
