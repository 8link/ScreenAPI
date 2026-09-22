# CHANGELOG.md

**Current firmware version:** 0.0.0 (no firmware yet)

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
