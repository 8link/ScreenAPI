# CHANGELOG.md

**Current firmware version:** 0.0.24

## Format

- Newest entry at the top. Every entry is a level-2 heading; the heading is the one-line entry.
  - Firmware change: `## X.Y.Z - Short imperative description`
  - Non-firmware change: `## docs - ...`, `## test - ...`, or `## tooling - ...`
- Firmware entries that change behavior, pins, protocols, storage layout, power, or display must include a detail block with: **Date**, **Author**, **Type**, **Summary**, **Changes** (one line per file), **Verification**, **Git commit**. Other entries may be one heading line only.
- Take the date from the system (`date "+%Y-%m-%d %H:%M"`), never guess it.
- Reference feature IDs (F-xxx) and quirk IDs (Q-xxx) where relevant.
- Update the current firmware version above with every firmware entry.

Example:

```
## 0.0.16 - Couple simulated temperature to load

**Date:** 2026-06-27 12:15
**Author:** <agent or model name, or user>
**Type:** Simulator

**Summary:**
Made simulated temperature rise with speed, PWM/current load, and acceleration instead of only following PWM.

**Changes:**
- `src/sim_data.cpp` - Temperature target combines PWM/current load, sustained speed, and acceleration stress.
- `include/config.h` - Raised simulated thermal range to `25..75 C`; separate heat/cool step constants so cooling is slower than heating.
- `PROJECT.md` - Updated F-012 behavior notes.
- `platformio.ini` - `FW_VERSION` bumped to `0.0.16`.

**Verification:**
- `pio run -e <env>` succeeded.
- On device: temperature climbs during fast or high-current sections and falls more slowly when load drops.

**Git commit:**
- `v0.0.16 - Couple simulated temperature to load`
```

---

## 0.0.24 - Restyle the message title

**Date:** 2026-09-24 11:52
**Author:** Claude Opus 5.5
**Type:** Display

**Summary:**
The message title is white with a thin grey line under it, and the value's default "white" text is drawn light grey (198, 195, 198) so the title stands out (D-037). On the Waveshare AMOLED the title uses the larger bold `fub20` font and sits 2 px lower, with 5 px between the line and the text. A long title scrolls within the line's width, clear of the rounded corners. The user asked for a larger title set apart from the text; after several rejected styles (a light band, blue lines, a title card, a blue title, mockups of other layouts) they chose this one and confirmed it on the panel. The T-Display keeps its row positions; only its colors change.

**Changes:**
- `src/screen.cpp` - White title, grey line under it, light grey default value color, title area clear of rounded corners and clipped when scrolling.
- `src/boards/waveshare_amoled18/hal.cpp` - Title font `fub20`.
- `VERSION` - `0.0.24`.
- `PROJECT.md` - Screen layout, F-005, D-037.
- `README.md` - `color` parameter note.

**Verification:**
- `pio test -e native`: 81 of 81 passed.
- `pio run -j 4`: `waveshare_amoled18` (flash 1,585,573 bytes, 24.2%), `tdisplay` (flash 1,792,005 bytes, 91.1%), `template` (flash 1,785,097 bytes, 90.8%); all three images contain `ScreenAPI v0.0.24`. The T-Display was not checked on the device.
- AMOLED: flashed; screenshots of a short title and a scrolling long title; the user confirmed the final style on the panel.

**Git commit:**
- `v0.0.24 - Restyle the message title`

## 0.0.23 - Add a band behind the top bar on rounded boards

**Date:** 2026-09-24 09:59
**Author:** Claude Opus 5.5
**Type:** Display

**Summary:**
On boards with rounded corners the top bar sits on a cool dark grey band (40, 44, 48) with a grey line under it, so it stands apart from the message (requested by the user). The title starts 3 px lower. The panel's rounded corners trim the band. Square boards are unchanged. The user checked it on the AMOLED: good as is.

**Changes:**
- `src/screen.cpp` - Band and line on rounded boards; title 3 px lower there.
- `VERSION` - `0.0.23`.
- `PROJECT.md` - F-007.

**Verification:**
- `pio run -j 4`: `waveshare_amoled18` 72 s (flashed), `tdisplay` (flash 1,791,925 bytes, 91.1%), `template` 94 s (flash 1,785,057 bytes, 90.8%); all three images contain `ScreenAPI v0.0.23`.
- AMOLED: screenshots of a darker first try (24, 28, 24) and the final band; the user confirmed the final band on the panel.

**Git commit:**
- `v0.0.23 - Add a band behind the top bar on rounded boards`

## 0.0.22 - Fit top bar and countdown into the AMOLED's rounded corners

**Date:** 2026-09-24 09:16
**Author:** Claude Opus 5.5
**Type:** Display

**Summary:**
0.0.21 still cut the corners: its arc test was misread (20 px). A clearer corner test (squares along the diagonal, a frame on the edges) measured 24 px (D-036). A first layout with 24 px on both sides was complete but left a large colored band above the bar. Now, on boards with rounded corners, the top bar sits high on black with capsule pills and a status dot, and the countdown is a capsule; the side and corner insets are computed from the largest radius the measurement allows. The user checked it on the panel: nothing cut, looks right. Square boards are unchanged.

**Changes:**
- `src/screen.cpp`, `src/screen.h` - Corner test with squares and an edge frame; layout for rounded corners (bar top, side inset, capsule pills, status dot, no bar background, capsule countdown with diagonal inset).
- `include/boards/*/board.h` - `kCornerInset` replaces `kCornerRadius`: AMOLED 24, T-Display and template 0.
- `VERSION` - `0.0.22`.
- `PROJECT.md` - D-036; F-007.
- `BOARDS.md` - AMOLED rounded corners and hidden edge pixels.

**Verification:**
- AMOLED: corner test read by the user (square at 16 px cut, 24 px complete, frame not visible on any edge); screenshot of the final layout; the user confirmed on the panel that nothing is cut.
- `pio run -j 4`, source files only: `waveshare_amoled18` 81 s (flashed), `tdisplay` 84 s (flash 1,791,925 bytes, 91.1%), `template` 82 s (flash 1,785,057 bytes, 90.8%).

**Git commit:**
- `v0.0.22 - Fit top bar and countdown into the AMOLED's rounded corners`

