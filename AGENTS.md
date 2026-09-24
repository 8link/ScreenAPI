# AGENTS.md

Rules for any agent working in this repository. Read this file at the start of every task.

## Documentation and project knowledge

- Log every code, configuration, documentation, or firmware change in CHANGELOG.md (format defined in CHANGELOG.md).
- Document every project feature related to an IoT board in PROJECT.md.
- Document every finding about board specifications, quirks, or usage in BOARDS.md: pinouts, power draw, sensor behavior, display behavior, BLE/UART quirks, protocol details, timing limitations, hardware workarounds. Record findings when observed, including negative results.
- Add any new rule given by the user to AGENTS.md. Do not invent project rules independently.
- When adding a rule, place it in the matching section and merge it with overlapping rules instead of appending duplicates. If AGENTS.md grows past roughly 200 lines, propose a consolidation to the user; do not rewrite rules without approval.
- PlatformIO is the required build, flash, and test toolchain. All instructions assume PlatformIO. Do not reference Arduino IDE workflows.

## Sources of truth

Consult the relevant file before making assumptions:

- Project purpose, architecture, firmware behavior, board-related features -> PROJECT.md
- Board specifications, pinouts, peripherals, protocols, quirks -> BOARDS.md
- Change history and current firmware version -> CHANGELOG.md
- Rules, conventions, workflow -> AGENTS.md

Precedence:

- Documentation is authoritative over assumptions or memory.
- Exception: for values the firmware actually compiles (pin numbers, bus addresses, UUIDs, timing constants, version), the source code is the truth and documentation mirrors it. Pin assignments live in one header per board, include/boards/<board>/pins.h; the board's BOARDS.md pin table must match it.
- If documentation and code disagree, do not silently pick one. Flag the discrepancy, state which side you believe is correct and why, and fix it in the same task if in scope; otherwise report it.
- A task is not complete until affected documentation is updated.

## Firmware versioning

- Semantic versioning, MAJOR.MINOR.PATCH, no leading zeros (valid SemVer 2.0.0).
- Initial state: 0.0.0. First firmware change: 0.0.1.
- A version bump happens only for changes that alter the built firmware: anything under src/, include/, lib/, build-relevant settings in platformio.ini, partition tables, or embedded assets.
- Each such completed change increments PATCH by 1, unless the user says otherwise.
- Changes that do not alter the binary (documentation, tests, CI, tooling, .gitignore) do not bump the version. They are still logged and committed with the docs / test / tooling prefix (see Git workflow).
- PATCH rollover: after 0.0.99, the next firmware change is 0.1.0. This automatic rollover is the only case where MINOR increases without an explicit user request.
- MAJOR and any other MINOR increase happen only on explicit user request.
- Single source of truth: the version is defined once, in the `VERSION` file at the repository root (one line, `X.Y.Z`). `tools/version.py` passes it to the firmware as `FW_VERSION`. Keep it out of platformio.ini: any change there makes PlatformIO rebuild everything. All code, display labels, BLE/web metadata, and esp_app_desc must read FW_VERSION. Never hardcode the version anywhere else. If a hardcoded copy is found, report it.
- The version in CHANGELOG.md, FW_VERSION, and the commit message must be identical.

## Git workflow

Before editing:

- Run `git status` and check the current branch.
- Review recent history with `git log` when context matters.
- Inspect existing uncommitted changes. Never overwrite, discard, stash, reset, or revert user changes without explicit approval. Keep unrelated changes out of your commit.

For every completed change:

- One logical change per commit. Do not mix unrelated fixes, refactors, formatting, or features.
- Code, documentation, version bump, and changelog entry for the same change go in the same commit.
- Build with PlatformIO before committing any firmware change: `pio run -e <env>`.
- Run relevant tests where feasible: `pio test -e native` for hardware-independent logic.
- Review `git diff` and `git diff --cached` before committing.
- Stage files explicitly by path. Do not use `git add .` or `git add -A`.
- Commit message format:
  - Firmware change: `vX.Y.Z - Short imperative description` (example: `v0.0.16 - Couple simulated temperature to load`)
  - Non-firmware change: `docs - ...`, `test - ...`, or `tooling - ...`
- Do not amend, squash, rebase, force-push, push, switch or create branches, create tags, or change remotes unless the user explicitly asks.
- Never commit secrets, credentials, Wi-Fi passwords, API keys, private certificates, tokens, or local configuration. Use placeholders and a secrets.h.example pattern; the real file is gitignored.
- Do not commit .pio/, build artifacts, logs, captures, or device dumps unless the user explicitly asks.

After committing:

- Run `git status` and report whether the tree is clean.
- Report: commit hash, commit message, files changed, firmware version, build and test result. If a build or test was not run, say so and why.

## Engineering rules

- Do not guess. If a requirement, pin assignment, protocol, API, or expected behavior is unclear, ask before writing code.
- Read existing code and the four documentation files before relying on memory.
- Verify behavior against source code and hardware documentation. Do not present guesses as verified facts; label unverified claims as such.
- Make the smallest change that solves the problem. Do not refactor, rename, reformat, or restructure unrelated code unless asked.
- Match existing style, naming, structure, and conventions.
- For large, structural, risky, or multi-file changes: explain the plan first and wait for confirmation.
- Write or update tests when changing behavior where feasible. Keep hardware-independent logic separable so it can run in the native environment.
- For hardware-dependent behavior that cannot be unit-tested, document on-device verification: expected serial output, LED/display state, BLE/UART traffic, measured values.
- A firmware change is not complete if it does not compile. If compilation cannot be run, state that explicitly.
- After each change, state what changed, why, how to verify it, and the commit that contains it.
- Ask for confirmation before any operation that can wipe or brick a board: flash erase, NVS erase, partition-table changes, bootloader changes, eFuse writes, filesystem formatting, secure boot or flash encryption changes.
- Keep functions small and readable.
- Comment non-obvious hardware interactions with the reason, not only the action.
- Bugs or suspicious behavior found outside the current task: report them or add them to PROJECT.md Open questions. Do not silently fix or ignore them.
- Use buffered (off-screen) rendering for display refreshes where the driver allows it, to avoid visible flicker.

## Status messages on the desk display

Report task progress on the ScreenAPI display through its MCP tool `show_message` (server `screen`, `http://screenapi.local/mcp`).

- Task start: a timed message, `duration_s` 10, saying what the task is.
- Each major step: a timed message, `duration_s` 10.
- Task end: a confirm message (no `duration_s`, `kind` confirm), so it stays until the user deletes it. Green when the task succeeded, red when it failed or is blocked.
- Use one `id` per task (for example `job-<topic>`, at most 16 characters) for the start, step, and end messages, so each replaces the previous one.
- Keep titles and values short and ASCII.
- If the `screen` tool is not loaded in the session, send the same JSON-RPC `tools/call` with curl (PROJECT.md Verification). If the display cannot be reached, continue the task and say so in the report.
