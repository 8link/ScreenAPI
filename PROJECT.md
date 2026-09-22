# PROJECT.md

Project and feature documentation. Board hardware details live in BOARDS.md; change history lives in CHANGELOG.md.

## Purpose and scope

- **Purpose:** A small desk display on the local network. The ESP32 joins local Wi-Fi and hosts an MCP (Model Context Protocol) server. MCP clients send messages to it, and the device shows them on its screen. Example: Claude Code sends its working status, and the user sees it on the desk.
- **In scope:**
  - Wi-Fi station connection with a DHCP address. Credentials are provisioned with the Espressif ESP BLE Provisioning app (F-002).
  - MCP server on the device, used only to pass message data in (F-003).
  - Message queue of up to 30 messages, persisted across reboot, with lifecycle handled on the device: time-driven messages expire, and confirm-required messages stay until deleted with a button (F-004).
  - Message screen: a title on top and the message value in a large text area below. The value uses one of two font sizes and one of four text colors: white, blue, green, red (F-005).
  - Two buttons: delete (short press deletes the shown message, hold clears all) and scroll (loops through queued messages) (F-006).
  - Top status bar: IP address, queue depth, battery, clock (F-007).
  - Clock from NTP, with the timezone derived from the device's public IP (F-008).
  - Welcome screen showing the IP address; the IP is also added to the queue as a message (F-009).
- **Out of scope:**
  - MCP is not used for device control or configuration beyond passing message data.
  - Wi-Fi credentials are not entered over MCP or hardcoded in firmware.
  - Authentication on the MCP server (D-011).
- **Current firmware version:** see the header of CHANGELOG.md (mirrors `FW_VERSION` in platformio.ini once it exists).

## Architecture

### Runtime model

- **Framework:** Arduino (arduino-esp32) under PlatformIO (D-006).
- **Execution model (tasks / loop):** TBD
- **Core assignment:** TBD
- **Task priorities:** TBD
- **Tick rates / update intervals:** TBD

### Modules

Planned module boundaries follow the feature log. Names and paths are TBD until code exists.

| Module | Responsibility | Source path |
|--------|----------------|-------------|
| Wi-Fi / provisioning | BLE provisioning, station connect with DHCP, reconnect (F-002) | TBD |
| MCP server | HTTP transport, JSON-RPC, tool handling, validation, queue-full reporting (F-003) | TBD |
| Message queue | Up to 30 messages, validation, replace by id, expiry, delete, clear all, scroll position (F-004); persistence planned | `lib/message_queue/` |
| Display / UI | Boot screen, welcome screen, top bar, message screen, queue-full popup, buffered rendering (F-005, F-007, F-009, F-010) | `src/main.cpp` (F-010 only so far) |
| Buttons | Debounce, short press and hold detection (F-006) | TBD |
| Time | NTP sync, timezone lookup by IP (F-008) | TBD |
| Battery | Voltage reading for the top bar (F-007) | TBD |

The message queue should be hardware-independent so it can be tested in the native environment (AGENTS.md, Engineering rules).

### Data flow

- MCP client (for example Claude Code) -> HTTP over LAN -> MCP server on the device -> message queue -> display. If the queue is full, the message is dropped, the device shows a popup, and the MCP response tells the client the queue is full.
- Buttons -> message queue (delete, clear all, scroll) -> display.
- Timer -> message queue (expired time-driven messages are removed) -> display.
- Message queue <-> flash storage (persisted across reboot).
- Wi-Fi connect (DHCP) -> welcome screen with IP, IP message added to the queue, top bar IP.
- Internet -> NTP time and IP-based timezone -> top bar clock.

### Screen layout

Top bar (IP, queue depth, battery, clock) above the message area. The message area shows the title on top and the value in a large text box below. Orientation, pixel layout, and fonts are TBD.

### Planned repository layout

```
platformio.ini   PlatformIO environments; shared [env] defines FW_VERSION
include/         Headers: pins.h (single pin header), tft_setup.h (TFT_eSPI configuration)
src/             Firmware sources
lib/             Project-local libraries
test/            Unit tests (native env for hardware-independent logic)
```

platformio.ini, include/, and src/ exist since 0.0.1; lib/ and test/ since 0.0.2.

## Supported boards

Details for each board live in BOARDS.md.