## 0.0.21 - Keep top bar and countdown clear of rounded corners

**Date:** 2026-09-24 08:13
**Author:** Claude Opus 5.5
**Type:** Display

**Summary:**
The AMOLED's rounded corners cut off the top bar and the countdown box (reported by the user). Boards now declare `kCornerRadius`; the top bar and the countdown move in by the corner margin (D-036). A corner test (serial command `C`) measured the AMOLED radius at no more than 20 px, giving a 7 px margin. The T-Display and the template have square corners (0).

**Changes:**
- `src/screen.cpp`, `src/screen.h` - Corner margin for the top bar pills, the stripe, the label, and the countdown box; corner test screen with arcs of 20 to 70 px.
- `src/main.cpp` - Serial command `C` shows the corner test for 60 s; the cover timer covers the welcome and corner test screens.
- `include/boards/*/board.h` - `kCornerRadius`: AMOLED 20, T-Display and template 0.
- `VERSION` - `0.0.21`.
- `PROJECT.md` - D-036; F-007.
- `BOARDS.md` - AMOLED rounded corners.

**Verification:**
- `pio run -j 4`: `waveshare_amoled18` 90 s, `tdisplay` 93 s (flash 1,797,701 bytes, 91.4%), `template` 92 s (flash 1,790,753 bytes, 91.1%); source files only, thanks to the VERSION file.
- AMOLED: corner test screenshot shows arcs in all four corners; the user saw the 20 px arc complete. With `kCornerRadius = 20`, a screenshot of a timed message shows the top bar pills and the countdown `1:25` 7 px in from the corners.
- Not yet checked by eye on the panel with the margin.

**Git commit:**
- `v0.0.21 - Keep top bar and countdown clear of rounded corners`

## 0.0.20 - Fix AMOLED hang when USB is connected to a PC

**Date:** 2026-09-24 08:03
**Author:** Claude Opus 5.5
**Type:** Bug fix

**Summary:**
With USB connected to a PC and no program reading the serial port, the AMOLED board stalled after boot on the welcome screen (reported by the user). Cause: a zero USB serial write timeout hits an unsigned wrap in arduino-esp32 2.0.17 `HWCDC::write()` (BOARDS.md Q-008). The timeout is now 10 ms.

**Changes:**
- `src/boards/waveshare_amoled18/hal.cpp` - Serial write timeout 10 ms (1 s for screenshots) instead of 0.
- `VERSION` - `0.0.20`.
- `BOARDS.md` - Q-008; serial timeout; MCP response time observation.

**Verification:**
- `pio run -e waveshare_amoled18 -j 2` succeeded (177 s, source files only) and was flashed.
- Before the fix: rebooted with the port closed; after 20 s the screen still showed the welcome screen and MCP did not answer; reading the port released it.
- After the fix: same test, MCP answers after 20 s; after another minute `queue_status` answers (first request 2.8 s, then about 20 ms).
- 0.0.19 builds finished after that commit: `tdisplay` 423 s (full), flash 1,791,201 bytes (91.1%); `template` 451 s (full), flash 1,784,365 bytes (90.8%). This change touches only the AMOLED board's source.

**Git commit:**
- `v0.0.20 - Fix AMOLED hang when USB is connected to a PC`

## 0.0.19 - Move the firmware version to a VERSION file

**Date:** 2026-09-24 07:39
**Author:** Claude Opus 5.5
**Type:** Tooling, build

**Summary:**
The firmware version moved from platformio.ini to a `VERSION` file, passed to `src/` as `FW_VERSION` by `tools/version.py` (D-035). PlatformIO rebuilds everything whenever platformio.ini changes, so every version bump took about 9 minutes per board. A first attempt with `build_src_flags` did not help (548 s for a version-only change) and was replaced. The AGENTS.md versioning rule points to `VERSION` (approved by the user). The firmware behaves the same.

**Changes:**
- `VERSION` - New. `0.0.19`.
- `tools/version.py` - New. Reads and checks `VERSION`, defines `FW_VERSION` for `src/` only.
- `platformio.ini` - No version; `extra_scripts = post:tools/version.py` in `[esp32]`; the per-board `build_flags` no longer include `${env.build_flags}`.
- `AGENTS.md` - Versioning rule: the `VERSION` file.
- `PROJECT.md` - D-035; repository layout.
- `README.md` - Version bump in the development table.

**Verification:**
- `pio run -e waveshare_amoled18 -j 4`: full build after the platformio.ini change 426 s; after changing only `VERSION` (to 0.0.99 and back) 79 s and 81 s, against 548 s with the version in platformio.ini. Flash 1,581,845 bytes (24.1%). `-j 4` works with the 4 GB swap added on the development machine.
- `tdisplay` and `template` were still building at commit time; their result is recorded with the next firmware entry.

**Git commit:**
- `v0.0.19 - Move the firmware version to a VERSION file`

## 0.0.18 - Use the mDNS name screenapi on every board

**Date:** 2026-09-24 06:44
**Author:** Claude Opus 5.5
**Type:** Networking

**Summary:**
All boards answer at `screenapi.local` (D-033 updated), so Claude Code and the AGENTS.md status rule keep one URL whichever board is online. Only one board should be online at a time.

**Changes:**
- `include/boards/waveshare_amoled18/board.h` - `kHostname` `screenapi` (was `screenapi-amoled`).
- `include/boards/template/board.h` - `kHostname` `screenapi` (was `screenapi-template`).
- `include/boards/tdisplay/board.h` - Comment only; the name was already `screenapi`.
- `platformio.ini` - `FW_VERSION` bumped to `0.0.18`.
- `PROJECT.md` - D-033.
- `BOARDS.md` - AMOLED mDNS name.
- `README.md` - One address for all boards; version badge.

**Verification:**
- `pio run -e waveshare_amoled18 -j 2` succeeded (flash 1,581,845 bytes, 24.1%) and was flashed: `ScreenAPI v0.0.18`, connected to Wi-Fi (provisioned by the user with the app), `queue_status` over MCP by IP works, and a raw mDNS query for `screenapi.local` is answered by the AMOLED board. The T-Display was offline, so there was no name conflict.
- `tdisplay` and `template` builds were stopped before finishing, to switch to faster builds in 0.0.19; both are built there.

