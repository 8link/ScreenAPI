# PROJECT.md

Project and feature documentation. Board hardware details live in BOARDS.md; change history lives in CHANGELOG.md.

## Purpose and scope

- **Purpose:** TBD
- **In scope:** TBD
- **Out of scope:** TBD
- **Current firmware version:** see the header of CHANGELOG.md (mirrors `FW_VERSION` in platformio.ini once it exists).

## Architecture

### Runtime model

- **Execution model (tasks / loop):** TBD
- **Core assignment:** TBD
- **Task priorities:** TBD
- **Tick rates / update intervals:** TBD

### Modules

| Module | Responsibility | Source path |
|--------|----------------|-------------|
| TBD | TBD | TBD |

### Data flow

TBD

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
| TBD | TBD | TBD | TBD | BOARDS.md section TBD |

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

TBD

### Display

TBD

### BLE

TBD

### Wi-Fi

TBD. Credentials are never committed; use `include/secrets.h` (gitignored) with a committed `secrets.h.example` placeholder.

### UART / serial

- **Log format:** TBD
- **Other serial usage:** TBD

### Storage

- **NVS keys:** TBD
- **Filesystem:** TBD
- **Partitions:** TBD

### Power

TBD

### OTA

TBD

## Design decisions

Record decisions that a later change could accidentally undo.

| ID | Decision | Reason | Date |
|----|----------|--------|------|
| D-001 | TBD | TBD | TBD |

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

- Target board not decided.
- Project purpose not defined.
- Pin header name not decided (include/pins.h or include/config.h).
