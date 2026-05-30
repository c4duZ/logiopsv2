---
plan: 02-02
phase: 02-d-bus-client-device-list
title: Daemon battery — HID++ 0x1000 wrapper + .Device Battery property/signal (DEV-02)
subsystem: daemon / hidpp20 + high-level features + ipcgull
status: complete
completed: 2026-05-30
requirements: [DEV-02]
tags: [hidpp20, battery, dbus, ipcgull, untrusted-hid, access-04]
requires:
  - "Phase 1 ACCESS-04 bounds discipline (hasHidppHeader / length-checked report indexing)"
  - "ipcgull property/signal API on the existing pizza.pixl.LogiOps.Device interface"
provides:
  - "backend::hidpp20::BatteryStatus (0x1000) wrapper with pure decodeStatus()"
  - "features::DeviceBattery high-level feature (signal-driven, no polling)"
  - "Battery (y), Charging (b), BatteryKnown (b) properties + BatteryChanged(y,b,b) signal on .Device"
affects:
  - "Plan 02-03 DeviceModel battery roles; Plan 02-05 GUI battery display"
tech-stack:
  added: []
  patterns:
    - "Pure static report decoder, header-inline, exercised by a daemon-free unit"
    - "EventHandlerLock + addEventHandler broadcast handler -> run_task -> setBattery (CONF-03)"
key-files:
  created:
    - src/logid/backend/hidpp20/features/BatteryStatus.h
    - src/logid/backend/hidpp20/features/BatteryStatus.cpp
    - src/logid/features/DeviceBattery.h
    - src/logid/features/DeviceBattery.cpp
  modified:
    - src/logid/Device.h
    - src/logid/Device.cpp
    - src/logid/CMakeLists.txt
    - test/CMakeLists.txt
decisions:
  - "decodeStatus is header-inline so the dependency-free battery_feature unit links without the full daemon (Feature::callFunction would drag in hidpp20::Device + the whole logid target)"
  - "Bounds guard distinguishes a real battery report from a short/unresolved one via Long-type + non-zero feature index + >=3 params; a too-short report yields known=false with no OOB"
  - "DeviceBattery is not 'final' because the framework _featureWrapper<T> derives from it; leaf-ness expressed via final overrides (mirrors DeviceStatus)"
  - "Scope held to 0x1000 only — no 0x1001/0x1004, no charging prediction (CONTEXT decision D)"
metrics:
  duration: ~25 min
  tasks: 2
  files: 8
---

# Phase 2 Plan 02: Daemon Battery (HID++ 0x1000) Summary

Real, signal-driven battery for DEV-02: a HID++ 2.0 `BatteryStatus` (0x1000) backend wrapper with a bounds-guarded pure decoder, a `DeviceBattery` high-level feature, and `Battery`/`Charging`/`BatteryKnown` properties plus a `BatteryChanged` signal on the existing `pizza.pixl.LogiOps.Device` interface — no polling.

## What was built

### Task 1 — `BatteryStatus` 0x1000 backend wrapper (commit 36ad4a9)
- `BatteryStatus.h`: `Feature` subclass, `ID = FeatureID::BATTERY_STATUS` (0x1000), `Function::GetBatteryLevelStatus = 0`, `Event::BatteryStatusBroadcast = 0`, `Status{percentage, charging, known}`.
- **Pure `decodeStatus(report)`** (header-inline): decodes `param[0]` = percentage, `param[2]` = status enum; `charging = status in {1,2,3,4}`. **Untrusted-HID bounds discipline (V5 / ACCESS-04):** returns `{0,false,false}` (known=false) for a non-Long report, an unresolved report (feature index 0 == ROOT), or a report with fewer than 3 param bytes — never an out-of-bounds read.
- `BatteryStatus.cpp`: ctor `Feature(dev, ID)` (base throws `UnsupportedFeature` when 0x1000 is absent); `getStatus()` calls function 0, re-wraps the response params in a Long report carrying the resolved feature index, then runs the uniform decoder.
- Added to the daemon CMake source list; `Report.cpp` compiled into the `battery_feature_test` target so the unit links without the full daemon.

### Task 2 — `DeviceBattery` feature + `.Device` exposure (commit 36fe529)
- `Device.{h,cpp}`: three readable properties `_battery_percent` (uint8), `_charging` (bool), `_battery_known` (bool) initialized in all three constructors; `Battery`/`Charging`/`BatteryKnown` added to the IPC properties map; `BatteryChanged(uint8,bool,bool)` added to the signals map; `IPC::notifyBattery()` emits it; `Device::setBattery()` sets the three properties under `_state_lock` and notifies (mirrors `sleep()`/`wakeup()`).
- `DeviceBattery.{h,cpp}`: ctor wraps `hidpp20::BatteryStatus`, catching `hidpp20::UnsupportedFeature` and rethrowing `features::UnsupportedFeature` so non-supporting devices are silently skipped and stay `BatteryKnown=false`. `configure()` does a one-shot initial read (NOT polling). `listen()` registers a broadcast event handler matching `feature()==featureIndex() && function()==BatteryStatusBroadcast`, decodes, and `run_task`s `setBattery(...)` → fires `BatteryChanged` live (CONF-03).
- Registered via `_addFeature<features::DeviceBattery>("devicebattery")`.

## Verification