**Git commit:**
- `v0.0.18 - Use the mDNS name screenapi on every board`

## docs - Add README for GitHub

## 0.0.17 - Add porting template board and document the hardware contract

**Date:** 2026-09-24 06:03
**Author:** Claude Opus 5.5
**Type:** Portability

**Summary:**
A template board (generic ESP32 with an ST7789 SPI display, two buttons, no battery) is the starting point for porting ScreenAPI to other ESP32 boards; its `template` env builds it so it always compiles (D-034). `src/hal.h` now documents the whole board contract and the requirements. The firmware of the T-Display and the AMOLED board is unchanged apart from comments.

**Changes:**
- `include/boards/template/board.h`, `pins.h` - New. Example values marked PORT.
- `src/boards/template/hal.cpp` - New. Complete `hal.h` implementation with porting notes.
- `src/hal.h` - Contract documented: when each function is called, what it must return, requirements for a board.
- `lib/clock_logic/src/clock_logic.h`, `test/test_clock_logic/test_main.cpp` - Example timezone changed to Europe/Berlin.
- `platformio.ini` - `template` env; `FW_VERSION` bumped to `0.0.17`.
- `PROJECT.md` - Adding a board uses the template; repository layout; D-034.

**Verification:**
- `pio test -e native`: 81 of 81 tests passed (clock tests with the new example timezone).
- `pio run -j 2`: `template` RAM 86,912 bytes (26.5%), flash 1,784,365 bytes (90.8%); `tdisplay` flash 1,791,201 bytes (91.1%); `waveshare_amoled18` flash 1,581,861 bytes (24.1%).
- Not flashed: the template is not a real board; the T-Display and AMOLED code changed only in comments.

**Git commit:**
- `v0.0.17 - Add porting template board and document the hardware contract`

## docs - Redact network and device identifiers

## 0.0.16 - Add Waveshare ESP32-S3-Touch-AMOLED-1.8 board

**Date:** 2026-09-24 05:10
**Author:** Claude Opus 5.5
**Type:** Board support, refactor

**Summary:**
Second board: Waveshare ESP32-S3-Touch-AMOLED-1.8, env `waveshare_amoled18` (D-033). A hardware layer, `src/hal.h` with `src/boards/<board>/hal.cpp`, now holds all board-specific code (D-032). The AMOLED firmware detects the revision from the touch chip (V2 found on the unit in hand), resets display and touch through the TCA9554 expander, reads battery and USB from the AXP2101, and uses BOOT as delete and the touchscreen as scroll. An SH8601 driver for the original revision is ported to Arduino_GFX 1.6.0 (untested). The T-Display moved onto the same layer; not yet checked on the device.

**Changes:**
- `src/hal.h` - New. Board hardware interface.
- `src/boards/tdisplay/hal.cpp` - New. T-Display pins, ST7789, fonts, buttons, battery ADC (from the removed `src/battery.*` and `include/boards/tdisplay/display.h`).
- `src/boards/waveshare_amoled18/hal.cpp` - New. Expander reset, revision detection, QSPI panel, fonts `helvR14` / `helvR18` / `helvR24`, BOOT and touch inputs, AXP2101 battery, blocking serial for screenshots.
- `include/boards/waveshare_amoled18/board.h`, `pins.h` - New.
- `include/boards/tdisplay/board.h`, `pins.h` - mDNS hostname; comments.
- `lib/sh8601_display/` - New. SH8601 driver: Arduino_GFX 1.6.0 CO5300 driver with the SH8601 init sequence and rotation from Waveshare's bundled Arduino_GFX (BSD license included).
- `src/main.cpp` - Hardware through `hal`; hostname in the IP message; `Hardware:` boot line; battery command shows percent and power source.
- `src/screen.cpp` - Display and fonts from `hal`; hostname on the welcome screen.
- `src/mcp_server.cpp`, `.h` - mDNS hostname from board.h.
- `lib/mcp_protocol/src/mcp_handler.cpp` - Instructions and font description no longer mention the T-Display's screen size.
- `platformio.ini` - Shared `[esp32]` section; `build_src_filter` per board; `waveshare_amoled18` env; `FW_VERSION` bumped to `0.0.16`.
- `PROJECT.md` - Adding a board, modules, Supported boards, F-006, F-012; D-030 updated, D-032, D-033.
- `BOARDS.md` - Waveshare AMOLED section; Q-006, Q-007.

**Verification:**
- `pio test -e native`: 81 of 81 tests passed.
- `pio run -e tdisplay -j 2`: RAM 86,928 bytes (26.5%), flash 1,791,173 bytes (91.1%). `pio run -e waveshare_amoled18 -j 2`: RAM 95,496 bytes (29.1%), flash 1,582,144 bytes of 6,553,600 (24.1%).
- AMOLED, flashed 2026-09-24: `Hardware: CO5300 + CST820 (V2), expander ok, AXP2101 ok`; Wi-Fi setup started (`PROV_XXXXXX`); screenshot shows the portrait setup screen with the QR code; `B` reads 4,180 mV, 100 %, USB power. First screenshot attempt came back cut off at 256 hex digits per row; fixed with blocking serial during screenshots.
- Not yet: the AMOLED panel by eye, touch, BOOT, Wi-Fi pairing, MCP; anything on the T-Display (disconnected; 0.0.15 and 0.0.16 both unverified there); the SH8601 path (no original-revision board).

**Git commit:**
- `v0.0.16 - Add Waveshare ESP32-S3-Touch-AMOLED-1.8 board`

## 0.0.15 - Switch graphics to Arduino_GFX with u8g2 fonts

**Date:** 2026-09-23 20:06
**Author:** Claude Opus 5.5
**Type:** Display, refactor

**Summary:**
Rendering moved from TFT_eSPI to Arduino_GFX 1.6.0 with u8g2 fonts (D-031), the library the Waveshare AMOLED board needs. Each board's `display.h` creates the panel and names its fonts; the layout rows come from the measured font metrics. The frame buffer is an indexed canvas with exact colors (D-019). Screenshots use a new `indexed565` format. Not yet checked on the device: the T-Display was disconnected after the build.

