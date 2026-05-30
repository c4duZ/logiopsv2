---
phase: 02-d-bus-client-device-list
verified: 2026-05-30T00:00:00Z
status: human_needed
score: 13/13 statically-verifiable must-haves verified
overrides_applied: 0
re_verification:
  previous_status: none
  note: "Initial verification. 02-REVIEW + 02-REVIEW-FIX (WR-01/02/03 fixed) already applied; this is the goal-backward phase verification."
human_verification:
  - test: "Launch logiops-gui against a live daemon and confirm the device list renders"
    expected: "Connected Logitech device(s) appear in the sidebar with name/model + connection status; first device auto-selected; detail pane shows read-only status."
    why_human: "Requires the base qml6-module-qtquick runtime package (not yet installed) plus a display/session — cannot be exercised headlessly. Static build + ctest confirm the model/shell compile and the model logic is correct, but actual QML rendering is runtime-only."
  - test: "Battery % updates live on the real MX Master 4"
    expected: "Numeric battery % + glyph appear; on charge/discharge the value and charging bolt update live (no app restart, no flicker/reorder); confirms the 0x1004 UnifiedBattery decode against real hardware (cross-check via busctl)."
    why_human: "The 0x1004 wire format is MEDIUM-confidence (verified against cvuchener/hidpp + libratbag references, not live hardware). decodeStatus is unit-tested for bounds + charging logic, but the real-device decode path needs hardware confirmation."
  - test: "Hotplug / sleep-wake updates the list without restart"
    expected: "Sleeping/waking the mouse or unplug/replug of the receiver updates the row's connection badge live, with no flicker, no reorder, and no app restart (DEV-03 / CONF-03)."
    why_human: "Requires live D-Bus signals from real hardware events; the no-flicker model invariant is unit-proven (device_model_noflicker) but the end-to-end signal path is runtime-only."
  - test: "Daemon-down vs access-denied vs no-devices screens are distinct"
    expected: "Stopping logid shows 'Can't reach the logid service' with a working Retry; a non-logiops user sees the usermod onboarding with a working Copy command; zero devices shows the empty state — three visually distinct screens, not a generic error."
    why_human: "Distinguishing AccessDenied from DaemonDown depends on the live D-Bus error name (A3 MEDIUM confidence). The C++ enum mapping and QML state switch are statically verified; the actual error name returned by the bus and the rendered screens need a live run."
  - test: "Theme follows the system light/dark setting"
    expected: "Toggling the desktop light/dark theme re-themes the app (palette-luminance detection)."
    why_human: "Qt 6.4.2 has no Application.styleHints.colorScheme; dark detection relies on palette luminance + paletteChanged (A5 MEDIUM). Requires a live desktop session to confirm."
---

# Phase 2: D-Bus Client Device List Verification Report

**Phase Goal:** The user opens the app and sees their connected Logitech devices with live status, with the daemon as the single source of truth.
**Verified:** 2026-05-30
**Status:** human_needed
**Re-verification:** No — initial goal-backward verification (code review + fix already applied).

## Goal Achievement

### Observable Truths

