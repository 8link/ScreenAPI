# CHANGELOG.md

**Current firmware version:** 0.0.4

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