**Changes:**
- `include/boards/tdisplay/display.h` - New. SPI bus, ST7789 with the panel offsets, backlight, fonts `helvR12` and `helvR18`.
- `include/boards/tdisplay/tft_setup.h` - Removed.
- `include/boards/tdisplay/board.h`, `pins.h` - Comments for display.h.
- `src/screen.cpp` - Arduino_GFX indexed canvas; text measuring and alignment; layout from font metrics; stacked setup screen on portrait boards; boot screen on the canvas; screenshot with palette.
- `src/screen.h` - Exported colors; screenshot format.
- `src/main.cpp` - Colors from screen.h.
- `tools/screenshot.py` - Reads `indexed565` and `rgb332`.
- `platformio.ini` - Arduino_GFX 1.6.0 and U8g2 2.36.18 instead of TFT_eSPI; `FW_VERSION` bumped to `0.0.15`.
- `PROJECT.md` - F-005 fonts, F-010 sources, Adding a board, build command; D-013 superseded, D-019, D-030 updated, D-031 added.
- `BOARDS.md` - Display driver and offsets; Q-002 no longer relevant.

**Verification:**
- `pio test -e native`: 81 of 81 tests passed.
- `pio run -e tdisplay -j 2` succeeded: RAM 86,920 bytes (26.5%) static, flash 1,790,925 bytes (91.1%). With the default job count the compiler was killed for lack of memory (U8g2 font source).
- Test builds on arduino-esp32 2.0.17: Arduino_GFX 1.4.9 to 1.6.0 build; 1.6.1 fails (`esp_rgb_panel_t`); 1.6.2 and later need core 3.x (`esp32-hal-periman.h`); the SH8601 driver first appears in 1.6.x releases that need core 3.x.
- Not yet on the device: the T-Display was unplugged after the build. To check: screenshots against 0.0.14, boot, fonts, all screens.

**Git commit:**
- `v0.0.15 - Switch graphics to Arduino_GFX with u8g2 fonts`

## 0.0.14 - Organize board-specific code per board

**Date:** 2026-09-23 19:23
**Author:** Claude Opus 5.5
**Type:** Refactor

**Summary:**
Board-specific headers moved to `include/boards/tdisplay/` with a new `board.h` for the screen size, rotation, and capabilities (D-030). The layout derives from the board's screen size, the setup QR code scales to the height, battery support is optional per board, and the boot line names the board. Behavior on the T-Display is unchanged. PROJECT.md gains an "Adding a board" checklist.

**Changes:**
- `include/boards/tdisplay/board.h` - New. Name, screen size and rotation, battery sense flag.
- `include/boards/tdisplay/pins.h`, `tft_setup.h` - Moved from `include/`; pins.h documents what every board must provide.
- `platformio.ini` - `-Iinclude/boards/tdisplay` instead of `-Iinclude`; `FW_VERSION` bumped to `0.0.14`.
- `src/screen.h`, `.cpp` - Size and rotation from board.h, compile-time minimum size, centered setup and welcome screens, QR scale from the height, battery pill only with battery sense.
- `src/battery.h`, `.cpp` - `available()`; compiled without battery pins; compile-time check against board.h.
- `src/main.cpp` - board.h; boot line with the board name; battery reading only when available.
- `AGENTS.md` - Pin header rule points to `include/boards/<board>/pins.h`.
- `PROJECT.md` - Repository layout, Adding a board, F-010 sources, D-013 updated, D-030.
- `BOARDS.md` - Pin table path, boot line, second battery reading.

**Verification:**
- `pio test -e native`: 81 of 81 tests passed.
- `pio run -e tdisplay` succeeded: RAM 86,832 bytes (26.5%) static, flash 1,782,129 bytes (90.6%). The TFT_eSPI library compile still sees ST7789_DRIVER, CGRAM_OFFSET, and the T-Display pins.
- On device: `ScreenAPI v0.0.14 on LilyGO TTGO T-Display`; Wi-Fi, clock, MCP, and saving as before; screenshots identical in layout to 0.0.13; `B` reads 4,707 mV.

**Git commit:**
- `v0.0.14 - Organize board-specific code per board`

## docs - Add rule for task status messages on the display

## 0.0.13 - Add clock with NTP and IP-based timezone

**Date:** 2026-09-23 18:59
**Author:** Claude Opus 5.5
**Type:** Networking, time

**Summary:**
The top bar clock shows local time (F-008, D-029): NTP for the time, the current UTC offset from ip-api.com for the public IP, refreshed hourly, 24-hour format.

**Changes:**
- `src/clock.h`, `.cpp` - New. NTP start, timezone lookup with a plain HTTP/1.0 GET, clock text.
- `lib/clock_logic/src/clock_logic.h`, `.cpp` - New. ip-api reply parsing, HTTP body split, `HH:MM` formatting.
- `test/test_clock_logic/test_main.cpp` - New. 5 tests.
- `src/main.cpp` - Clock polling and top bar text.
- `platformio.ini` - `FW_VERSION` bumped to `0.0.13`.
- `PROJECT.md` - F-007 done, F-008; D-029; open questions and memory baseline.

**Verification:**
- `pio test -e native`: 81 of 81 tests passed.
- `pio run -e tdisplay` succeeded: RAM 86,832 bytes (26.5%) static, flash 1,781,937 bytes (90.6%). A first version with HTTPClient reached 98.0% flash and was replaced.
- On device: `Clock: timezone <timezone>, UTC offset +7200 s`, `Clock: NTP time received`; screenshot shows 20:58 with the host at 18:58 UTC. Free heap 156,724 bytes.

**Git commit:**
- `v0.0.13 - Add clock with NTP and IP-based timezone`

## 0.0.12 - Redesign top bar and add battery level

**Date:** 2026-09-23 18:59
**Author:** Claude Opus 5.5
**Type:** Display, power

**Summary:**
Top bar redesigned (F-007, D-028): dark slate bar with separate pills; the queue position moved right into a bold amber pill; the network pill has a connection status stripe. Battery level from GPIO34 shown as an icon, with a lightning bolt on USB power.