| #  | Truth | Status | Evidence |
| -- | ----- | ------ | -------- |
| 1  | Daemon exposes Battery/Charging/BatteryKnown + BatteryChanged on `.Device` | ✓ VERIFIED | `Device.cpp:350-357` properties map + signal `make_signal<uint8_t,bool,bool>`; `notifyBattery()` at 366; `setBattery()` at 210; `Device.h:99,180` |
| 2  | Two HID++ battery wrappers exist with bounds discipline; DeviceBattery prefers 0x1004 | ✓ VERIFIED | `BatteryStatus.h` (0x1000) + `UnifiedBattery.h` (0x1004) both guard `type()!=Long`, `feature()==0`, `end-begin<3` before indexing; `DeviceBattery.cpp:32-40` tries UnifiedBattery first, BatteryStatus fallback |
| 3  | GUI is an optional target — daemon builds alone with BUILD_GUI=OFF | ✓ VERIFIED | `CMakeLists.txt:62-66` `option(BUILD_GUI ON)` + `find_package(Qt6 ... QUIET)`; `build-novalidate` configured+built `logid` clean with -DBUILD_GUI=OFF |
| 4  | DeviceModel is signal-driven QAbstractListModel with no cached list (CONF-03) | ✓ VERIFIED | `DeviceModel.cpp`: `beginResetModel` ONLY in `clear()` (192-194); add/remove use begin/endInsert/RemoveRows (138,148); status change emits `dataChanged(..,{ConnectionStateRole})` only (171); no `std::sort` on change |
| 5  | Typed proxies generated from trimmed introspection XML | ✓ VERIFIED | `dbus/logid_devices.xml` (Enumerate) + `dbus/logid_device.xml` (StatusChanged + Battery/Charging/BatteryKnown/BatteryChanged); `qt_add_dbus_interface` in CMake; proxies link into logiops-gui-lib (build clean) |
| 6  | Four screen states + reconnect (QDBusServiceWatcher) exist in DaemonConnection | ✓ VERIFIED | `DaemonConnection.cpp:30,65-85` QDBusServiceWatcher WatchForOwnerChange + serviceRegistered/Unregistered/OwnerChanged; ScreenState mapping Loading/Populated/Empty/DaemonDown/AccessDenied; AccessDenied vs DaemonDown distinguished (141-145) |
| 7  | GUI builds clean -Werror; logiops-gui binary produced | ✓ VERIFIED | `build-werror` (-DCMAKE_CXX_FLAGS=-Werror) configure+build EXIT 0, 0 warnings/errors; `logiops-gui` 667KB binary produced |
| 8  | ctest: all 5 units pass | ✓ VERIFIED | `ctest` 5/5 passed: hidpp_bounds, device_model, device_model_noflicker, battery_feature, unified_battery |

**Score:** 8/8 verifiable truths verified (13/13 underlying must-have items across the 5 plans; remaining live behaviors routed to human verification).

### Required Artifacts