| Board | MCU | PlatformIO env | Status | Details |
|-------|-----|----------------|--------|---------|
| LilyGO TTGO T-Display | ESP32 (dual-core Xtensa LX6) | `tdisplay` (board id `lilygo-t-display`) | Brought up with 0.0.1 (verified on device 2026-09-22) | BOARDS.md "LilyGO TTGO T-Display" |

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
- **Description:** The device connects to the local Wi-Fi as a station and gets its IP address from DHCP. Credentials are provisioned with the Espressif ESP BLE Provisioning app and stored on the device.
- **Source files:** TBD
- **Behavior:** TBD: first-boot flow, what the screen shows while provisioning, reconnect behavior, how to re-provision or reset credentials.
- **Verification:** TBD

### F-003 - MCP server on device

- **Area:** Networking
- **Status:** Planned
- **Added in version:** not yet implemented
- **Description:** The device hosts an MCP server on the local network, with no authentication (D-011). MCP is only for passing data in: clients submit messages to display. Clients such as Claude Code connect directly to the device's IP address.
- **Source files:** TBD
- **Behavior:**
  - A message has an optional id, a title, a value, a value font size (one of two), a value color (white, blue, green, red), a kind (time-driven or confirm-required), and for time-driven messages a duration in seconds. Limits are in D-016; the queue rejects input outside them (F-004).
  - A message with the same id as a queued one replaces it (D-015).
  - When the queue is full, the message is dropped and the response tells the client the queue is full, so an LLM caller knows its message was not shown.
  - TBD: MCP transport and protocol version, tool names, field names, how invalid input is reported to the client (the response should state the limits).
- **Verification:** TBD

### F-004 - Message queue and lifecycle

- **Area:** Core logic
- **Status:** In progress (queue logic done in 0.0.2; persistence and firmware integration pending)
- **Added in version:** 0.0.2
- **Description:** The device owns message queuing, display order, and lifecycle. Two message kinds:
  - Time-driven: removed automatically when its time runs out.
  - Confirm-required: stays until the user deletes it with the delete button.
- **Source files:** `lib/message_queue/src/message_queue.h`, `lib/message_queue/src/message_queue.cpp`, `test/test_message_queue/test_main.cpp`
- **Behavior:**
  - Capacity: 30 messages (D-008). Fixed slots, no heap: 224 bytes per message, 6,728 bytes for the whole queue on the ESP32 (measured with the xtensa toolchain).
  - Validation (D-016): value required; id up to 16, title up to 30, value up to 160 characters; valid font size, color, and kind; time-driven duration 1 to 86400 seconds. Invalid input is rejected, not truncated. Confirm-required messages ignore the duration.
  - Order (D-014): newest first. Adding a message moves the cursor (the shown message) to it.
  - Replace by id (D-015): a message with the same non-empty id as a queued one replaces it and moves to the front. Replacing works even when the queue is full.
  - Full queue: new messages without a matching id are dropped and existing messages are kept. The device shows a popup (F-005) and the MCP response reports it (F-003).
  - Scroll: shows the next older message, wrapping from the oldest to the newest.
  - Delete: removes the shown message of either kind, then shows the next older one (wrapping to the newest). Clear all removes every message (F-006).
  - Expiry: time-driven messages are removed once the current time reaches added time + duration. When other messages are removed, the cursor stays on the message being shown.
  - Time is passed in by the caller in milliseconds as a 64-bit value. The firmware must use a 64-bit uptime clock, not the 32-bit `millis()`, which wraps after about 49.7 days.
  - Persistence (D-009, D-017), planned: messages survive reboot; time-driven messages restart their full duration after a reboot. Storage medium and write strategy TBD.
- **Verification:** `pio test -e native`: 18 unit tests covering validation, limits, order, scroll, full queue, replace by id, delete, clear, and expiry with cursor handling.

### F-005 - Message screen and rendering

- **Area:** Display
- **Status:** Planned
- **Added in version:** not yet implemented
- **Description:** The message screen shows the message title on top and the message value in a large text area below, under the top bar (F-007). The title has one fixed style. The value uses the font size (one of two) and color (white, blue, green, red) chosen by the sender (D-004). When the queue is full, a popup says so.
- **Source files:** TBD
- **Behavior:** TBD: fonts, background color, orientation, wrapping and truncation, scroll position indicator, popup text and duration, what shows when the queue is empty. Refreshes use buffered (off-screen) rendering (AGENTS.md).
- **Verification:** TBD