**Changes:**
- `src/screen.h`, `.cpp` - `StatusBar` for the top bar contents; new bar drawing, battery icon, status stripe; bar 20 px, title and value moved down 2 px.
- `src/battery.h`, `.cpp` - New. ADC_EN high, averaged calibrated reading times 2.
- `lib/ui_logic/src/battery_level.h`, `.cpp` - New. USB detection and Li-ion curve.
- `src/main.cpp` - Status bar contents, battery reading every 10 s, serial command `B`.
- `test/test_ui_logic/test_main.cpp` - 2 battery level tests (30 total).
- `platformio.ini` - `FW_VERSION` bumped to `0.0.12`.
- `PROJECT.md` - F-007, screen layout, modules; D-028.
- `BOARDS.md` - Battery sense reading on USB power.

**Verification:**
- `pio test -e native`: 76 of 76 tests passed.
- `pio run -e tdisplay` succeeded: RAM 85,732 bytes (26.2%) static, flash 1,773,597 bytes (90.2%).
- On device: screenshots of the bar (a first version with battery text did not leave room for the IP address with a long queue position, so the battery became icon-only); `B` reads 4,765 mV on USB.

**Git commit:**
- `v0.0.12 - Redesign top bar and add battery level`

## 0.0.11 - Add screenshot over serial

**Date:** 2026-09-23 18:59
**Author:** Claude Opus 5.5
**Type:** Tooling

**Summary:**
The firmware sends its frame buffer over serial on the `S` command, and `tools/screenshot.py` saves it as a PNG (F-012). Used to check screen designs without a camera.

**Changes:**
- `src/screen.h`, `.cpp` - `sendScreenshot`: RGB332 frame as hex rows.
- `src/main.cpp` - Serial command handling.
- `tools/screenshot.py` - New. Captures without resetting the board and writes a PNG.
- `platformio.ini` - `FW_VERSION` bumped to `0.0.11`.
- `PROJECT.md` - F-012; verification command.
- `BOARDS.md` - Port opening order that avoids a reset.

**Verification:**
- `pio test -e native`: 74 of 74 tests passed.
- `pio run -e tdisplay` succeeded: RAM 85,508 bytes (26.1%) static, flash 1,764,577 bytes (89.8%).
- On device: screenshot of the message screen captured; the first attempt reset the board (DTR released before RTS), the fixed order does not.

**Git commit:**
- `v0.0.11 - Add screenshot over serial`

## docs - Record Claude Code connection to the MCP server

## 0.0.10 - Remove demo messages

**Date:** 2026-09-23 17:56
**Author:** Claude Opus 5.5
**Type:** Cleanup

**Summary:**
The temporary demo messages (F-011) are removed now that messages arrive over MCP (F-003). They were added whenever nothing was restored, so an emptied queue refilled with demos after each reboot.

**Changes:**
- `src/main.cpp` - `addDemoMessages` and its call removed.
- `platformio.ini` - `FW_VERSION` bumped to `0.0.10`.
- `PROJECT.md` - F-011 deprecated; modules table.

**Verification:**
- `pio run -e tdisplay` succeeded: RAM 85,508 bytes (26.1%) static, flash 1,764,261 bytes (89.7%). No new warnings.
- On device: with the empty saved queue, `Restored 0 messages`, no demo messages; the IP message was added and saved as the only message (105 bytes).

**Git commit:**
- `v0.0.10 - Remove demo messages`

## 0.0.9 - Add welcome screen and IP message

**Date:** 2026-09-23 17:52
**Author:** Claude Opus 5.5
**Type:** Display, Wi-Fi

**Summary:**
After connecting, a 3 s welcome screen shows the network, the IP address, and the MCP URL, and a timed IP message with id `ip` is added to the queue (F-009, D-027).

**Changes:**
- `src/main.cpp` - Connection announcement: welcome screen once per boot, IP message on first connect or IP change; one cover path for the setup and welcome screens.
- `src/screen.h`, `.cpp` - Welcome screen.
- `platformio.ini` - `FW_VERSION` bumped to `0.0.9`.
- `PROJECT.md` - F-009; D-027.

**Verification:**
- `pio test -e native`: 74 of 74 tests passed (no logic library changes).
- `pio run -e tdisplay` succeeded: RAM 85,508 bytes (26.1%) static, flash 1,765,505 bytes (89.8%). No new warnings.
- On device: 4 messages restored, IP message added, 5 messages saved (1,036 bytes); MCP `queue_status` reported 5 of 30. The user then scrolled and deleted all messages with the buttons (serial log).
- Not yet: the welcome screen by eye.

**Git commit:**
- `v0.0.9 - Add welcome screen and IP message`

## 0.0.8 - Add MCP server with show_message and queue_status

**Date:** 2026-09-23 17:47
**Author:** Claude Opus 5.5
**Type:** Networking, MCP

**Summary:**
MCP server on the device (F-003, D-025): Streamable HTTP without streaming or sessions at `/mcp` on port 80, mDNS name `screenapi.local`, Origin check. Tools `show_message` and `queue_status` with error texts an LLM can act on. Non-ASCII text is mapped or replaced for the ASCII fonts (D-026). Queue-full popup on the screen (F-005).

**Changes:**
- `lib/mcp_protocol/src/mcp_handler.h`, `.cpp` - New. JSON-RPC handling, version negotiation, tool schemas, argument validation, results.
- `lib/mcp_protocol/src/text_clean.h`, `.cpp` - New. UTF-8 to ASCII for the fonts.
- `src/mcp_server.h`, `.cpp` - New. WebServer routes, Origin check, mDNS, request log.
- `src/screen.h`, `.cpp` - Queue-full popup.
- `src/main.cpp` - Start the MCP server; poll it; popup and save on MCP changes.
- `test/test_mcp_protocol/test_main.cpp` - New. 16 tests.
- `platformio.ini` - ArduinoJson 7.4.3 for both environments; `FW_VERSION` bumped to `0.0.8`.
- `PROJECT.md` - F-003, F-005, modules, verification commands; D-025, D-026; open questions updated.
- `BOARDS.md` - mDNS observation.

