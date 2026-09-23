# PROJECT.md

Project and feature documentation. Board hardware details live in BOARDS.md; change history lives in CHANGELOG.md.

## Purpose and scope

- **Purpose:** A small desk display on the local network. The ESP32 joins local Wi-Fi and hosts an MCP (Model Context Protocol) server. MCP clients send messages to it, and the device shows them on its screen. Example: Claude Code sends its working status, and the user sees it on the desk.
- **In scope:**
  - Wi-Fi station connection with a DHCP address. Credentials are provisioned with the Espressif ESP BLE Provisioning app (F-002).
  - MCP server on the device, used only to pass message data in (F-003).
  - Message queue of up to 30 messages, persisted across reboot, with lifecycle handled on the device: time-driven messages expire, and confirm-required messages stay until deleted with a button (F-004).
  - Message screen: a title on top and the message value in a large text area below. The value uses one of two font sizes and one of four text colors: white, blue, green, red. A title wider than the screen scrolls horizontally; a value taller than the text area scrolls vertically (F-005).
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
| Display / UI | Boot screen, welcome screen, top bar, message screen, queue-full popup, buffered rendering (F-005, F-007, F-009, F-010) | `src/screen.cpp` |
| UI logic | Word wrap, auto-scroll timing, button debounce and long press; hardware-independent (F-005, F-006) | `lib/ui_logic/` |
| Buttons | Reading the pins and acting on button events (F-006) | `src/main.cpp` |
| Main loop | Buttons, expiry, screen update; demo messages (F-011) | `src/main.cpp` |
| Time | NTP sync, timezone lookup by IP (F-008) | TBD |
| Battery | Voltage reading for the top bar (F-007) | TBD |

`lib/message_queue/` and `lib/ui_logic/` stay hardware-independent so they run in the native test environment (AGENTS.md, Engineering rules).

### Data flow

- MCP client (for example Claude Code) -> HTTP over LAN -> MCP server on the device -> message queue -> display. If the queue is full, the message is dropped, the device shows a popup, and the MCP response tells the client the queue is full.
- Buttons -> message queue (delete, clear all, scroll) -> display.
- Timer -> message queue (expired time-driven messages are removed) -> display.
- Message queue <-> flash storage (persisted across reboot).
- Wi-Fi connect (DHCP) -> welcome screen with IP, IP message added to the queue, top bar IP.
- Internet -> NTP time and IP-based timezone -> top bar clock.

### Screen layout

Landscape (rotation 1), 240 x 135 pixels, black background. Implemented in `src/screen.cpp` since 0.0.3.

| Area | Rows (y) | Content |
|------|----------|---------|
| Top bar | 0 to 17, line at 18 | IP (left), position / count such as `2/5` (center), battery and clock (right); font 2, light grey (F-007) |
| Title | 21 to 36, line at 39 | Message title, font 2, light grey; scrolls horizontally when wider than 232 px (F-005) |
| Value | 41 to 134 (94 px) | Message value in the sender's font and color, word-wrapped to 232 px; scrolls vertically when taller than the area (F-005) |

Left and right margins are 4 px.

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
- **Status:** In progress (queue logic in 0.0.2, used by the firmware since 0.0.3; persistence pending)
- **Added in version:** 0.0.2
- **Description:** The device owns message queuing, display order, and lifecycle. Two message kinds:
  - Time-driven: removed automatically when its time runs out.
  - Confirm-required: stays until the user deletes it with the delete button.
- **Source files:** `lib/message_queue/src/message_queue.h`, `lib/message_queue/src/message_queue.cpp`, `test/test_message_queue/test_main.cpp`
- **Behavior:**
  - Capacity: 30 messages (D-008). Fixed slots, no heap: 616 bytes per message, 18,488 bytes for the whole queue on the ESP32 (measured with the xtensa toolchain, 0.0.3).
  - Validation (D-016): value required; id up to 16, title up to 64, value up to 512 characters; valid font size, color, and kind; time-driven duration 1 to 86400 seconds. Invalid input is rejected, not truncated. Confirm-required messages ignore the duration.
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
- **Status:** In progress (message screen in 0.0.3; queue-full popup pending with F-003)
- **Added in version:** 0.0.3
- **Description:** The message screen shows the message title on top and the message value in a large text area below, under the top bar (F-007). The title has one fixed style. The value uses the font size (one of two) and color (white, blue, green, red) chosen by the sender (D-004). Long text scrolls automatically (D-018). When the queue is full, a popup says so.
- **Source files:** `src/screen.cpp`, `src/screen.h`, `lib/ui_logic/src/text_wrap.*`, `lib/ui_logic/src/scroll_offset.*`
- **Behavior:**
  - Layout: see Architecture, Screen layout.
  - Fonts: small = TFT_eSPI font 2 (16 px line height), large = font 4 (26 px). Title: font 2, light grey.
  - Colors: white, blue (0x4C9F, lighter than TFT_BLUE for readability on black), green, red.
  - Word wrap at spaces; a word wider than the line is broken; `\n` starts a new line.
  - Auto-scroll (D-018): a title wider than 232 px scrolls horizontally at 40 px/s; a value taller than 94 px scrolls vertically at 20 px/s. Each pauses 1.5 s at the start, moves to the end, pauses 1.5 s, then jumps back. Scrolling restarts only when the shown text or font changes.
  - Empty queue: "No messages" centered in the value area.
  - Rendering (D-019): each frame is drawn into one full-screen 8-bit sprite and pushed at once. Redraws happen on queue changes and every 33 ms only while something scrolls.
  - TBD: queue-full popup text and duration (with F-003), handling of non-ASCII characters.
