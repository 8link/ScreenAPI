# ScreenAPI

**A small Wi-Fi desk display for your AI agents.** An ESP32 board with a screen runs an [MCP](https://modelcontextprotocol.io) server on your local network. Claude Code, or any other MCP client, sends it status messages, and the device takes care of showing them: queuing, scrolling, colors, timers, and button or touch controls.

![PlatformIO](https://img.shields.io/badge/build-PlatformIO-orange)
![Arduino](https://img.shields.io/badge/framework-Arduino%20(ESP32)-00979D)
![MCP](https://img.shields.io/badge/MCP-Streamable%20HTTP-6f42c1)
![Boards](https://img.shields.io/badge/boards-ESP32%20%7C%20ESP32--S3-lightgrey)
![Version](https://img.shields.io/badge/firmware-0.0.18-blue)

```text
+----------------------------------------------+
| [|] 192.168.x.x   [ 2/5 ] [bat] [ 14:32 ]    |
| Claude Code                                  |
|----------------------------------------------|
| Build passed, 81 of 81 tests green.          |
| Flashing the AMOLED board next.              |
|                                       [ 8s ] |
+----------------------------------------------+
```

---

## Contents

- [What it does](#what-it-does)
- [How it works](#how-it-works)
- [Features](#features)
- [Hardware](#hardware)
- [Built to be ported](#built-to-be-ported)
- [Getting started](#getting-started)
- [Sending messages](#sending-messages)
- [Development](#development)
- [Project status](#project-status)

---

## What it does

- **Your agents report to your desk.** Claude Code can say "started", "tests running", "done, please review" on a screen next to your keyboard, without you watching the terminal.
- **The device owns the message lifecycle.** Senders only pass data. The display queues up to 30 messages, shows the newest first, scrolls long text, counts down timed messages while they are on screen, and keeps confirm messages until you delete them.
- **Nothing to configure in code.** Wi-Fi is set up from your phone over Bluetooth; the MCP endpoint announces itself on the network as `screenapi.local`.

## How it works

```mermaid
flowchart LR
    A["Claude Code<br/>or any MCP client"] -- "MCP over HTTP<br/>show_message / queue_status" --> B
    subgraph B["ESP32 board"]
        direction TB
        M["MCP server<br/>/mcp on port 80"] --> Q["Message queue<br/>30 slots, id replace,<br/>timers"]
        Q --> S["Screen<br/>top bar, title,<br/>scrolling text"]
        I["Buttons / touch"] --> Q
        Q <--> F["Flash storage<br/>(survives reboot)"]
        N["NTP + timezone<br/>lookup"] --> S
    end
    P["Phone: ESP BLE<br/>Provisioning app"] -. "Wi-Fi setup over BLE" .-> B
```

## Features

| Area | What you get |
|------|--------------|
| **MCP** | Streamable HTTP transport at `http://<hostname>.local/mcp`. Tools `show_message` and `queue_status`. Errors come back as plain sentences an LLM can act on ("color must be one of: white blue green red."). Web pages cannot post to it (Origin check). |
| **Messages** | Title plus a value of up to 512 characters, two font sizes, four colors, and inline color tags: `Build {green}passed{/}, 2 {red}warnings{/}`. Non-ASCII typography (dashes, quotes, ellipsis) is converted for the fonts. |
| **Lifecycle** | *Timed* messages count down only while they are on screen, with the time left in a small box. *Confirm* messages stay until deleted. A message with the same `id` replaces the old one, so a status update never fills the queue. |
| **Queue** | 30 messages, newest first, saved to flash and restored after a reboot. When full, new messages are dropped, a popup says so, and the sender is told. |
| **Screen** | Top bar with connection status, IP address, queue position, battery, and clock (NTP, timezone from your IP). Long titles scroll sideways, long text scrolls down. Flicker-free, full-frame buffered drawing. |
| **Sound** | A short two-note chime for every message sent to the display, on boards with a speaker (Waveshare AMOLED). |
| **Controls** | Delete the shown message, hold to clear all, show the next message; hold both inputs for 5 s to reset Wi-Fi. |
| **Setup** | Wi-Fi via the ESP BLE Provisioning app: scan the QR code on the screen. A welcome screen shows the IP address and MCP URL. |

## Hardware

ScreenAPI runs on ESP32-family boards with Wi-Fi, BLE, and a display. Two boards are supported today; the firmware is structured so that adding more is mostly configuration (see [Built to be ported](#built-to-be-ported)).

| | LilyGO TTGO T-Display | Waveshare ESP32-S3-Touch-AMOLED-1.8 |
|---|---|---|
| **Chip** | ESP32 (dual-core LX6, 240 MHz) | ESP32-S3 (dual-core LX7, 240 MHz) |
| **Memory** | 4 MB flash, no PSRAM | 16 MB flash, 8 MB PSRAM |
| **Display** | 1.14 inch IPS LCD, ST7789, 240 x 135, SPI | 1.8 inch AMOLED, 368 x 448, QSPI (CO5300 or SH8601) |
| **Orientation** | Landscape | Portrait |
| **Input** | Two buttons | BOOT button and capacitive touchscreen |
| **Battery** | Li-ion connector, voltage via ADC | Li-ion connector, AXP2101 power management chip |
| **Sound** | None | ES8311 codec and speaker: chime on new messages |
| **USB** | USB-C with a USB-UART bridge | USB-C, native USB serial |
| **PlatformIO env** | `tdisplay` | `waveshare_amoled18` |
| **Status** | Verified on the device up to firmware 0.0.14 | Boots and runs Wi-Fi setup since 0.0.16 |

### LilyGO TTGO T-Display

A compact, inexpensive ESP32 board with a small, sharp LCD, two buttons, and a battery connector. ScreenAPI uses it in landscape.

- **Controls:** the GPIO35 button deletes the shown message, hold 1.5 s to clear all; the GPIO0 (BOOT) button shows the next message; both held for 5 s reset Wi-Fi.
- **Battery:** read through a 2:1 divider on GPIO34; on USB power the top bar shows a lightning bolt.
- **Vendor:** [LilyGO TTGO T-Display](https://github.com/Xinyuan-LilyGO/TTGO-T-Display)

### Waveshare ESP32-S3-Touch-AMOLED-1.8

A larger, high-density AMOLED touch board with plenty of memory. ScreenAPI uses it in portrait with larger fonts. The board comes in two revisions (SH8601 display with FT3168 touch, and CO5300 with CST820); the firmware detects which one it runs on.

- **Controls:** BOOT deletes the shown message, hold 1.5 s to clear all; a touch on the screen shows the next message; BOOT and touch held for 5 s reset Wi-Fi.
- **Battery:** percentage, USB, and charge state from the AXP2101 power chip.
- **Sound:** a two-note chime when a message arrives, through the ES8311 codec and the onboard speaker.
- **Vendor:** [Waveshare ESP32-S3-Touch-AMOLED-1.8](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8)

Pinouts, measured values, and hardware quirks for each board are in [BOARDS.md](BOARDS.md).

## Built to be ported

The goal is that any ESP32 with Wi-Fi and a display can run ScreenAPI with one small file of board code. The firmware is split into three layers:

```mermaid
flowchart TB
    subgraph L1["lib/ - plain C++, no hardware, 81 unit tests on the PC"]
        direction LR
        q["message_queue<br/>queue, timers,<br/>saved format"] --- u["ui_logic<br/>word wrap, scrolling,<br/>markup, buttons"] --- m["mcp_protocol<br/>JSON-RPC, tools,<br/>text cleanup"] --- c["clock_logic<br/>timezone reply,<br/>time format"]
    end
    subgraph L2["src/ - shared firmware, same for every board"]
        direction LR
        s["screen<br/>layout from<br/>font metrics"] --- n["network<br/>Wi-Fi, BLE setup"] --- h["mcp_server<br/>HTTP, mDNS"] --- st["storage<br/>LittleFS"]
    end
    subgraph L3["src/boards/(board)/hal.cpp - the only board-specific code"]
        direction LR
        t1["tdisplay"] --- t2["waveshare_amoled18"] --- t3["template"]
    end
    L2 --> L1
    L2 --> L3
```

- **`lib/`** holds the logic: the queue and its lifecycle, word wrap and scrolling, the MCP protocol, the clock. It has no Arduino or hardware dependency and is tested on the host with `pio test -e native`.
- **`src/`** is the firmware shared by all boards. The screen layout is computed from the screen size and from the fonts the board chooses, so it adapts to landscape and portrait panels of any size from 240 x 135 up.
- **`src/boards/<board>/hal.cpp`** implements [`src/hal.h`](src/hal.h), a small interface of twelve functions: start the board, return the display driver and fonts, read two inputs, and optionally read the battery and play a chime. Pins and screen size live in `include/boards/<board>/`.

### Porting to your board

1. Copy `include/boards/template/` and `src/boards/template/` to a new board name.
2. Set the screen size, rotation, and hostname in `board.h`, your wiring in `pins.h`.
3. In `hal.cpp`, pick your display driver from [Arduino_GFX](https://github.com/moononournation/Arduino_GFX) (ST7789, ILI9341, GC9A01, CO5300, and many more; SPI, QSPI, parallel, RGB), choose four u8g2 fonts for your pixel density, and map your buttons or touchscreen.
4. Add an env to `platformio.ini` (copy the `template` env), build, and flash.

The `template` env builds the template for a generic ESP32 with an ST7789 SPI display, so the starting point always compiles. The full checklist is in [PROJECT.md](PROJECT.md) under "Adding a board".

**Requirements:** ESP32-family chip with Wi-Fi and BLE (BLE is used for Wi-Fi setup), a display supported by Arduino_GFX with at least 240 x 135 pixels, width x height bytes of RAM for the frame buffer (PSRAM is used when present), two inputs (a touchscreen can be one), and about 1.8 MB for the app.

## Getting started

### 1. Build and flash

Install [PlatformIO](https://platformio.org), connect the board over USB, and flash its environment:

```sh
# LilyGO TTGO T-Display
pio run -e tdisplay -j 2 -t upload

# Waveshare ESP32-S3-Touch-AMOLED-1.8 (always pass -e: the default env is the T-Display)
pio run -e waveshare_amoled18 -j 2 -t upload --upload-port /dev/ttyACM0
```

`-j 2` limits parallel compiler jobs; the U8g2 font source needs about 1 GB of RAM per job.

### 2. Connect it to Wi-Fi

On first start the board shows a QR code. Scan it with the **ESP BLE Provisioning** app (Espressif, Android and iOS), pick your network, and enter the password. The board connects, shows a welcome screen with its IP address, and remembers the network. To change networks later, hold both inputs for 5 seconds.

### 3. Connect Claude Code

```sh
claude mcp add --transport http screen http://screenapi.local/mcp
```

On Linux, `.local` names need an mDNS resolver (`avahi-daemon` and `libnss-mdns`); otherwise use the IP address from the welcome screen.

Every board answers at `screenapi.local`, so one Claude Code setting works whichever board is plugged in. Keep one board online at a time; after switching boards, reconnect with `/mcp` in Claude Code, and allow up to about 2 minutes for the old address to leave the mDNS cache.

## Sending messages

From Claude Code, just ask: *"Show 'Deploy finished' in green on the screen for 30 seconds."*

The `show_message` tool takes:

| Field | Values | Default |
|-------|--------|---------|
| `value` | Text, up to 512 characters; newlines and `{green}...{/}` color tags allowed | required |
| `title` | One line, up to 64 characters; scrolls sideways if too long | empty |
| `color` | `white` (drawn light grey, below the white title), `blue`, `green`, `red` | `white` |
| `font_size` | `small`, `large` | `small` |
| `kind` | `timed` (removed after its time on screen) or `confirm` (stays until deleted) | `timed` if `duration_s` is given, else `confirm` |
| `duration_s` | 1 to 86400 | - |
| `id` | Up to 16 characters; a message with the same id replaces the old one | none |

Any HTTP client works too:

```sh
curl -s http://screenapi.local/mcp -H 'Content-Type: application/json' -d '{
  "jsonrpc": "2.0", "id": 1, "method": "tools/call",
  "params": {"name": "show_message", "arguments": {
    "id": "build", "title": "CI", "value": "Build {green}passed{/}", "duration_s": 30}}}'
```

This repository's [AGENTS.md](AGENTS.md) includes a rule that makes coding agents report their own progress on the display: a short timed message when a task starts and at each major step, and a confirm message when it ends.

## Development

| Task | Command |
|------|---------|
| Unit tests (host, no hardware) | `pio test -e native` |
| Build one board | `pio run -e <env> -j 2` |
| Serial log | `pio device monitor -e <env>` |
| Screenshot of the device screen | `python3 tools/screenshot.py screen.png --port <port>` |
| Bump the firmware version | edit `VERSION` (not platformio.ini: any change there triggers a full rebuild) |

The documentation is part of the code:

- [AGENTS.md](AGENTS.md) - rules for anyone (human or agent) changing the project
- [PROJECT.md](PROJECT.md) - architecture, features (F-xxx), design decisions (D-xxx), open questions
- [BOARDS.md](BOARDS.md) - pinouts, measurements, and quirks (Q-xxx) per board
- [CHANGELOG.md](CHANGELOG.md) - every change, with how it was verified

## Project status

ScreenAPI is an early MVP (firmware 0.0.x). All MVP features are implemented; on-device checks are still open for the latest graphics changes on the T-Display and for the display, touch, and Wi-Fi setup on the AMOLED board. See the open questions in [PROJECT.md](PROJECT.md).

**Security:** the MCP endpoint has no authentication; anyone on your local network can post messages. It rejects requests from web pages (Origin check), but do not expose it to the internet. The clock's timezone lookup uses [ip-api.com](https://ip-api.com), free for non-commercial use.

**License:** not chosen yet. Until a license file is added, all rights are reserved by the author.