### F-006 - Buttons: delete, clear all, scroll

- **Area:** Input
- **Status:** Planned
- **Added in version:** not yet implemented
- **Description:** Button mapping (D-007):
  - GPIO35 (delete): short press deletes the currently shown message; hold clears all messages.
  - GPIO0 (scroll): short press shows the next queued message, wrapping from last to first.
- **Source files:** TBD
- **Behavior:** Delete removes messages of either kind (F-004). TBD: hold duration for clear all, whether clear all asks for confirmation.
- **Verification:** TBD

### F-007 - Top status bar

- **Area:** Display
- **Status:** Planned
- **Added in version:** not yet implemented
- **Description:** A bar across the top of the screen shows the IP address, queue depth, battery, and clock (F-008).
- **Source files:** TBD
- **Behavior:** TBD: layout, battery format (voltage, percent, or icon) and what shows on USB power without a battery, what shows before Wi-Fi connects and before the clock is set, update intervals.
- **Verification:** TBD

### F-008 - Clock with NTP and IP-based timezone

- **Area:** Networking, time
- **Status:** Planned
- **Added in version:** not yet implemented
- **Description:** The device gets the time from NTP and the timezone from a geolocation lookup of its public IP address. The clock shows in the top bar (F-007). This needs internet access, not only the LAN.
- **Source files:** TBD
- **Behavior:** TBD: NTP servers, geolocation service, fallback when the lookup fails, resync interval, 12 or 24 hour format.
- **Verification:** TBD

### F-009 - Welcome screen and IP message

- **Area:** Display, Wi-Fi
- **Status:** Planned
- **Added in version:** not yet implemented
- **Description:** After connecting, the device shows a welcome screen with its DHCP IP address and adds a message with the IP address to the queue.
- **Source files:** TBD
- **Behavior:** TBD: how long the welcome screen stays, the kind of the IP message (time-driven or confirm-required), what happens on reconnect or IP change, what happens if the queue is full.
- **Verification:** TBD

### F-010 - Boot screen with firmware version

- **Area:** Display
- **Status:** Done
- **Added in version:** 0.0.1
- **Description:** First firmware. Brings up the toolchain, display driver, and board: prints the firmware version on serial and shows it on screen.
- **Source files:** `src/main.cpp`, `include/pins.h`, `include/tft_setup.h`, `platformio.ini`
- **Behavior:** On boot, prints `ScreenAPI v<FW_VERSION>` on serial at 115200 baud. The screen, in landscape (rotation 1), shows "ScreenAPI" (font 4) and "v<FW_VERSION>" (font 2) centered, white on black, with the backlight on. The loop idles.
- **Verification:** `pio run -e tdisplay` succeeds. On device, 2026-09-22 (reported by the user): serial shows `ScreenAPI v0.0.1` after the ROM boot log (BOARDS.md, On-device verification); the screen works.

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

Built-in 1.14 inch ST7789 panel, 135 x 240 (BOARDS.md). Top bar (F-007), message screen with title and value (F-005), welcome screen (F-009), queue-full popup (F-005). Two value font sizes, four value colors (D-004).

### BLE

Used for Wi-Fi provisioning with the ESP BLE Provisioning app (F-002). Whether BLE stays enabled after provisioning is TBD.

### Wi-Fi

Station mode on the local network, IP from DHCP (F-002). Credentials are never committed. They come from BLE provisioning and live on the device only. Any development-time credentials use `include/secrets.h` (gitignored) with a committed `secrets.h.example` placeholder. Internet access is needed for NTP and the timezone lookup (F-008).

### UART / serial

- **Log format:** TBD
- **Other serial usage:** TBD

### Storage

- **NVS keys:** TBD (Wi-Fi credentials from provisioning)
- **Message persistence:** up to 30 messages survive reboot (F-004); time-driven messages restart their full duration (D-017). Medium (NVS or LittleFS) TBD.
- **Filesystem:** TBD
- **Partitions:** TBD

### Power

Battery level shows in the top bar (F-007). TBD: USB or battery as the normal power source, sleep behavior.

### OTA

TBD

## Design decisions

Record decisions that a later change could accidentally undo.

