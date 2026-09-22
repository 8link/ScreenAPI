# BOARDS.md

Hardware reference. Record board specifications, quirks, and usage findings when observed, including negative results.

## Board index

| Board | Revision | MCU | PlatformIO env | Status |
|-------|----------|-----|----------------|--------|
| TBD | TBD | TBD | TBD | TBD |

No board has been selected yet. Copy the template below once per board.

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