**Verification:**
- `pio test -e native`: 74 of 74 tests passed.
- `pio run -e tdisplay` succeeded: RAM 85,476 bytes (26.1%) static, flash 1,764,553 of 1,966,080 bytes (89.7%). No new warnings.
- On device, from a machine on the same LAN: initialize (175 ms), notifications/initialized (202), tools/list, show_message (78 ms, test message shown), queue_status, validation error, GET 405, foreign Origin 403. mDNS query for `screenapi.local` answered with <device IP>. Free heap 159,824 bytes after boot.
- Not yet: a session from Claude Code; the queue-full popup on the device.

**Git commit:**
- `v0.0.8 - Add MCP server with show_message and queue_status`

## 0.0.7 - Add Wi-Fi with BLE provisioning

**Date:** 2026-09-23 17:25
**Author:** Claude Opus 5.5
**Type:** Wi-Fi, BLE, partitions

**Summary:**
Wi-Fi station with ESP BLE Provisioning (F-002, D-023): setup screen with QR code, device name, and random code when no network is saved; IP, `connecting`, or `no network` in the top bar (F-007); retry every 15 s while offline; both buttons held 5 s forget the network (F-006). Partition table changed to `min_spiffs.csv` (D-024), approved by the user; the saved messages were erased once. Fixed Arduino releasing the BLE memory at boot (Q-005).

**Changes:**
- `src/network.h`, `.cpp` - New. Provisioning start, device name and code, QR payload, state from Wi-Fi events, reconnect, reset; `btInUse()` override.
- `src/screen.h`, `.cpp` - Setup screen with QR code (ESP-IDF `qrcode` component), notice screen, network label in the top bar.
- `src/main.cpp` - Network start during the boot screen, setup mode, button pair with Wi-Fi reset, countdown paused during setup, heap log after connect.
- `lib/ui_logic/src/button_pair.h`, `.cpp` - New. Two buttons with a both-held event; single actions suppressed during a two-button hold.
- `lib/ui_logic/src/button_tracker.h` - `idle()`.
- `lib/message_queue/src/message_queue.h` - `pauseCountdown()`.
- `test/test_ui_logic/test_main.cpp` - 5 button pair tests (28 total).
- `test/test_message_queue/test_main.cpp` - 1 pause test (23 total).
- `platformio.ini` - `board_build.partitions = min_spiffs.csv`; `FW_VERSION` bumped to `0.0.7`.
- `PROJECT.md` - F-002, F-006, F-007, storage, modules; D-023, D-024; memory baseline and open questions.
- `BOARDS.md` - Partition table, filesystem, NVS, BLE, Wi-Fi findings; Q-005.

**Verification:**
- `pio test -e native`: 58 of 58 tests passed.
- `pio run -e tdisplay` succeeded: RAM 81,204 bytes (24.8%) static, flash 1,658,345 of 1,966,080 bytes (84.3%). No new warnings.
- Flashed. First boot: LittleFS formatted at the new location, demo messages added and saved. The board had a saved network (`<SSID>`): connected, IP <device IP>. After the Q-005 fix: no `bt_mem_release ... failed 259` errors, `btInUse` returns 1, free heap 164,836 bytes, largest block 98,292 bytes.
- Not yet tested on the device: setup screen, pairing with the app, wrong password, Wi-Fi reset.

**Git commit:**
- `v0.0.7 - Add Wi-Fi with BLE provisioning`

## 0.0.6 - Save messages across reboots

**Date:** 2026-09-23 17:00
**Author:** Claude Opus 5.5
**Type:** Storage

**Summary:**
Messages survive reboot (F-004, D-009): the queue is saved as `/queue.bin` on LittleFS 1 s after the last change and restored at boot (D-022). Timed messages restart their full duration (D-017). Demo messages are added only when nothing was restored (F-011). The `spiffs` partition was blank and is formatted as LittleFS on first boot; the user approved the format.

**Changes:**
- `lib/message_queue/src/queue_codec.h`, `.cpp` - New. Versioned binary format with checksum; decode validates everything before changing the queue.
- `src/storage.h`, `.cpp` - New. Mount (format if needed), load, save through a temporary file and rename.
- `src/main.cpp` - Load at boot, demo messages only when nothing restored, save 1 s after the last content change (delete, clear all, expiry, demo add); scrolling does not save.
- `test/test_queue_codec/test_main.cpp` - New. 7 tests.
- `platformio.ini` - `FW_VERSION` bumped to `0.0.6`.
- `PROJECT.md` - F-004 done, F-011, storage section, modules, D-022, memory baseline; persistence open question closed.
- `BOARDS.md` - Partition table; LittleFS findings (first-mount log, exists() log, rename, write time).

**Verification:**
- `pio test -e native`: 52 of 52 tests passed.
- `pio run -e tdisplay` succeeded: RAM 44,652 bytes (13.6%) static, flash 357,929 bytes (27.3%). No new warnings.
- Before flashing: read back the whole `spiffs` partition; all bytes 0xFF.
- Boot 1: formatted, `Restored 0 messages`, 5 demo messages saved (1,030 bytes, 65 ms); expiry at 32.5 s, 4 saved (939 bytes, 70 ms).
- Boot 2 (reset): `Restored 4 messages`, no demo messages added. Same result after reflashing with the scroll-save fix.

**Git commit:**
- `v0.0.6 - Save messages across reboots`

## 0.0.5 - Add inline text colors and move countdown right

**Date:** 2026-09-23 16:40
**Author:** Claude Opus 5.5
**Type:** Display

**Summary:**
Parts of a message value can have their own color with inline tags such as `{green}ok{/}` (D-021, F-005). The countdown box moved to the bottom right (D-020). Demo messages show both (F-011).

**Changes:**
- `lib/ui_logic/src/markup.h`, `.cpp` - New. Splits markup into visible text and a color per character.
- `src/screen.cpp` - Parses markup before wrapping; draws each line as color runs; relayout also on color change; countdown box at the bottom right.
- `src/main.cpp` - Demo messages with inline colors; countdown text says bottom right.
- `test/test_ui_logic/test_main.cpp` - 7 markup tests (23 total).
- `platformio.ini` - `FW_VERSION` bumped to `0.0.5`.
- `PROJECT.md` - F-003, F-005, F-011; D-020 updated; D-021 added.

