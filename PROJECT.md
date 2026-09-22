# PROJECT.md

Project and feature documentation. Board hardware details live in BOARDS.md; change history lives in CHANGELOG.md.

## Purpose and scope

- **Purpose:** A small desk display on the local network. The ESP32 joins local Wi-Fi and hosts an MCP (Model Context Protocol) server. MCP clients send messages to it, and the device shows them on its screen. Example: Claude Code sends its working status, and the user sees it on the desk.
- **In scope:**
  - Wi-Fi station connection. Credentials are provisioned with the Espressif ESP BLE Provisioning app (F-002).
  - MCP server on the device, used only to pass message data in (F-003).
  - Message queue and lifecycle handled on the device: time-driven messages expire, and confirm-required messages stay until deleted with a button (F-004).
  - Rendering: two font sizes, four text colors (white, blue, green, red) (F-005).
  - Two buttons: one confirms (deletes) the shown message, the other scrolls through queued messages in a loop (F-006).
- **Out of scope:**
  - MCP is not used for device control or configuration beyond passing message data.
  - Wi-Fi credentials are not entered over MCP or hardcoded in firmware.
- **Current firmware version:** see the header of CHANGELOG.md (mirrors `FW_VERSION` in platformio.ini once it exists).

## Architecture

### Runtime model

- **Execution model (tasks / loop):** TBD
- **Core assignment:** TBD
- **Task priorities:** TBD
- **Tick rates / update intervals:** TBD

### Modules

Planned module boundaries follow the feature log. Names and paths are TBD until code exists.

| Module | Responsibility | Source path |
|--------|----------------|-------------|
| Wi-Fi / provisioning | BLE provisioning, station connect, reconnect (F-002) | TBD |
| MCP server | HTTP transport, JSON-RPC, tool handling, validation (F-003) | TBD |
| Message queue | Storage, expiry, confirm, scroll position (F-004) | TBD |
| Display | Rendering with two font sizes and four colors, buffered (F-005) | TBD |
| Buttons | Debounce, confirm and scroll events (F-006) | TBD |

The message queue should be hardware-independent so it can be tested in the native environment (AGENTS.md, Engineering rules).

### Data flow

MCP client (for example Claude Code) -> HTTP over LAN -> MCP server on the device -> message queue -> display.
Buttons -> message queue (confirm deletes the shown message, scroll advances to the next one) -> display.
Timer -> message queue (expired time-driven messages are removed) -> display.

### Planned repository layout

```
platformio.ini   PlatformIO environments; shared [env] defines FW_VERSION
include/         Headers, including the single pin header (pins.h or config.h, TBD)
src/             Firmware sources
lib/             Project-local libraries
test/            Unit tests (native env for hardware-independent logic)
```

Not created yet. Created with the first firmware change.

## Supported boards

Details for each board live in BOARDS.md.

| Board | MCU | PlatformIO env | Status | Details |
|-------|-----|----------------|--------|---------|
| LilyGO TTGO T-Display | ESP32 (dual-core Xtensa LX6) | TBD (board id `lilygo-t-display`) | First target, not yet brought up | BOARDS.md "LilyGO TTGO T-Display" |

## Feature log

Stable IDs F-001, F-002, ... Reference them from code comments, CHANGELOG.md, and commits. Never renumber. Status: Planned / In progress / Done / Deprecated.

### F-001 - Project documentation scaffold

- **Area:** Documentation
- **Status:** Done
- **Added in version:** 0.0.0 (no firmware)
- **Description:** Initial documentation and repository scaffold: AGENTS.md, CLAUDE.md, PROJECT.md, BOARDS.md, CHANGELOG.md, .gitignore.
- **Source files:** none (documentation only)
- **Behavior:** No firmware behavior.
- **Verification:** Files present in the repository root; no build, no firmware yet.

### F-002 - Wi-Fi connection with BLE provisioning

- **Area:** Wi-Fi, BLE
- **Status:** Planned
- **Added in version:** not yet implemented
- **Description:** The device connects to the local Wi-Fi as a station. Credentials are provisioned with the Espressif ESP BLE Provisioning app and stored on the device.
- **Source files:** TBD
- **Behavior:** TBD: first-boot flow, what the screen shows while provisioning, reconnect behavior, how to re-provision or reset credentials.
- **Verification:** TBD

### F-003 - MCP server on device

- **Area:** Networking
- **Status:** Planned
- **Added in version:** not yet implemented
- **Description:** The device hosts an MCP server on the local network. MCP is only for passing data in: clients submit messages to display. Clients such as Claude Code connect directly to the device.
- **Source files:** TBD
- **Behavior:** TBD: MCP transport and protocol version, tool names and input schema, error responses, discovery (fixed IP / mDNS), authentication.
- **Verification:** TBD

### F-004 - Message queue and lifecycle

