---
plan: 02-01
phase: 02-d-bus-client-device-list
title: Wave 0 — optional GUI target + RED test harness
status: complete
completed: 2026-05-30
requirements: [DEV-01, DEV-02, DEV-03, DEV-04, CONF-03]
key_files:
  created:
    - src/logiops-gui/CMakeLists.txt
    - src/logiops-gui/placeholder.cpp
    - test/DeviceModelTest.cpp
    - test/BatteryStatusTest.cpp
    - test/smoke/device-list.sh
    - test/smoke/access-denied-gui.sh
  modified:
    - CMakeLists.txt
    - test/CMakeLists.txt
---

# Plan 02-01 — Wave 0 Test Harness + Optional GUI Target

## What was built

Established the Phase 2 verification scaffold and the optional Qt 6 GUI build target.

- **Optional GUI target** (`CMakeLists.txt`): new `option(BUILD_GUI ... ON)` guarded by a QUIET `find_package(Qt6 6.4 COMPONENTS Core Gui Qml Quick DBus Svg Test)`. Missing Qt6 (or `-DBUILD_GUI=OFF`) only WARNs — the daemon still builds and the test suite still runs. This is the load-bearing "GUI is optional" contract. `src/logiops-gui/` holds a placeholder target until plans 02-03/04/05 fill it in.
- **RED units** (extend `test/CMakeLists.txt`):
  - `DeviceModelTest.cpp` — drives a fake proxy emitting DeviceAdded/Removed/StatusChanged; asserts the QAbstractListModel rows/roles and the no-flicker contract (per-role `dataChanged`, no `modelReset`). RED until plan 02-03 implements `DeviceModel`.
  - `BatteryStatusTest.cpp` — feeds synthetic HID++ 0x1000 reports incl. a too-short one (must yield `known==false`, no OOB read). RED until plan 02-02 implements the battery decode.
- **Smoke scripts** (graceful-skip, Phase 1 pattern): `test/smoke/device-list.sh` (enumerate live daemon, read Name/Battery, observe StatusChanged) and `test/smoke/access-denied-gui.sh` (non-group user sees onboarding, not a crash).

## Verification

- `cmake -S . -B build -DBUILD_GUI=OFF -DBUILD_TESTING=ON` configures cleanly — daemon-only build with no Qt dependency (the optional-target contract).
- Qt6 6.4.2 present on this machine, so the GUI path also configures.
- The two units are RED as designed (reference `DeviceModel`/battery decode not yet implemented) — they go GREEN in plans 02-03 and 02-02.

## Notes / carry-forward

- `src/ipcgull` is vendored in-tree (not a submodule) — already handled repo-wide.
- The Qt-present `build/` tree is configured implicitly by the first Wave-1 build command (plan-checker INFO note, non-blocking).