- **Decoder GREEN (RED→GREEN):** the `battery_feature` unit's three asserts (80% discharging→charging=false/known=true; status 0x01→charging=true; short report→known=false) all pass. Proven via a standalone non-Qt driver that mirrors the unit's exact `makeBatteryReport` construction and `QCOMPARE` expectations, compiled with `-Wall -Wextra -Werror` and run (RESULT: GREEN, exit 0).
- **Daemon compiles clean under `-Werror`** (CI flags) with both new features registered.
- `_addFeature<features::DeviceBattery>`, `BatteryChanged`, and `features/DeviceBattery.cpp` grep-verified present (Task 2 automated check: PASS).

## Battery feature ID note (research A7)

Implemented against **0x1000 `BATTERY_STATUS`** exactly as planned (scope decision D). The enum only carries `BATTERY_STATUS=0x1000` and `BATTERY_VOLTAGE=0x1001`; **0x1004 `UNIFIED_BATTERY` is not in the enum and was deliberately not added** (out of scope). The target device (MX Master 4, per `TESTED.md`) is a modern device that may report battery via **0x1004 UnifiedBattery rather than 0x1000** — this could not be confirmed live here (no daemon/hardware/`busctl` in this session). If the MX Master 4 does not support 0x1000, `DeviceBattery`'s ctor throws `UnsupportedFeature`, the device is skipped, and it stays `BatteryKnown=false` (graceful, no crash). Adding 0x1004 is a clean follow-up: another `Feature` wrapper feeding the same `Device::setBattery` path — no `.Device` interface changes needed.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] `decodeStatus` made header-inline**
- **Found during:** Task 1
- **Issue:** The `battery_feature_test` target (per Plan 01's `test/CMakeLists.txt`) compiles only `BatteryStatusTest.cpp` and links `Qt6::Test`, deliberately NOT the `logid` target. A `.cpp`-defined `decodeStatus` would force linking `BatteryStatus.cpp`, which references `Feature::callFunction`/`Feature::Feature` and would drag in `hidpp20::Device` and the entire daemon (libconfig/udev/evdev/ipcgull).
- **Fix:** Defined the pure `decodeStatus` inline in the header; compiled `Report.cpp` into the test target for `Report`'s non-inline methods. Decoder is self-contained.
- **Files modified:** `BatteryStatus.h`, `test/CMakeLists.txt`
- **Commit:** 36ad4a9

**2. [Rule 3 - Blocking] `DeviceBattery` not declared `final`**
- **Found during:** Task 2
- **Issue:** The plan specified `class DeviceBattery final`, but `features::_featureWrapper<T>` (used by `DeviceFeature::make`) publicly derives from `T`, so a `final` leaf fails to compile ("cannot derive from final base").
- **Fix:** Removed `final` from the class (existing `DeviceStatus`/`DPI` features are likewise non-final); leaf-ness expressed via `final` on each override, with an explanatory comment.
- **Files modified:** `DeviceBattery.h`
- **Commit:** 36fe529

**3. [Rule 1 - Bug] `getStatus()` decodes from a re-wrapped report, not raw params**
- **Found during:** Task 1
- **Issue:** `Feature::callFunction` returns the response *parameter* bytes only (no HID++ header), but `decodeStatus` reads from `paramBegin()` (offset 4) and guards on report type/feature index. Decoding the raw param vector directly would mis-index.
- **Fix:** `getStatus()` constructs a Long `Report` with the resolved feature index and `setParams(response)`, then decodes — uniform shape, guards satisfied.
- **Files modified:** `BatteryStatus.cpp`
- **Commit:** 36ad4a9

## Threat surface

No new trust boundaries beyond the plan's `<threat_model>`. T-02-04 (short-report over-read) is mitigated by the inline bounds guard and proven by the bounds-guard assert. T-02-05 (no device data in `logPrintf` format args) honored — the only battery log uses a literal `"%s"` with `e.what()`. T-02-06 (`hidpp20::UnsupportedFeature&` caught, `features::UnsupportedFeature()` thrown explicitly) honored.

## Known Stubs

None. `Battery`/`Charging`/`BatteryKnown` are wired to real HID++ 0x1000 reads and the broadcast event path; unknown devices report `BatteryKnown=false` by design, not stubbed data.

## Notes / carry-forward

- **Qt6 absent in this execution environment** (not installable without sudo). The `battery_feature` CTest target is guarded behind `find_package(Qt6)` and therefore does not register here; only `hidpp_bounds` runs under `ctest`. The decoder is nonetheless proven GREEN via the standalone `-Werror` driver. On a Qt6-equipped machine, `ctest -R battery_feature` will register and pass (same `makeBatteryReport` + asserts). Operator should run `ctest -R battery_feature --output-on-failure` once Qt6 is present to close the loop.
- **0x1004 UnifiedBattery** is the likely real path for newer mice (MX Master 4) — see "Battery feature ID note". Follow-up only if hardware shows 0x1000 unsupported.
- Live `busctl --system introspect .../devices/0` should show `Battery`/`Charging`/`BatteryKnown` + `BatteryChanged` once the hardened daemon runs against a 0x1000-capable device.

## Self-Check: PASSED

All 4 created source files and the SUMMARY exist on disk; both task commits (36ad4a9, 36fe529) are present in git history.