**Verification:**
- `pio test -e native`: 45 of 45 tests passed.
- `pio run -e tdisplay` succeeded: RAM 44,596 bytes (13.6%) static, flash 312,609 bytes (23.9%). No new warnings.
- Flashed. Serial: `ScreenAPI v0.0.5`, `Queue: 5 messages`, free heap 294,756 bytes; first message expired at 32.3 s.
- Not yet checked by eye: colored parts and the countdown box position.

**Git commit:**
- `v0.0.5 - Add inline text colors and move countdown right`

## docs - Fix stale verification note in BOARDS.md

## 0.0.4 - Count down timed messages only while shown

**Date:** 2026-09-23 16:35
**Author:** Claude Opus 5.5
**Type:** Core logic, display

**Summary:**
A time-driven message now counts down only while it is on screen, and its remaining time shows in a small box at the bottom left (D-020, F-004, F-005). The demo shows the 30 s message first (F-011).

**Changes:**
- `lib/message_queue/src/message_queue.h`, `.cpp` - `remainingMs` replaces `expiresAtMs`; `tick(now)` replaces `expire(now)` and counts down only the shown message; `add()` no longer takes the time.
- `lib/ui_logic/src/countdown.h`, `.cpp` - New. Seconds rounded up; `45s`, `4:05`, `1:02:03` format.
- `src/screen.cpp`, `src/screen.h` - Countdown box; redraw when the shown number changes.
- `src/main.cpp` - `tick()` before button handling; demo order and texts.
- `test/test_message_queue/test_main.cpp` - Expiry tests replaced by 7 countdown tests (22 total).
- `test/test_ui_logic/test_main.cpp` - 2 countdown format tests (16 total).
- `platformio.ini` - `FW_VERSION` bumped to `0.0.4`.
- `PROJECT.md` - F-004, F-005, F-011 behavior; D-020; queue size.

**Verification:**
- `pio test -e native`: 38 of 38 tests passed.
- `pio run -e tdisplay` succeeded: RAM 43,564 bytes (13.3%) static, flash 312,109 bytes (23.8%). No new warnings.
- On device: the user scrolled away from the 30 s message after 13.96 s and back 12.54 s later; it expired 16.06 s after returning, 30.02 s of time on screen in total (serial log). The 60 s message, shown for 5.3 s, did not expire.
- Not yet checked by eye: the countdown box.

**Git commit:**
- `v0.0.4 - Count down timed messages only while shown`

## docs - Record 0.0.3 on-device verification

**Date:** 2026-09-23 16:31
**Author:** Claude Opus 5.5
**Type:** Documentation

**Summary:**
The user checked 0.0.3 on the device (screen, scrolling, buttons) and reported it looks good. F-006 marked Done; F-005 verification recorded; 0.0.3 recorded as known-good.

**Changes:**
- `PROJECT.md` - F-005 verification, F-006 status and verification.
- `BOARDS.md` - No visible flicker; known-good firmware 0.0.3.
- `CHANGELOG.md` - This entry.

**Verification:**
- No build: documentation only. Device check reported by the user.

**Git commit:**
- `docs - Record 0.0.3 on-device verification`

## 0.0.3 - Add message screen, auto-scroll, and buttons

**Date:** 2026-09-22 21:44
**Author:** Claude Opus 5.5
**Type:** Display, input

**Summary:**
The firmware now uses the message queue. Message screen with top bar, title, and word-wrapped value (F-005); a long title scrolls horizontally and a long value vertically (D-018). Delete, hold to clear all, and scroll buttons (F-006). Top bar shows queue position and count (F-007). Demo messages at boot until MCP exists (F-011). Title and value limits raised to 64 and 512 characters (D-016).

**Changes:**
- `src/screen.cpp`, `src/screen.h` - New. Layout, off-screen 8-bit sprite rendering, word wrap and auto-scroll, top bar, empty state, boot screen.
- `src/main.cpp` - Buttons, expiry, screen updates, demo messages, 64-bit uptime clock, serial log of actions and free heap.
- `lib/ui_logic/src/text_wrap.*` - New. Word wrap with a caller-supplied width function.
- `lib/ui_logic/src/scroll_offset.*` - New. Pause, move, pause, jump-back scroll timing.
- `lib/ui_logic/src/button_tracker.*` - New. Debounce, short press on release, long press while held.
- `lib/message_queue/src/message_queue.h` - Title limit 64, value limit 512.
- `test/test_ui_logic/test_main.cpp` - New. 14 tests for wrap, scroll timing, and buttons.
- `platformio.ini` - `FW_VERSION` bumped to `0.0.3`.
- `PROJECT.md` - Screen layout; F-004 to F-007, F-010 updated; F-011 added; D-016 changed; D-018, D-019 added; memory baseline.
- `BOARDS.md` - Chip ESP32-D0WDQ6 v1.0; Q-001 verified (CP2104); GPIO35 idle HIGH; rotation, refresh, boot log, serial capture note.

**Verification:**
- `pio test -e native`: 32 of 32 tests passed (18 queue, 14 UI logic).
- `pio run -e tdisplay` succeeded: RAM 43,788 bytes (13.4%) static, flash 310,441 bytes (23.7%). No new warnings.
- Flashed to the board. Serial: `ScreenAPI v0.0.3`, `Queue: 5 messages` at 2.3 s, free heap 295,564 bytes, largest block 110,580 bytes; timed demo messages expired at 32.3 s and 62.3 s; no phantom button events in 68 s.
- Not yet checked: screen appearance, scrolling smoothness, flicker, and button presses (needs the user at the device).

**Git commit:**
- `v0.0.3 - Add message screen, auto-scroll, and buttons`

## 0.0.2 - Add message queue with native tests

**Date:** 2026-09-22 21:32
**Author:** Claude Opus 5.5
**Type:** Core logic

**Summary:**
Hardware-independent message queue (F-004): 30 messages, newest first, replace by id, drop when full, delete, clear all, scroll with wrap, and expiry of time-driven messages. Unit tests run on the host. The firmware does not use the queue yet, so device behavior is unchanged. Queue capacity changed from 100 to 30 (D-008).

**Changes:**
- `lib/message_queue/src/message_queue.h` - New. Message layout, limits, and queue interface.
- `lib/message_queue/src/message_queue.cpp` - New. Validation, ordering, replace by id, delete, clear, scroll, expiry.
- `test/test_message_queue/test_main.cpp` - New. 18 Unity tests.
- `platformio.ini` - New `native` env for host tests; `FW_VERSION` bumped to `0.0.2`.
- `PROJECT.md` - F-004 behavior and status; F-003, F-006, storage updated; D-008 changed to 30; D-014 to D-017 added; open questions updated.