| ID | Decision | Reason | Date |
|----|----------|--------|------|
| D-001 | The MCP server runs on the ESP32; clients connect directly to the device over the LAN. No PC-side bridge. | User decision. | 2026-09-22 |
| D-002 | Wi-Fi credentials are provisioned with the Espressif ESP BLE Provisioning app, not hardcoded or sent over MCP. | User decision. | 2026-09-22 |
| D-003 | MCP is only for passing message data. Display, queuing, and message lifecycle are handled on the device. | User decision. | 2026-09-22 |
| D-004 | A message is a title plus a value. The sender styles the value only: two font sizes and four colors (white, blue, green, red). The title has one fixed style. | User decision. | 2026-09-22 |
| D-005 | One button deletes the shown message; the other scrolls through messages in a loop. Mapping in D-007. | User decision. | 2026-09-22 |
| D-006 | Framework is Arduino (arduino-esp32) under PlatformIO. | User decision. The vendor display library (TFT_eSPI) and examples are Arduino. | 2026-09-22 |
| D-007 | GPIO35 is delete (hold = clear all); GPIO0 is scroll. | The user left the mapping open. GPIO0 is a strapping pin (BOARDS.md Q-003), so the button that gets held stays off it. | 2026-09-22 |
| D-008 | Queue holds 30 messages (changed from 100 by the user on 2026-09-22). When full, new messages are dropped (existing ones kept), a popup shows, and the MCP response reports it. | User decision. | 2026-09-22 |
| D-009 | Messages persist across reboot. | User decision. | 2026-09-22 |
| D-010 | IP address comes from DHCP. It shows on the welcome screen, in the top bar, and as a queued message. | User decision. | 2026-09-22 |
| D-011 | The MCP server has no password or token. Anyone on the LAN can post messages. | User decision; accepted risk. | 2026-09-22 |
| D-012 | Time from NTP; timezone from a lookup of the device's public IP. | User decision. | 2026-09-22 |
| D-013 | Pins live in `include/pins.h`. TFT_eSPI is upstream `bodmer/TFT_eSPI@2.5.43`, configured by `include/tft_setup.h` (which includes pins.h); library files stay unmodified. The `tdisplay` env needs `-Iinclude` in build_flags. | One pin header. Without `-Iinclude`, TFT_eSPI does not see tft_setup.h and silently compiles with its default ILI9341 setup while src/ uses ours; the build still succeeds (observed in a verbose build). | 2026-09-22 |
| D-014 | Newest message first; a new message is shown immediately. Scroll goes from newest to oldest and wraps. | User decision. | 2026-09-22 |
| D-015 | A message may carry an optional id. A new message with the same id replaces the old one and moves to the front, also when the queue is full. | User decision, so repeated status updates from one sender take one slot. | 2026-09-22 |
| D-016 | Limits: id 16, title 30, value 160 characters; time-driven duration 1 to 86400 seconds. Input outside the limits is rejected, not truncated. | The user left lengths open. Title: one small-font line; value: about a full screen of small font (about 6 lines of 30 characters, estimated, not measured). Rejecting lets the MCP client resend a shorter message instead of showing cut-off text. | 2026-09-22 |
| D-017 | After a reboot, time-driven messages restart their full duration. | User decision. Needs no clock and no extra flash writes. | 2026-09-22 |

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

Known unknowns and issues found outside the current task. Smaller per-feature details are listed as TBD in each feature entry.

- MCP: transport and protocol version, tool names, field names.
- Persistence: storage medium (NVS or LittleFS) and how often it writes, to limit flash wear.
- Timezone: which geolocation service to use, and the fallback when it fails or there is no internet.
- Memory budget (unverified): BLE provisioning, Wi-Fi, an HTTP MCP server, the message queue (6,728 bytes, measured), and a full-screen sprite (240 x 135 x 16 bit = 64,800 bytes) on an ESP32 without PSRAM. Check once the firmware uses them.
- Character set: the built-in TFT_eSPI fonts cover ASCII only. How non-ASCII text (accents, emoji) and newlines from MCP clients are handled is TBD (F-003, F-005).
- `esp_app_desc` does not carry FW_VERSION. The built image reports project name `arduino-lib-builder` and app version `esp-idf: v4.4.7 38eeba213a`, from the precompiled Arduino core. This does not meet the AGENTS.md rule that esp_app_desc reads FW_VERSION. Options (override the descriptor, or accept it for the Arduino framework) TBD.