- **Area:** Core logic
- **Status:** Planned
- **Added in version:** not yet implemented
- **Description:** The device owns message queuing, display order, and lifecycle. Two message kinds:
  - Time-driven: removed automatically when its time runs out.
  - Confirm-required: stays until the user deletes it with the confirm button.
- **Source files:** TBD
- **Behavior:** TBD: queue capacity and overflow policy, ordering, whether a new message replaces an existing one (for example by id), which message is shown by default, persistence across reboot.
- **Verification:** TBD (planned: native unit tests, see Architecture).

### F-005 - Message rendering

- **Area:** Display
- **Status:** Planned
- **Added in version:** not yet implemented
- **Description:** Messages render in one of two font sizes and one of four text colors: white, blue, green, red. The sender chooses font size and color per message.
- **Source files:** TBD
- **Behavior:** TBD: fonts, background color, orientation, wrapping and truncation, queue position indicator. Refreshes use buffered (off-screen) rendering (AGENTS.md).
- **Verification:** TBD

### F-006 - Button confirm and scroll

- **Area:** Input
- **Status:** Planned
- **Added in version:** not yet implemented
- **Description:** One button confirms (deletes) the currently shown message. The other button scrolls through queued messages in a loop, wrapping from last to first.
- **Source files:** TBD
- **Behavior:** TBD: which physical button (GPIO0 or GPIO35) does which, whether confirm also deletes time-driven messages, whether a long press does anything.
- **Verification:** TBD

<!-- Template for new entries:

### F-XXX - Title

- **Area:**
- **Status:**
- **Added in version:**
- **Description:**
- **Source files:**
- **Behavior:**
- **Verification:**
-->

## Feature areas

### Sensors

None planned.

### Display

Built-in 1.14 inch ST7789 panel, 135 x 240 (BOARDS.md). Two font sizes, four text colors (F-005). Details TBD.

### BLE

Used for Wi-Fi provisioning with the ESP BLE Provisioning app (F-002). Whether BLE stays enabled after provisioning is TBD.

### Wi-Fi

Station mode on the local network (F-002). Credentials are never committed. They come from BLE provisioning and live on the device only. Any development-time credentials use `include/secrets.h` (gitignored) with a committed `secrets.h.example` placeholder.

### UART / serial

- **Log format:** TBD
- **Other serial usage:** TBD

### Storage

- **NVS keys:** TBD (Wi-Fi credentials from provisioning)
- **Filesystem:** TBD
- **Partitions:** TBD

### Power

TBD: USB or battery power, sleep behavior.

### OTA

TBD

## Design decisions

Record decisions that a later change could accidentally undo.

| ID | Decision | Reason | Date |
|----|----------|--------|------|
| D-001 | The MCP server runs on the ESP32; clients connect directly to the device over the LAN. No PC-side bridge. | User decision. | 2026-09-22 |
| D-002 | Wi-Fi credentials are provisioned with the Espressif ESP BLE Provisioning app, not hardcoded or sent over MCP. | User decision. | 2026-09-22 |
| D-003 | MCP is only for passing message data. Display, queuing, and message lifecycle are handled on the device. | User decision. | 2026-09-22 |
| D-004 | Text styling is limited to two font sizes and four colors: white, blue, green, red. | User decision. | 2026-09-22 |
| D-005 | One button confirms (deletes) the shown message; the other scrolls through messages in a loop. | User decision. | 2026-09-22 |

## Verification

Standard PlatformIO commands. Replace `<env>` with an environment from platformio.ini.

```
pio run -e <env>                     # build
pio run -e <env> -t upload           # build and flash
pio device monitor -e <env>          # serial monitor
pio test -e native                   # hardware-independent unit tests
pio test -e <env>                    # on-device tests
```

## Open questions

Known unknowns and issues found outside the current task.

- Pin header name not decided (include/pins.h or include/config.h).
- Button mapping: which of GPIO0 / GPIO35 confirms and which scrolls. Note: GPIO0 is a strapping pin; holding it during reset enters download mode (BOARDS.md).
- MCP details: transport and protocol version, tool names and input schema, discovery (fixed IP or mDNS), authentication on the LAN.
- Message schema: field names for text, font size, color, kind (time-driven / confirm-required), duration; maximum text length.
- Queue: capacity, overflow policy, ordering, replace-by-id, persistence across reboot.
- Provisioning: BLE stack choice, re-provisioning or credential reset method, whether BLE is released after provisioning.
- Memory budget (unverified): BLE provisioning, Wi-Fi, an HTTP MCP server, and a full-screen sprite (240 x 135 x 16 bit = 64,800 bytes) on an ESP32 without PSRAM. Check once code exists.
- Framework and TFT_eSPI version: vendor README says their bundled TFT_eSPI compiles only up to arduino-esp32 2.0.14; the installed PlatformIO platform ships a newer core (BOARDS.md Q-002).
- Framework choice: Arduino or ESP-IDF (both supported by the PlatformIO board id).
