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
| Wi-Fi / provisioning | BLE provisioning, station connect with DHCP, reconnect, Wi-Fi reset (F-002) | `src/network.cpp` |
| MCP server | HTTP endpoint, Origin check, mDNS (F-003) | `src/mcp_server.cpp` |
| MCP protocol | JSON-RPC, tools, validation, text cleanup; hardware-independent (F-003) | `lib/mcp_protocol/` |
| Message queue | Up to 30 messages, validation, replace by id, expiry, delete, clear all, scroll position (F-004); persistence planned | `lib/message_queue/` |
| Display / UI | Boot screen, welcome screen, top bar, message screen, queue-full popup, buffered rendering (F-005, F-007, F-009, F-010) | `src/screen.cpp` |
| UI logic | Word wrap, auto-scroll timing, button debounce and long press; hardware-independent (F-005, F-006) | `lib/ui_logic/` |
| Buttons | Reading the pins and acting on button events (F-006) | `src/main.cpp` |
| Main loop | Buttons, expiry, MCP events, connection announcement, screen update | `src/main.cpp` |
| Storage | Mount LittleFS, load the queue at boot, save it after changes (F-004) | `src/storage.cpp`, `lib/message_queue/src/queue_codec.*` |
| Time | NTP sync, timezone lookup by IP (F-008) | `src/clock.cpp`, `lib/clock_logic/` |
| Board hardware | Per-board pins, display, fonts, inputs, battery (D-032) | `src/hal.h`, `src/boards/<board>/hal.cpp`, `lib/ui_logic/src/battery_level.*`, `lib/sh8601_display/` |

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
| Top bar | 0 to 19 | Dark bar with pills, right to left: clock, battery icon, queue position (amber, bold), network with status stripe (F-007, D-028) |
| Title | 23 to 38, line at 41 | Message title, font 2, light grey; scrolls horizontally when wider than 232 px (F-005) |
| Value | 43 to 134 (92 px) | Message value in the sender's font and color, word-wrapped to 232 px; scrolls vertically when taller than the area (F-005) |

Left and right margins are 4 px.

### Planned repository layout

```
platformio.ini   PlatformIO environments; one env per board
VERSION          Firmware version, passed to src/ as FW_VERSION by tools/version.py (D-035)
include/boards/<board>/
                 board.h (name, screen size, rotation, mDNS hostname), pins.h (the board's pins); since 0.0.14 (D-030)
src/             Firmware sources; src/hal.h with src/boards/<board>/hal.cpp per board since 0.0.16 (D-032);
                 src/boards/template/ is the porting starting point, built by the template env (0.0.17)
tools/           screenshot.py (F-012), version.py (D-035)
README.md        Project overview for GitHub
lib/             Project-local libraries
test/            Unit tests (native env for hardware-independent logic)
```

platformio.ini, include/, and src/ exist since 0.0.1; lib/ and test/ since 0.0.2; tools/ since 0.0.11.

### Adding a board

Since 0.0.16 each board has two folders (D-030, D-032). To add one:

1. Copy `include/boards/template/` and `src/boards/template/` (since 0.0.17; every value to change is marked PORT). `board.h`: name, screen size after rotation, rotation, mDNS hostname; `pins.h`: the board's pins.
2. `src/boards/<board>/hal.cpp` implementing `src/hal.h`: `begin()` (pins, buses, power), `description()`, `setSerialBlocking()`, `display()` (Arduino_GFX bus and panel), `displaySpeedHz()`, `fonts()` (four u8g2 fonts), `deletePressed()`, `scrollPressed()` (a button or the touchscreen), `hasBattery()`, `readBattery()`.
3. `[env:<board>]` in platformio.ini, copied from the `template` env: `extends = esp32`, `board`, `board_build.partitions`, extra `lib_deps` if needed, `build_flags = ${env.build_flags} -Iinclude/boards/<board>`, and `build_src_filter = +<*> -<boards/> +<boards/<board>/>`.
4. Build with `pio run -e <board> -j 2` (U8g2's font source needs about 1 GB per compiler job).
5. Add a BOARDS.md section from the template and a row in Supported boards.
6. Flash, then check the boot lines (`ScreenAPI vX.Y.Z on <board name>`, `Hardware: ...`), `tools/screenshot.py --port <port>`, both inputs, and `B` for the battery.

Limits of the current code: the screen must be at least 240 x 135 after rotation (compile-time check); two inputs (delete and scroll; the scroll input can be the touchscreen); the 8-bit frame buffer takes width x height bytes (internal RAM without PSRAM, about 94 KB largest free block on the T-Display); BLE provisioning needs BLE (not the ESP32-S2); the firmware needs an app slot of about 1.8 MB; the `btInUse()` override (Q-005) is for the classic ESP32 and harmless elsewhere. Row heights come from the board's fonts; setup and welcome screens are centered; on portrait screens the setup QR code sits above the text.

## Supported boards

Details for each board live in BOARDS.md.

| Board | MCU | PlatformIO env | Status | Details |
|-------|-----|----------------|--------|---------|
| LilyGO TTGO T-Display | ESP32 (dual-core Xtensa LX6) | `tdisplay` (board id `lilygo-t-display`) | Brought up with 0.0.1 (verified on device 2026-09-22) | BOARDS.md "LilyGO TTGO T-Display" |
| Waveshare ESP32-S3-Touch-AMOLED-1.8 | ESP32-S3 (dual-core Xtensa LX7), 8 MB PSRAM | `waveshare_amoled18` (board id `esp32-s3-devkitc-1`) | Boots with 0.0.16 (V2 revision); display, touch, and Wi-Fi not yet checked by eye | BOARDS.md "Waveshare ESP32-S3-Touch-AMOLED-1.8" |

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
- **Status:** In progress (implemented in 0.0.7; connecting with saved settings verified; the setup path not yet tested on the device)
- **Added in version:** 0.0.7
- **Description:** The device connects to the local Wi-Fi as a station and gets its IP address from DHCP. Credentials are provisioned with the Espressif ESP BLE Provisioning app and stored on the device (D-002, D-023).
- **Source files:** `src/network.cpp`, `src/network.h`, `src/screen.cpp` (`showSetup`, `showNotice`), `src/main.cpp`
- **Behavior:**
  - Wi-Fi starts during the boot screen through the Arduino `WiFiProv` wrapper: BLE scheme, security 1, BLE memory released when setup ends or is not needed (`WIFI_PROV_SCHEME_HANDLER_FREE_BTDM`).
  - Saved settings present: connect; the message screen shows `connecting`, then the IP in the top bar (F-007).
  - No saved settings: full-screen setup screen until provisioned (D-023). Left: QR code with the app payload `{"ver":"v1","name":"PROV_XXXXXX","pop":"<code>","transport":"ble"}`. Right: "Wi-Fi setup", "ESP BLE Provisioning app", the device name `PROV_` + last 3 MAC bytes, `Code <code>`, and a status line: `Waiting for app` (grey), `Connecting...` (green), `Wrong password` or `Network not found` (red; retry from the app). The code is 8 random characters without look-alikes (no 0/o, 1/l/i), new each boot while unprovisioned. Timed messages do not count down while the setup screen is shown.
  - Network lost: message screen with `no network`; the device retries every 15 s in addition to the core's auto-reconnect.
  - Wi-Fi reset: hold both buttons for 5 s (F-006). The screen shows "Wi-Fi reset, restarting"; the saved network is erased and the device restarts into setup.
  - Serial: `Wi-Fi setup: ...` lines for the setup steps, `Wi-Fi: connected to <SSID>, IP <ip>`, `Wi-Fi: disconnected, reason N`, `Wi-Fi: reconnecting`.
- **Verification:** On device, 2026-09-23 (0.0.7): the board had saved settings from before this project (BOARDS.md Wi-Fi); it connected to `<SSID>` and got <device IP> about 0.9 s after boot; BLE memory was released (free heap 164,836 bytes). Not yet tested: the setup screen, pairing with the app, wrong-password handling, and the Wi-Fi reset.

### F-003 - MCP server on device

- **Area:** Networking
- **Status:** In progress (implemented in 0.0.8; verified over the LAN with raw HTTP requests and a Claude Code connection check; no tool call from Claude Code yet)
- **Added in version:** 0.0.8
- **Description:** The device hosts an MCP server on the local network, with no authentication (D-011). MCP is only for passing data in: clients submit messages to display. Clients such as Claude Code connect directly to the device (D-001, D-025).
- **Source files:** `src/mcp_server.*`, `lib/mcp_protocol/src/mcp_handler.*`, `lib/mcp_protocol/src/text_clean.*`, `test/test_mcp_protocol/test_main.cpp`
- **Behavior:**
  - Endpoint: `POST http://<device-ip>/mcp` or `http://screenapi.local/mcp` (mDNS), port 80, started once Wi-Fi is connected. Streamable HTTP transport without streaming: every request gets one `application/json` response; notifications and client responses get 202 with no body. GET and DELETE get 405. No sessions (no `Mcp-Session-Id`).
  - Methods: `initialize` (answers the client's protocol version if it is one of 2025-11-25, 2025-06-18, 2025-03-26, 2024-11-05, otherwise 2025-11-25; capabilities: tools; server name `ScreenAPI`, version FW_VERSION; usage instructions), `ping`, `tools/list`, `tools/call`. Other methods: JSON-RPC error -32601. Batches (JSON arrays), invalid JSON, or a body over 8 KB: HTTP 400 or 413 with a JSON-RPC error.
  - Tool `show_message`: `value` (required), `title`, `color` (white, blue, green, red; default white), `font_size` (small, large; default small), `kind` (timed, confirm; default timed if `duration_s` is given, otherwise confirm), `duration_s` (whole seconds 1 to 86400), `id`. Result text: `Shown on the display. Queue: N of 30 messages.` or `Replaced the message with id "x" and showed it. ...`.
  - Tool `queue_status`: `Queue: N of 30 messages.`
  - Tool errors come back as a tool result with `isError: true` and a sentence the LLM can act on, for example `color must be one of: white blue green red.` or `value is 513 characters; the limit is 512.` Unknown tool: JSON-RPC error -32602.
  - A message has an optional id, a title, a value, a value font size (one of two), a value color (white, blue, green, red), a kind (time-driven or confirm-required), and for time-driven messages a duration in seconds. Parts of the value can be colored with inline tags (D-021); tags count toward the value limit. Limits are in D-016; the queue rejects input outside them (F-004).
  - A message with the same id as a queued one replaces it (D-015).
  - When the queue is full, the message is dropped, the device shows the queue-full popup (F-005), and the result is an error: `Queue full (30 of 30): the message was dropped and not shown. ...`, so an LLM caller knows its message was not shown.
  - Text cleanup (D-026): title, value, and id are converted to what the ASCII fonts can draw before the limits are checked. The result reports how many characters became '?'.
  - Origin check: a request with an `Origin` header other than `http://<device-ip>` or `http://screenapi.local` gets 403, so web pages cannot post to the device (DNS rebinding protection required by the MCP spec). Claude Code sends no Origin.
  - Serial: `MCP: <method or tool>: <result>` per request.
  - Connect Claude Code: `claude mcp add --transport http screen http://screenapi.local/mcp` (or the IP address). On Linux, `.local` names need an mDNS resolver: `apt-get install avahi-daemon libnss-mdns` (adds `mdns4_minimal` to the `hosts` line in `/etc/nsswitch.conf`). macOS and Windows resolve `.local` without extra setup.
- **Verification:** `pio test -e native`: 16 tests in `test_mcp_protocol` (initialize and version negotiation, notifications, ping, tools/list, protocol errors, all show_message fields and defaults, replace by id, validation errors, cleanup and limits, full queue, queue_status, text cleanup). On device, 2026-09-23 (0.0.8), from a machine on the same LAN: initialize 175 ms, show_message 78 ms, tools/list, queue_status, validation error, GET 405, foreign Origin 403; mDNS query for `screenapi.local` answered with <device IP>. Claude Code 2.1.280 on the development machine (Debian 13, after installing avahi-daemon and libnss-mdns), 2026-09-23: `claude mcp list` reports `screen: http://screenapi.local/mcp (HTTP) - Connected`. Not yet: a tool call from a Claude Code session; the full-queue path on the device.

### F-004 - Message queue and lifecycle

- **Area:** Core logic
- **Status:** Done (queue logic in 0.0.2, used by the firmware since 0.0.3, persistence since 0.0.6)
- **Added in version:** 0.0.2
- **Description:** The device owns message queuing, display order, and lifecycle. Two message kinds:
  - Time-driven: removed automatically when its time on screen runs out (D-020).
  - Confirm-required: stays until the user deletes it with the delete button.
- **Source files:** `lib/message_queue/src/message_queue.*`, `lib/message_queue/src/queue_codec.*`, `src/storage.*`, `test/test_message_queue/test_main.cpp`, `test/test_queue_codec/test_main.cpp`
- **Behavior:**
  - Capacity: 30 messages (D-008). Fixed slots, no heap: 608 bytes per message, 18,264 bytes for the whole queue on the ESP32 (measured with the xtensa toolchain, 0.0.4).
  - Validation (D-016): value required; id up to 16, title up to 64, value up to 512 characters; valid font size, color, and kind; time-driven duration 1 to 86400 seconds. Invalid input is rejected, not truncated. Confirm-required messages ignore the duration.
  - Order (D-014): newest first. Adding a message moves the cursor (the shown message) to it.
  - Replace by id (D-015): a message with the same non-empty id as a queued one replaces it and moves to the front. Replacing works even when the queue is full.
  - Full queue: new messages without a matching id are dropped and existing messages are kept. The device shows a popup (F-005) and the MCP response reports it (F-003).
  - Scroll: shows the next older message, wrapping from the oldest to the newest.
  - Delete: removes the shown message of either kind, then shows the next older one (wrapping to the newest). Clear all removes every message (F-006).
  - Countdown (D-020): a time-driven message keeps its remaining time and counts down only while it is the shown message. `tick(now)` charges the time since the previous tick to the shown message and removes it when its time runs out; the next older message is then shown. Messages not on screen keep their remaining time. Replacing a message by id starts the new duration. When other messages are removed, the cursor stays on the message being shown.
  - Time is passed to `tick()` in milliseconds as a 64-bit value; the first call only sets the starting point, so time before the first tick (boot) is not counted. The firmware must use a 64-bit uptime clock, not the 32-bit `millis()`, which wraps after about 49.7 days.
  - Persistence (D-009, D-017, D-022): the queue is saved to `/queue.bin` on LittleFS 1 s after the last content change (add, delete, clear all, expiry; scrolling does not save) and restored at boot in the same order, showing the newest message. Time-driven messages restart their full duration. Remaining time and the scroll position are not saved. A missing, corrupt, or unknown-version file is ignored and the queue starts empty.
- **Verification:** `pio test -e native`: 22 unit tests covering validation, limits, order, scroll, full queue, replace by id, delete, clear, and the shown-only countdown with expiry. On device (0.0.4, 2026-09-23): with the user scrolling away and back, the 30 s message expired after exactly 30 s of time on screen (serial log, within 20 ms). `test_queue_codec`: 7 tests for the saved format (round trip, restart of durations, maximum size, bad data). On device (0.0.6, 2026-09-23): 5 messages saved (1,030 bytes, 65 ms); after an expiry 4 saved (939 bytes, 70 ms); after a reset `Restored 4 messages`.

### F-005 - Message screen and rendering

- **Area:** Display
- **Status:** In progress (message screen in 0.0.3; queue-full popup pending with F-003)
- **Added in version:** 0.0.3
- **Description:** The message screen shows the message title on top and the message value in a large text area below, under the top bar (F-007). The title has one fixed style. The value uses the font size (one of two) and color (white, blue, green, red) chosen by the sender (D-004). Long text scrolls automatically (D-018). When the queue is full, a popup says so.
- **Source files:** `src/screen.cpp`, `src/screen.h`, `lib/ui_logic/src/text_wrap.*`, `lib/ui_logic/src/scroll_offset.*`, `lib/ui_logic/src/countdown.*`, `lib/ui_logic/src/markup.*`
- **Behavior:**
  - Layout: see Architecture, Screen layout.
  - Fonts (since 0.0.15, D-031): each board names four u8g2 fonts in its `display.h` (top bar, title, small, large); the T-Display uses `helvR12` for the first three and `helvR18` for large. Row heights (bar, title, value area, countdown box) and the setup and welcome screens are laid out from the fonts' measured metrics at boot. Before 0.0.15: TFT_eSPI font 2 (16 px line) and font 4 (26 px). Title: light grey.
  - Colors: white, blue (0x4C9F, lighter than TFT_BLUE for readability on black), green, red.
  - Inline colors (D-021, since 0.0.5): `{white}`, `{blue}`, `{green}`, `{red}` switch the color of the following text; `{/}` returns to the message color. Other text in braces is shown as written. Tags are removed before wrapping, so they take no space on screen.
  - Word wrap at spaces; a word wider than the line is broken; `\n` starts a new line.
  - Auto-scroll (D-018): a title wider than 232 px scrolls horizontally at 40 px/s; a value taller than 92 px (94 px before 0.0.12) scrolls vertically at 20 px/s. Each pauses 1.5 s at the start, moves to the end, pauses 1.5 s, then jumps back. Scrolling restarts only when the shown text or font changes.
  - Countdown box (D-020): a time-driven message shows its remaining time in a small bordered box at the bottom right of the value area (bottom left in 0.0.4), over the value text: `45s`, `4:05`, or `1:02:03`, rounded up so it never shows 0. Font 2, light grey on black, dark grey border. The screen redraws when the shown number changes.
  - Empty queue: "No messages" centered in the value area.
  - Rendering (D-019): each frame is drawn into one full-screen 8-bit sprite and pushed at once. Redraws happen on queue changes and every 33 ms only while something scrolls.
  - Queue-full popup (since 0.0.8): a red-bordered box in the middle of the screen, "Queue full" and "new messages dropped", for 3 s after show_message hits a full queue. Not yet seen on the device.
  - Non-ASCII text is cleaned up before it reaches the queue (F-003, D-026).
- **Verification:** `pio test -e native` covers word wrap and scroll timing. On device (0.0.3, 2026-09-22): serial log confirms boot and demo messages; the user checked the screen on 2026-09-23 and reported it looks good (title scrolls sideways, values scroll down, no flicker, text not clipped).

### F-006 - Buttons: delete, clear all, scroll

- **Area:** Input
- **Status:** Done
- **Added in version:** 0.0.3
- **Description:** Button mapping (D-007; on the Waveshare AMOLED: BOOT is delete, the touchscreen is scroll, D-033):
  - GPIO35 (delete): short press deletes the currently shown message; hold clears all messages.
  - GPIO0 (scroll): short press shows the next queued message, wrapping from last to first.
- **Source files:** `src/main.cpp`, `lib/ui_logic/src/button_tracker.*`
- **Behavior:**
  - Both buttons are active LOW, debounced for 30 ms.
  - Delete: short press fires on release and removes the shown message of either kind (F-004). Holding for 1.5 s clears all messages; it fires while still held, without a confirmation step, and the release after it does nothing.
  - Scroll: short press shows the next older message, wrapping to the newest; does nothing with fewer than two messages. Long press has no action.
  - Both buttons held for 5 s: Wi-Fi reset (F-002). From the moment both are down until both are released and settled, no single-button action runs, so the delete button's 1.5 s hold cannot clear the queue on the way (`lib/ui_logic/src/button_pair.*`, since 0.0.7).
  - Button events are ignored while the Wi-Fi setup screen is shown.
  - Each action is logged on serial: `Deleted message, N left`, `Cleared all messages`, `Showing message N of M`.
- **Verification:** `pio test -e native` covers debounce, short press, and long press. On device, 2026-09-23: the user checked delete, hold to clear all, and scroll with 0.0.3 and reported it looks good.

### F-007 - Top status bar

- **Area:** Display
- **Status:** Done (redesigned with battery in 0.0.12, clock since 0.0.13)
- **Added in version:** 0.0.3
- **Description:** A bar across the top of the screen shows the IP address, queue position and count, battery, and clock (F-008).
- **Source files:** `src/screen.cpp` (`drawTopBar`, `drawBatteryIcon`), `src/battery.*`, `lib/ui_logic/src/battery_level.*`, `src/main.cpp` (`statusBar`, `readBatteryIfDue`)
- **Behavior (since 0.0.12, D-028):**
  - Dark slate bar, 20 px, with rounded slate-grey pills 18 px high, 3 px apart, laid out from the right: clock, battery, queue position; the network pill takes the rest on the left.
  - Clock pill: `HH:MM`, or `--:--` until the time is known (F-008, since 0.0.13).
  - Battery pill: an 18 x 9 px battery icon. On battery: fill level, green above 50 %, amber above 20 %, red below. On USB power (reading at or above 4,400 mV): an amber lightning bolt. Before the first reading: empty outline. Read every 10 s, 16 samples averaged (`analogReadMilliVolts` on GPIO34 times 2, ADC_EN high). The percentage comes from a typical Li-ion curve (4,200 mV = 100 %, 3,300 mV = 0 %) and is approximate.
  - Queue pill: amber, `position/count` such as `2/5` (`0/0` when empty) in black, drawn twice one pixel apart for a bold look.
  - Network pill: a 3 px status stripe (green connected, amber connecting or setup, red offline), then the IP address or `connecting` / `no network`. The worst case (`30/30` in the queue pill) still fits the IP address.
  - Before 0.0.12: plain text on black, IP left, position centered, `--%` and `--:--` right.
  - Serial command `B` prints the battery voltage.
- **Verification:** `pio test -e native` covers the battery level mapping. On device, 2026-09-23 (0.0.12): screenshot shows the bar as described with the USB lightning bolt; `B` reads 4,765 mV on USB power.

### F-008 - Clock with NTP and IP-based timezone

- **Area:** Networking, time
- **Status:** Done
- **Added in version:** 0.0.13
- **Description:** The device gets the time from NTP and the timezone from a geolocation lookup of its public IP address (D-012, D-029). The clock shows in the top bar (F-007). This needs internet access, not only the LAN.
- **Source files:** `src/clock.*`, `lib/clock_logic/src/clock_logic.*`, `test/test_clock_logic/test_main.cpp`
- **Behavior:**
  - NTP: `configTime` with `pool.ntp.org` and `time.google.com`, started at the first connection; the ESP-IDF SNTP client keeps it synced.
  - Timezone: `GET http://ip-api.com/json/?fields=status,message,timezone,offset` over plain HTTP/1.0 at the first connection, then every hour; after a failure, every minute. The reply's `offset` is the current UTC offset including daylight saving, so a daylight saving change shows within an hour. Offsets outside -14 h to +14 h are rejected.
  - Display: 24-hour `HH:MM`; `--:--` until both the NTP time and the offset are known. No fallback to UTC.
  - A lookup blocks the loop for its duration (about 0.2 s observed; at most 3 s timeout).
  - Serial: `Clock: timezone <zone>, UTC offset +N s` when the offset changes, `Clock: NTP time received`, `Clock: timezone lookup failed ...`.
- **Verification:** `pio test -e native`: 5 tests in `test_clock_logic` (reply parsing, failures, HTTP status and body split, time formatting across midnight and negative offsets). On device, 2026-09-23 (0.0.13): `Clock: timezone <timezone>, UTC offset +7200 s` 0.2 s after connecting, NTP synced 1 s later; the screenshot showed 20:58 while the host clock read 18:58 UTC.

### F-009 - Welcome screen and IP message

- **Area:** Display, Wi-Fi
- **Status:** In progress (implemented in 0.0.9; the IP message verified on the device, the welcome screen not yet seen)
- **Added in version:** 0.0.9
- **Description:** After connecting, the device shows a welcome screen with its DHCP IP address and adds a message with the IP address to the queue (D-010, D-027).
- **Source files:** `src/main.cpp` (`announceConnection`, `addIpMessage`), `src/screen.cpp` (`showWelcome`)
- **Behavior:**
  - Welcome screen: at the first connection after each boot, including right after Wi-Fi setup, for 3 s. Shows `ScreenAPI v<FW_VERSION>`, `Connected to <SSID>`, the IP in large green text, and `screenapi.local/mcp`. Timed messages do not count down and buttons are ignored while it is shown; MCP requests are still handled.
  - IP message: id `ip`, title `Network`, value `Connected to <SSID>` (green), `IP <ip>`, `MCP http://screenapi.local/mcp`; small white font; timed, 60 s on screen. Added at the first connection after boot and when the IP changes; a reconnect with the same IP adds nothing. The id makes it replace the previous IP message, including one restored from flash. A full queue drops it (serial: `IP message dropped: queue full`).
- **Verification:** On device, 2026-09-23 (0.0.9): 4 messages restored, then the IP message added (5 messages saved, 1,036 bytes); `queue_status` over MCP reported 5 of 30. Not yet: the welcome screen by eye, an IP change.

### F-010 - Boot screen with firmware version

- **Area:** Display
- **Status:** Done
- **Added in version:** 0.0.1
- **Description:** First firmware. Brings up the toolchain, display driver, and board: prints the firmware version on serial and shows it on screen.
- **Source files:** `src/main.cpp`, `src/screen.cpp`, `include/boards/tdisplay/pins.h`, `include/boards/tdisplay/display.h`, `platformio.ini`
- **Behavior:** On boot, prints `ScreenAPI v<FW_VERSION>` on serial at 115200 baud. The screen, in landscape (rotation 1), shows "ScreenAPI" (font 4) and "v<FW_VERSION>" (font 2) centered, white on black, with the backlight on. Since 0.0.3 the boot screen stays for 1.5 s, then the message screen (F-005) follows.
- **Verification:** `pio run -e tdisplay` succeeds. On device, 2026-09-22 (reported by the user): serial shows `ScreenAPI v0.0.1` after the ROM boot log (BOARDS.md, On-device verification); the screen works.

### F-011 - Demo messages (temporary)

- **Area:** Test
- **Status:** Deprecated (removed in 0.0.10, once messages arrive over MCP, F-003)
- **Added in version:** 0.0.3
- **Description:** Five sample messages added at boot so the message screen, scrolling, buttons, and expiry can be tried before MCP exists.
- **Source files:** `src/main.cpp` (`addDemoMessages`, 0.0.3 to 0.0.9; removed)
- **Behavior:** Shown first: a large-font message with inline green and red parts and 30 s on screen. Then: a confirm message with a title and value long enough to scroll both ways; a small-font confirm message showing inline colors and an unknown tag; a red large-font confirm message long enough to scroll; a white message with 60 s on screen. Serial prints `Queue: 5 messages` and the free heap after boot. Since 0.0.6 the demo messages are added only when nothing was restored (first boot, or an empty queue was saved).
- **Verification:** On device, 2026-09-23 (0.0.5): serial shows `Queue: 5 messages` at 2.3 s; the first message expired after 30 s on screen, at 32.3 s.

### F-012 - Screenshot over serial (development)

- **Area:** Tooling
- **Status:** Done
- **Added in version:** 0.0.11
- **Description:** Captures the current screen as a PNG without a camera, to check layouts and colors while developing.
- **Source files:** `src/main.cpp` (`handleSerialCommands`), `src/screen.cpp` (`sendScreenshot`), `tools/screenshot.py`
- **Behavior:** Sending `S` on the serial port makes the firmware write `SCREENSHOT <width> <height> indexed565`, a palette line, one line of hex per pixel row of the 8-bit frame buffer, and `END` (`rgb332` without a palette line in 0.0.11 to 0.0.14); on the native USB serial of the ESP32-S3 the firmware waits for the host while sending (`hal::setSerialBlocking`); the loop blocks for about 6 s. `tools/screenshot.py [out.png] [--port /dev/ttyUSB0] [--scale 3]` sends the command and saves a PNG. It opens the port without resetting the board (BOARDS.md UART). Only frames drawn through the frame buffer are captured; the boot screen is not.
- **Verification:** On device, 2026-09-23: screenshots of the message screen matched the expected layout; opening the port with the script's line order did not reset the board.

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
- **Message persistence:** up to 30 messages survive reboot (F-004); time-driven messages restart their full duration (D-017).
- **Filesystem:** LittleFS on the `spiffs` data partition, mounted at `/littlefs` (D-022). Files: `/queue.bin` (saved queue, format in `lib/message_queue/src/queue_codec.h`), `/queue.tmp` (written, then renamed over `/queue.bin`).
- **Partitions:** `min_spiffs.csv` since 0.0.7 (D-024), see BOARDS.md MCU, Partition scheme. LittleFS has 128 KB. NVS (20 KB) is too small for a full queue (up to 18,098 bytes encoded).

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
| D-013 | Superseded by D-031 in 0.0.15 (TFT_eSPI removed). Pins live in the board's `pins.h` (since 0.0.14 `include/boards/<board>/`, before `include/`). TFT_eSPI is upstream `bodmer/TFT_eSPI@2.5.43`, configured by the board's `tft_setup.h` (which includes pins.h); library files stay unmodified. Each env needs `-Iinclude/boards/<board>` in build_flags (before 0.0.14: `-Iinclude`). Check with the preprocessor that the library compile sees the board's driver and pins. | One pin header per board. Without the `-I` path, TFT_eSPI does not see tft_setup.h and silently compiles with its default ILI9341 setup while src/ uses ours; the build still succeeds (observed in a verbose build). | 2026-09-22 |
| D-014 | Newest message first; a new message is shown immediately. Scroll goes from newest to oldest and wraps. | User decision. | 2026-09-22 |
| D-015 | A message may carry an optional id. A new message with the same id replaces the old one and moves to the front, also when the queue is full. | User decision, so repeated status updates from one sender take one slot. | 2026-09-22 |
| D-016 | Limits: id 16, title 64, value 512 characters (raised from 30 and 160 in 0.0.3); time-driven duration 1 to 86400 seconds. Input outside the limits is rejected, not truncated. | The user asked for longer titles and values, with scrolling (D-018), and left the numbers open. 30 messages at these limits take 18,488 bytes. Rejecting lets the MCP client resend a shorter message instead of showing cut-off text. | 2026-09-22 |
| D-017 | After a reboot, time-driven messages restart their full duration. | User decision. Needs no clock and no extra flash writes. | 2026-09-22 |
| D-018 | Long text scrolls automatically: the title horizontally, the value vertically. | User decision. Both buttons are already used (D-005), so scrolling needs no input. | 2026-09-22 |
| D-019 | The screen is drawn into one full-screen 8-bit buffer (240 x 135, 32,400 bytes) and pushed at once. Since 0.0.15 an Arduino_GFX indexed canvas: one byte per pixel into a palette of up to 256 exact RGB565 colors (before: TFT_eSPI RGB332 sprite). | Flicker-free redraws (AGENTS.md). 8-bit halves the RAM of a 16-bit buffer; the palette keeps every color exact. | 2026-09-22 |
| D-020 | A time-driven message counts down only while it is on screen. Its remaining time shows in a small box at the bottom right (moved from bottom left in 0.0.5). | User decision. | 2026-09-23 |
| D-021 | Inline color tags in the value: `{white}`, `{blue}`, `{green}`, `{red}`, and `{/}` back to the message color. Unknown tags are shown as written; there is no nesting and no escape. Only the value takes tags; the title keeps its fixed style (D-004). | The user asked for parts of the text in different colors and left the syntax open. Short tags are easy for an LLM to write, and leaving unknown braces alone keeps code and JSON readable. | 2026-09-23 |
| D-022 | Messages are saved as one file on LittleFS in the `spiffs` partition, written to a temporary file and renamed, 1 s after the last change. The firmware formats the partition if it has no file system. | NVS is 20 KB, too small for a full queue next to Wi-Fi credentials. Rename is atomic, so a power cut leaves the old or the new file. The delay turns a burst of changes into one write. Formatting was approved by the user; the partition was blank when checked. | 2026-09-23 |
| D-023 | Wi-Fi setup: ESP BLE Provisioning with security 1; the setup screen shows a QR code, the device name, and a random code. Without saved settings the setup screen covers the messages until provisioned; with settings the message screen stays and shows `no network` while offline. Holding both buttons for 5 s forgets the network. | User decisions. | 2026-09-23 |
| D-024 | Partition table `min_spiffs.csv`: two 1.9 MB app slots (OTA stays possible), 128 KB LittleFS. | User decision. BLE provisioning made the firmware 1.64 MB, over the default 1.25 MB app slot; MCP needs more. Changing it erased the saved messages once. | 2026-09-23 |
| D-025 | MCP over Streamable HTTP without streaming or sessions: one JSON response per POST at `/mcp` on port 80, plus the mDNS name `screenapi.local`. Requests with a foreign `Origin` header are rejected. Tools: `show_message` and `queue_status`. | User approved the proposal. JSON-only responses keep the firmware small and work with Claude Code; the Origin check is required by the MCP spec; mDNS keeps the client setting valid when the DHCP address changes. | 2026-09-23 |
| D-026 | Text from MCP is converted for the ASCII fonts: common typographic characters are mapped (dashes, curly quotes, ellipsis, arrows, bullets, check marks, non-breaking spaces), other non-ASCII characters become '?', and limits apply after the conversion. | The fonts cover ASCII only, and LLM output often contains typographic characters. Mapping keeps the text readable; the result tells the client what was replaced. | 2026-09-23 |
| D-027 | The welcome screen shows for 3 s at the first connection after each boot. The IP message has id `ip`, is timed (60 s on screen), and is re-added only at the first connection after boot or when the IP changes. | The user asked for the IP on a welcome screen and as a queued message and left the details open. The id keeps one IP message at most; timed, because the IP is always in the top bar. | 2026-09-23 |
| D-028 | Top bar: dark slate bar with separate pills (network with status stripe, amber bold queue position, battery icon, clock). Battery as an icon only, a lightning bolt at or above 4,400 mV. Colors are exact RGB332 values. | The user asked for a dark bar, separation between elements, and a bolder queue position further right. Text for the battery and a Wi-Fi icon did not fit next to a full IP address and `30/30`. The 8-bit frame buffer would shift other colors (a 16-bit dark grey becomes olive). | 2026-09-23 |
| D-029 | Clock: NTP for the time; the current UTC offset from `ip-api.com` over plain HTTP/1.0, refreshed hourly (every minute after a failure); 24-hour format; `--:--` until both are known. | The user asked for NTP time and a timezone based on the IP (D-012). ip-api.com needs no key, and its offset already includes daylight saving, so no timezone database is needed. A hand-written GET instead of HTTPClient saves about 150 KB of flash. | 2026-09-23 |
| D-030 | Board-specific headers live in `include/boards/<board>/` (`board.h`, `pins.h`; `tft_setup.h` before 0.0.15, `display.h` in 0.0.15), board code in `src/boards/<board>/` since 0.0.16 (D-032), selected per PlatformIO env with `-Iinclude/boards/<board>`. `board.h` gives the screen size, rotation, and capabilities; the layout derives from the size; battery support is optional per board. | The user asked for a structure that makes adding boards easy. Keeping each board's headers together avoids `#if` chains. | 2026-09-23 |
| D-031 | Graphics with Arduino_GFX 1.6.0 on every board: an indexed canvas, u8g2 fonts (U8g2 2.36.18) chosen per board, text measured and aligned by the firmware. TFT_eSPI removed. | The user chose one library for all boards. TFT_eSPI cannot drive the AMOLED board's QSPI panel. 1.6.0 is the newest Arduino_GFX that builds on arduino-esp32 2.0.17 (1.6.1 and later need core 3.x; test builds 2026-09-23). u8g2 fonts come in many sizes for different screen densities. | 2026-09-23 |
| D-032 | A hardware layer, `src/hal.h`, with one implementation per board in `src/boards/<board>/hal.cpp`, selected with `build_src_filter`. It owns pins, buses, the display driver, fonts, inputs, and battery reading. | The AMOLED board needs code the T-Display does not (expander reset, touch, power chip, revision detection); per-board source files avoid `#if` chains in shared code. | 2026-09-24 |
| D-033 | Waveshare AMOLED: portrait 368 x 448; BOOT deletes (hold 1.5 s clears all), a touch on the screen shows the next message, BOOT and touch held 5 s reset Wi-Fi; the revision is detected from the touch chip's I2C address (FT3168 at 0x38 original, CST820 at 0x15 V2). Every board uses the mDNS name `screenapi` (since 0.0.18; `screenapi-amoled` in 0.0.16 and 0.0.17). | User decisions (revision detection, controls, orientation, one mDNS name). One name keeps one MCP URL for all boards; only one board should be online at a time. With two online, ESP-IDF mDNS is expected to rename one (for example `screenapi-2`); not verified. After switching boards, a client can keep the old address for up to about 2 minutes (mDNS cache). | 2026-09-24 |
| D-034 | Portability first: `src/hal.h` documents the whole board contract, and a template board (`include/boards/template/`, `src/boards/template/`, generic ESP32 with an ST7789 SPI display, no battery) is built by its own `template` env so the starting point for new boards always compiles. | The user asked to focus on hardware independence so ScreenAPI ports easily to any ESP32 with Wi-Fi and a display. A template that is built cannot silently fall out of date. | 2026-09-24 |
| D-035 | The firmware version lives in the `VERSION` file (one line, `X.Y.Z`); `tools/version.py`, a PlatformIO post script, checks the format and passes it to `src/` as `FW_VERSION`. It is not in platformio.ini. | PlatformIO wipes the whole build folder whenever platformio.ini changes (`compute_project_checksum` hashes the full configuration), so a version bump there rebuilt every library, about 9 minutes per board. Moving the define to `build_src_flags` did not help (measured 548 s). User approved the change of the AGENTS.md rule. | 2026-09-24 |

## Verification

Standard PlatformIO commands. Replace `<env>` with an environment from platformio.ini.

```
pio run -e <env> -j 2                # build (2 jobs: the U8g2 font source needs about 1 GB per job)
pio run -e <env> -t upload           # build and flash
pio device monitor -e <env>          # serial monitor
pio test -e native                   # hardware-independent unit tests
pio test -e <env>                    # on-device tests
```

Screenshot of the device screen (F-012): `python3 tools/screenshot.py screenshot.png`

MCP endpoint by hand (F-003), from a machine on the same network:

```
curl -s -H 'Content-Type: application/json' -H 'Accept: application/json, text/event-stream' \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"queue_status","arguments":{}}}' \
  http://screenapi.local/mcp
```

## Open questions

Known unknowns and issues found outside the current task. Smaller per-feature details are listed as TBD in each feature entry.

- Timezone lookup (D-029): ip-api.com is free for non-commercial use only and answers over plain HTTP; the device's public IP address goes to that service. Daylight saving changes can show up to an hour late. Revisit if the use becomes commercial or the service changes.
- Memory budget: baseline with 0.0.13 (Wi-Fi, MCP server, mDNS, clock): static RAM 86,832 bytes, free heap 156,724 bytes, largest free block 94,196 bytes after boot. Flash: 1,781,937 of 1,966,080 bytes (90.6%); about 180 KB left in the app slot. Avoid HTTPClient (about 150 KB of TLS code). Re-check as features land.
- The core's WebServer reads the whole request body into memory before the handler checks the 8 KB limit, so a LAN client sending a very large Content-Length can exhaust the heap. Accepted for now together with D-011 (no authentication on the LAN).
- Wi-Fi setup path (F-002) not yet tested on the device, because the board already had saved settings. Test by holding both buttons for 5 s, then pairing with the ESP BLE Provisioning app.
- The board came with saved Wi-Fi settings for `<SSID>` that this project did not write (BOARDS.md Wi-Fi). Unknown origin; confirm it is the intended network.
- `esp_app_desc` does not carry FW_VERSION. The built image reports project name `arduino-lib-builder` and app version `esp-idf: v4.4.7 38eeba213a`, from the precompiled Arduino core. This does not meet the AGENTS.md rule that esp_app_desc reads FW_VERSION. Options (override the descriptor, or accept it for the Arduino framework) TBD.
