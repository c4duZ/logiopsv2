---
status: partial
phase: 02-d-bus-client-device-list
source: [02-VERIFICATION.md]
started: 2026-05-30
updated: 2026-05-30
---

## Current Test

[awaiting human testing — needs a graphical session, the `qml6-module-qtquick` runtime package, the running logid daemon, and a real Logitech device]

## Tests

### 0. Runtime prerequisite (install first)
expected: The base QtQuick QML module is needed to RUN the GUI (only controls/layouts submodules are installed). Run: `sudo apt install -y qml6-module-qtquick` (plus `qml6-module-qtquick-templates` if not already present).
result: [pending]

### 1. GUI launches and shows the live device list (DEV-01/04)
expected: Build + run (`cmake -S . -B build -DBUILD_GUI=ON && cmake --build build && ./build/src/logiops-gui/logiops-gui`); the sidebar lists connected Logitech devices by name/model; selecting one shows the read-only detail pane.
result: [pending]

### 2. Live battery on the real MX Master 4 (DEV-02 + 0x1004 confirmation)
expected: First confirm the daemon decode via `busctl --system get-property pizza.pixl.LogiOps /pizza/pixl/logiops/devices/0 pizza.pixl.LogiOps.Device Battery` → `y NN` (0..100) with `BatteryKnown=true`. Then in the GUI: numeric % + glyph, threshold color (green >20% / amber ≤20%), charging bolt appears when you plug the charger — all WITHOUT restart. Confirms the 0x1004 UnifiedBattery wire-format on real hardware.
result: PARTIAL — daemon-side CONFIRMED on real MX Master 4 (2026-05-30): `busctl ... Battery → y 50` (0x1004 UnifiedBattery decode works). Also surfaced + fixed a segfault: emitting BatteryChanged during init crashed the daemon (fix commit a31a39e). STILL PENDING: GUI rendering (% glyph, threshold color, live charging bolt) — needs the GUI launched.

### 3. Hotplug / sleep-wake updates without restart, no flicker (DEV-03)
expected: Unplug/replug the receiver and let the mouse sleep/wake; the list updates live (sleeping/offline dimmed-in-place, true unpair removes the row), with no flicker, no row reorder, no full reset.
result: [pending]

### 4. Distinct screens: daemon-down vs access-denied vs no-devices (CONF-03)
expected: `sudo systemctl stop logid` → "daemon not running" screen + "Retry connection" repopulates after start; a user NOT in the `logiops` group → access-denied onboarding with a working "Copy command" (usermod); daemon up with no device → friendly empty state. Three distinct screens.
result: [pending]

### 5. Theme follows the system (UI-01 precursor)
expected: Toggle the desktop light/dark theme; the app follows.
result: [pending]

## Summary

total: 6
passed: 0
issues: 0
pending: 6
skipped: 0
blocked: 0

## Gaps