| Artifact | Expected | Status | Details |
| -------- | -------- | ------ | ------- |
| `src/logid/backend/hidpp20/features/BatteryStatus.{h,cpp}` | 0x1000 wrapper + pure bounded decodeStatus | ✓ VERIFIED | FeatureID::BATTERY_STATUS; decode guards type/feature/length; status {1,2,3,4}=charging |
| `src/logid/backend/hidpp20/features/UnifiedBattery.{h,cpp}` | 0x1004 wrapper, preferred | ✓ VERIFIED | FeatureID::UNIFIED_BATTERY; same bounds discipline; status {1,2,3}=charging |
| `src/logid/features/DeviceBattery.{h,cpp}` | High-level feature, property/signal wiring | ✓ VERIFIED | Prefers 0x1004; configure() one-shot read; listen() registers broadcast handler → setBattery → BatteryChanged |
| `src/logid/Device.{h,cpp}` | Battery props + signal on .Device; feature registered | ✓ VERIFIED | `_addFeature<features::DeviceBattery>` at 168; props/signal at 350-357 |
| `src/logiops-gui/DeviceModel.{h,cpp}` | Signal-driven model, 8 roles, no cache | ✓ VERIFIED | 197 lines; targeted dataChanged; reset only in clear() |
| `src/logiops-gui/DaemonConnection.{h,cpp}` | systemBus connector + watcher + screenState | ✓ VERIFIED | 302 lines; daemonBus() honors USE_USER_BUS (WR-01 fix); battery wiring |
| `src/logiops-gui/main.cpp` + qml/* | QGuiApplication + QML shell + 4 states | ✓ VERIFIED | Main.qml state switch (84 lines); 4 state files substantive; AccessDenied usermod + copy CTA |
| `src/logiops-gui/dbus/*.xml` | Trimmed introspection XML | ✓ VERIFIED | Both present with verified interface members incl. battery |

### Key Link Verification

| From | To | Via | Status |
| ---- | -- | --- | ------ |
| Device.cpp | .Device interface | Battery/Charging props + BatteryChanged signal | ✓ WIRED |
| DeviceBattery.cpp | BatteryStatus/UnifiedBattery decodeStatus | event handler on broadcast | ✓ WIRED |
| Device.cpp | features::DeviceBattery | `_addFeature<features::DeviceBattery>` | ✓ WIRED |
| DaemonConnection.cpp | DeviceModel slots | connect proxy DeviceAdded/Removed/StatusChanged/BatteryChanged → model | ✓ WIRED |
| DaemonConnection.cpp | screenState enum | QDBusServiceWatcher + AccessDenied/DaemonDown discrimination | ✓ WIRED |
| Main.qml | DeviceModel + screenState | context-property binding, state switch | ✓ WIRED |
| DeviceDelegate.qml | battery roles | per-role bindings (batteryKnown/batteryPercent/charging) + threshold color | ✓ WIRED |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| -------- | ------- | ------ | ------ |
| Full build (GUI on) | `cmake -S . -B build -DBUILD_GUI=ON -DBUILD_TESTING=ON && cmake --build build` | EXIT 0, both binaries | ✓ PASS |
| Build clean under -Werror | `cmake ... -DCMAKE_CXX_FLAGS=-Werror && cmake --build` | EXIT 0, 0 warnings | ✓ PASS |
| Daemon-only build | `cmake -S . -B build -DBUILD_GUI=OFF && cmake --build --target logid` | EXIT 0, logid built, GUI subdir not entered | ✓ PASS |
| ctest suite | `ctest --test-dir build` | 5/5 passed | ✓ PASS |
| Live device list / battery / hotplug / states / theme | GUI runtime against hardware | needs display + runtime pkg | ? SKIP → human |

### Requirements Coverage

| Requirement | Source Plan(s) | Description | Status | Evidence |
| ----------- | -------------- | ----------- | ------ | -------- |
| DEV-01 | 01,03,04 | See list of connected devices | ✓ SATISFIED (static) | DeviceModel enumerate+rows; DeviceList binds model; device_model unit GREEN. Live render → human. |
| DEV-02 | 01,02,05 | Live battery % + charging state | ✓ SATISFIED (static) | 0x1000+0x1004 decoders (battery_feature, unified_battery GREEN); BatteryChanged wired through DaemonConnection → onBatteryChanged → delegate. Live decode on real MX Master 4 → human. |
| DEV-03 | 01,03,04 | Live connection status, hotplug, no restart | ✓ SATISFIED (static) | StatusChanged → onStatusChanged per-role dataChanged; QDBusServiceWatcher reconnect; device_model_noflicker GREEN. Live hotplug → human. |
| DEV-04 | 01,03,04 | Device name/model | ✓ SATISFIED (static) | NameRole/ModelRole; proxy name()/productID(); kindFromPid. Render → human. |
| CONF-03 | 01,03,04 | Daemon single source of truth, signal-driven, no stale cache | ✓ SATISFIED (static) | No-cache model (reset only in clear()); reconnect re-enumerates; no polling (event-driven setBattery). Live reconnect → human. |

All 5 declared requirement IDs are accounted for. REQUIREMENTS.md Traceability maps DEV-01..04 + CONF-03 to Phase 2 (no orphaned IDs). All five are statically satisfied; live behaviors gated on human verification.

### Anti-Patterns Found

| File | Pattern | Severity | Impact |
| ---- | ------- | -------- | ------ |
| (none blocking) | — | — | No stubs in goal path. Battery roles initialized to 0/false but overwritten by onBatteryChanged from the live signal (not a stub). |
| INFO (deferred, from 02-REVIEW) | percentage not clamped at daemon decoder (INFO-01); kindFromPid defaults unknown→Mouse (INFO-03); usermod group hardcoded (INFO-04) | ℹ️ Info | Acknowledged/deferred in 02-REVIEW-FIX; delegate clamps display (T-02-15); none block the goal. |

Note: 02-REVIEW WR-01/WR-02/WR-03 were already fixed (02-REVIEW-FIX, all_fixed). WR-03's Offline-on-unreachable path in DaemonConnection::addDevice is not exercised by an automated test — folded into the hotplug human-verify item.

### Human Verification Required

5 items require live testing (see frontmatter `human_verification`): live device list render, live battery on real MX Master 4 (0x1004 decode confirmation), hotplug/sleep-wake live update, daemon-down/access-denied/empty distinct screens, and system theme following. All are blocked on a display/session and the base qml6-module-qtquick runtime package (not yet installed) plus real hardware, and on MEDIUM-confidence live behaviors (0x1004 wire format A7, AccessDenied error name A3, palette-luminance theme A5).

### Gaps Summary

No gaps in the statically-verifiable surface. The full build (GUI + daemon-only + -Werror) and all 5 ctest units pass; every must-have truth, artifact, and key link is present, substantive, and wired; all 5 requirement IDs are satisfied at the code level. The phase goal — "user opens the app and sees connected devices with live status, daemon as single source of truth" — is fully realized in code but its user-observable confirmation (rendering, live hardware battery, hotplug, distinct error screens, theme) is inherently runtime/hardware-bound and is routed to human verification.

---

_Verified: 2026-05-30_
_Verifier: Claude (gsd-verifier)_