**Verification:**
- `pio test -e native`: 18 of 18 tests passed.
- `lib/message_queue` compiles without warnings with the ESP32 toolchain (gnu++11, -Wall -Wextra) and host g++ (-Wpedantic). Queue size on ESP32: 6,728 bytes.
- `pio run -e tdisplay` succeeded: RAM 21,772 bytes (6.6%), flash 296,725 bytes (22.6%); same as 0.0.1 because src/ does not use the queue yet.
- Not flashed: no behavior change on the device.

**Git commit:**
- `v0.0.2 - Add message queue with native tests`

## docs - Record 0.0.1 on-device verification

**Date:** 2026-09-22 21:23
**Author:** Claude Opus 5.5
**Type:** Documentation

**Summary:**
The user flashed 0.0.1 on the T-Display and confirmed it works. F-010 marked Done; 0.0.1 recorded as known-good; boot serial log and Q-002 verification recorded.

**Changes:**
- `PROJECT.md` - F-010 status Done with on-device verification; board status.
- `BOARDS.md` - Board status, Q-002 verified, boot serial log, known-good firmware 0.0.1.
- `CHANGELOG.md` - This entry.

**Verification:**
- No build: documentation only. Device output reported by the user.

**Git commit:**
- `docs - Record 0.0.1 on-device verification`

## 0.0.1 - Add PlatformIO project and boot screen

**Date:** 2026-09-22 21:18
**Author:** Claude Opus 5.5
**Type:** Firmware bring-up

**Summary:**
First firmware (F-010). PlatformIO project for the LilyGO TTGO T-Display with FW_VERSION defined once, a single pin header, and TFT_eSPI configured from the project. On boot the device prints its version on serial and shows it on screen.

**Changes:**
- `platformio.ini` - New. Shared `[env]` defines `FW_VERSION` `0.0.1`; env `tdisplay` (espressif32 7.0.1, board `lilygo-t-display`, Arduino, TFT_eSPI 2.5.43, `-Iinclude`).
- `include/pins.h` - New. All T-Display pin assignments.
- `include/tft_setup.h` - New. TFT_eSPI setup for the ST7789V 135 x 240 panel; includes pins.h.
- `src/main.cpp` - New. Serial version line and centered boot screen.
- `PROJECT.md` - F-010, D-013, repository layout, supported board env; open question on esp_app_desc.
- `BOARDS.md` - Pin table mirrors include/pins.h; env `tdisplay`; Q-002 workaround; on-device verification steps.

**Verification:**
- `pio run -e tdisplay` succeeded: RAM 21,772 bytes (6.6%), flash 296,725 bytes (22.6%).
- Preprocessor check of the TFT_eSPI library compile: ST7789_DRIVER, T-Display pins, CGRAM_OFFSET, 40 MHz SPI (D-013).
- Only warning: TFT_eSPI notes TOUCH_CS is not defined (no touch on this board).
- No native tests: no hardware-independent logic yet.
- Not flashed: no board connected. On-device check per F-010 still pending.

**Git commit:**
- `v0.0.1 - Add PlatformIO project and boot screen`

## docs - Specify message format, queue, UI, and time features

**Date:** 2026-09-22 21:11
**Author:** Claude Opus 5.5
**Type:** Documentation

**Summary:**
Recorded user decisions: Arduino framework, button mapping, title and value message format, 100-message persistent queue with drop-on-full, DHCP IP on welcome screen and in the queue, no MCP authentication, top status bar, NTP clock with IP-based timezone. Added features F-007 to F-009 and decisions D-006 to D-012. No firmware; version stays 0.0.0.

**Changes:**
- `PROJECT.md` - Scope, modules, data flow, screen layout; F-002 to F-006 updated; new F-007 (top bar), F-008 (NTP clock, IP timezone), F-009 (welcome screen, IP message); D-004 and D-005 refined, D-006 to D-012 added; open questions narrowed.
- `BOARDS.md` - Framework set to Arduino; button roles and Q-003 workaround follow D-007.
- `CHANGELOG.md` - This entry.

**Verification:**
- No build: no firmware exists yet.

**Git commit:**
- `docs - Specify message format, queue, UI, and time features`

## docs - Document project purpose and T-Display board

**Date:** 2026-09-22 20:57
**Author:** Claude Opus 5.5
**Type:** Documentation

**Summary:**
Recorded the project purpose (desk display with an on-device MCP server), planned features F-002 to F-006, design decisions D-001 to D-005, and the LilyGO TTGO T-Display as first target board with quirks Q-001 to Q-004. No firmware; version stays 0.0.0.

**Changes:**
- `PROJECT.md` - Purpose and scope, planned modules and data flow, supported board, features F-002 to F-006, design decisions, open questions.
- `BOARDS.md` - LilyGO TTGO T-Display section from vendor README, schematic, TFT_eSPI setup, factory test, and PlatformIO board definition; quirks Q-001 to Q-004.
- `CHANGELOG.md` - This entry.

**Verification:**
- No build: no firmware exists yet. Board data comes from vendor documentation and is not verified on the unit.

**Git commit:**
- `docs - Document project purpose and T-Display board`

## docs - Add project documentation scaffold

**Date:** 2026-09-22 20:20
**Author:** Claude Opus 5.5
**Type:** Documentation

**Summary:**
Initial documentation and repository scaffold (F-001). No firmware; version stays 0.0.0.

**Changes:**
- `AGENTS.md` - Agent rules: documentation, sources of truth, versioning, Git workflow, engineering rules.
- `CLAUDE.md` - Points to AGENTS.md.
- `PROJECT.md` - Project and feature documentation scaffold with TBD placeholders; F-001 entry.
- `BOARDS.md` - Board index and per-board template.
- `CHANGELOG.md` - Change history with format definition.
- `.gitignore` - PlatformIO and secrets ignores.

**Verification:**
- No build: no firmware exists yet.

**Git commit:**
- `docs - Add project documentation scaffold`