- **Verification:** `pio test -e native` covers word wrap and scroll timing. On device (0.0.3, 2026-09-22): serial log confirms boot and demo messages; the user checked the screen on 2026-09-23 and reported it looks good (title scrolls sideways, values scroll down, no flicker, text not clipped).

### F-006 - Buttons: delete, clear all, scroll

- **Area:** Input
- **Status:** Done
- **Added in version:** 0.0.3
- **Description:** Button mapping (D-007):
  - GPIO35 (delete): short press deletes the currently shown message; hold clears all messages.
  - GPIO0 (scroll): short press shows the next queued message, wrapping from last to first.
- **Source files:** `src/main.cpp`, `lib/ui_logic/src/button_tracker.*`
- **Behavior:**
  - Both buttons are active LOW, debounced for 30 ms.
  - Delete: short press fires on release and removes the shown message of either kind (F-004). Holding for 1.5 s clears all messages; it fires while still held, without a confirmation step, and the release after it does nothing.
  - Scroll: short press shows the next older message, wrapping to the newest; does nothing with fewer than two messages. Long press has no action.
  - Each action is logged on serial: `Deleted message, N left`, `Cleared all messages`, `Showing message N of M`.
- **Verification:** `pio test -e native` covers debounce, short press, and long press. On device, 2026-09-23: the user checked delete, hold to clear all, and scroll with 0.0.3 and reported it looks good.

### F-007 - Top status bar

- **Area:** Display
- **Status:** In progress (layout and queue position in 0.0.3)
- **Added in version:** 0.0.3
- **Description:** A bar across the top of the screen shows the IP address, queue depth, battery, and clock (F-008).
- **Source files:** `src/screen.cpp`
- **Behavior:** Center shows the shown message's position and the queue count, such as `2/5` (`0/0` when empty). IP (`no network`), battery (`--%`), and clock (`--:--`) are placeholders until F-002, the battery reading, and F-008 exist. TBD: battery format (voltage, percent, or icon) and what shows on USB power without a battery, what shows before Wi-Fi connects and before the clock is set, update intervals.
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
- **Source files:** `src/main.cpp`, `src/screen.cpp`, `include/pins.h`, `include/tft_setup.h`, `platformio.ini`
- **Behavior:** On boot, prints `ScreenAPI v<FW_VERSION>` on serial at 115200 baud. The screen, in landscape (rotation 1), shows "ScreenAPI" (font 4) and "v<FW_VERSION>" (font 2) centered, white on black, with the backlight on. Since 0.0.3 the boot screen stays for 1.5 s, then the message screen (F-005) follows.
- **Verification:** `pio run -e tdisplay` succeeds. On device, 2026-09-22 (reported by the user): serial shows `ScreenAPI v0.0.1` after the ROM boot log (BOARDS.md, On-device verification); the screen works.

### F-011 - Demo messages (temporary)

- **Area:** Test
- **Status:** In progress (temporary; remove when messages arrive over MCP, F-003)
- **Added in version:** 0.0.3
- **Description:** Five sample messages added at boot so the message screen, scrolling, buttons, and expiry can be tried before MCP exists.
- **Source files:** `src/main.cpp` (`addDemoMessages`)
- **Behavior:** Shown first: a confirm message with a title and value long enough to scroll both ways. Then: a blue small-font confirm message; a green large-font message that expires after 30 s; a red large-font confirm message long enough to scroll; a white message that expires after 60 s. Serial prints `Queue: 5 messages` and the free heap after boot.
- **Verification:** On device, 2026-09-22: serial shows `Queue: 5 messages` at 2.3 s and `Timed message expired` at 32.3 s and 62.3 s after reset.

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
| D-016 | Limits: id 16, title 64, value 512 characters (raised from 30 and 160 in 0.0.3); time-driven duration 1 to 86400 seconds. Input outside the limits is rejected, not truncated. | The user asked for longer titles and values, with scrolling (D-018), and left the numbers open. 30 messages at these limits take 18,488 bytes. Rejecting lets the MCP client resend a shorter message instead of showing cut-off text. | 2026-09-22 |
| D-017 | After a reboot, time-driven messages restart their full duration. | User decision. Needs no clock and no extra flash writes. | 2026-09-22 |
| D-018 | Long text scrolls automatically: the title horizontally, the value vertically. | User decision. Both buttons are already used (D-005), so scrolling needs no input. | 2026-09-22 |
| D-019 | The screen is drawn into one full-screen 8-bit sprite (240 x 135, 32,400 bytes) and pushed at once. | Flicker-free redraws (AGENTS.md). 8-bit halves the RAM of a 16-bit buffer, and the four text colors and greys survive the reduction. | 2026-09-22 |

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
- Memory budget: BLE provisioning, Wi-Fi, and an HTTP MCP server still have to fit on an ESP32 without PSRAM. Baseline with 0.0.3 (queue 18,488 bytes, 8-bit screen buffer 32,400 bytes): free heap 295,564 bytes, largest free block 110,580 bytes after boot. Re-check as each feature lands.
- Character set: the built-in TFT_eSPI fonts cover ASCII only. How non-ASCII text (accents, emoji) from MCP clients is handled is TBD (F-003, F-005). Newlines are supported since 0.0.3.
- `esp_app_desc` does not carry FW_VERSION. The built image reports project name `arduino-lib-builder` and app version `esp-idf: v4.4.7 38eeba213a`, from the precompiled Arduino core. This does not meet the AGENTS.md rule that esp_app_desc reads FW_VERSION. Options (override the descriptor, or accept it for the Arduino framework) TBD.
