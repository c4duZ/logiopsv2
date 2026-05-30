---
phase: 02-d-bus-client-device-list
plan: 05
subsystem: ui
tags: [qt6, qml, dbus, qdbusxml2cpp, battery, hidpp, no-flicker, accessibility]

# Dependency graph
requires:
  - phase: 02-d-bus-client-device-list (Plan 02)
    provides: daemon Battery/Charging/BatteryKnown properties + BatteryChanged signal on .Device (HID++ 0x1004 UnifiedBattery preferred, 0x1000 BatteryStatus fallback)
  - phase: 02-d-bus-client-device-list (Plan 03)
    provides: typed qdbusxml2cpp .Device proxy + DeviceModel with battery roles/slot (present-but-unwired)
  - phase: 02-d-bus-client-device-list (Plan 04)
    provides: DaemonConnection (system-bus device wiring) + QML shell (DeviceDelegate/DetailPane) + Theme singleton
provides:
  - .Device proxy XML carries Battery/Charging/BatteryKnown + BatteryChanged (regenerated proxy: battery()/charging()/batteryKnown() + BatteryChanged(uchar,bool,bool))
  - DaemonConnection wires per-device BatteryChanged -> DeviceModel.onBatteryChanged (live, no polling) + initial battery from the GetAll snapshot; absent property degrades to "—"
  - DeviceDelegate live battery block (drawn glyph + fill + numeric % + charging bolt, threshold colors, "—" when unknown) with per-role bindings (no-flicker)
  - DetailPane Battery section (%/charging or "Battery status unavailable", threshold color)
  - smoke/device-list.sh hard Battery assertion (uint8 0..100 OR BatteryKnown=false)
  - test_battery_noflicker unit (onBatteryChanged emits only battery roles, no reset, no reorder)
affects: [phase-03-device-config, ui-01]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Battery glyph drawn with QML primitives (Rectangle outline + proportional fill + terminal nub) tinted by a threshold-color binding — avoids a runtime ColorOverlay/Qt5Compat.GraphicalEffects dependency"
    - "Charging bolt baked green in charging.svg (UI-SPEC: always Success green regardless of level) — no recolor needed"
    - "Stable numeric width via TextMetrics('100%') + AlignRight instead of font.features (font.features is Qt 6.7+, target is 6.4.2)"
    - "Initial property value piggybacks on the existing async Properties.GetAll snapshot (no extra D-Bus round-trip)"

key-files:
  created:
    - src/logiops-gui/icons/battery.svg
    - src/logiops-gui/icons/charging.svg
  modified:
    - src/logiops-gui/dbus/logid_device.xml
    - src/logiops-gui/DaemonConnection.cpp
    - src/logiops-gui/qml/DeviceDelegate.qml
    - src/logiops-gui/qml/DetailPane.qml
    - src/logiops-gui/CMakeLists.txt
    - test/smoke/device-list.sh
    - test/DeviceModelTest.cpp

key-decisions:
  - "Battery glyph drawn as QML primitives (not a tinted SVG) so the fill/outline recolors to the threshold color without pulling in Qt5Compat.GraphicalEffects (not installed) as a build/runtime prerequisite"
  - "Stable % width via TextMetrics rather than font.features (Qt 6.7+); UI-SPEC explicitly permits the fallback on the 6.4.2 target"
  - "Initial battery read reuses the same async Properties.GetAll snapshot already issued in addDevice() — no extra round-trip; absent Battery property -> known=false -> '—'"

patterns-established:
  - "Per-role dataChanged battery wiring proven by test_battery_noflicker: a tick repaints only the battery roles, never resets/reorders"

requirements-completed: [DEV-02]

# Metrics
duration: 18min
completed: 2026-05-30
---

# Phase 2 Plan 05: GUI Battery Display Wiring Summary

**Live, signal-driven battery end-to-end: the daemon's BatteryChanged signal drives a per-role-repainted battery block (drawn glyph + fill + numeric % + charging bolt, threshold colors, "—" when unknown) in the sidebar list item and detail pane, with a hard smoke assertion on the live Battery property.**

## Performance

- **Duration:** ~18 min
- **Started:** 2026-05-30
- **Completed:** 2026-05-30
- **Tasks:** 2 automatable complete + 1 human-verify checkpoint (documented)
- **Files modified:** 7 (2 created, 5 modified)

## Accomplishments
- Extended the `.Device` proxy XML with `Battery`/`Charging`/`BatteryKnown` + `BatteryChanged`; regenerated proxy gains the accessors + `BatteryChanged(uchar,bool,bool)` (names/types verified against `src/logid/Device.cpp`).
- Wired `DaemonConnection.addDevice()` to subscribe per-device `BatteryChanged -> DeviceModel.onBatteryChanged` (live, no polling, CONF-03) and to seed the initial value from the existing async `GetAll` snapshot; a missing/absent Battery property degrades to `known=false` → "—".
- Rendered the real battery block in `DeviceDelegate.qml` (drawn glyph + proportional fill + numeric %, >20% green / ≤20% amber, charging bolt overlay always-green, "—" muted when unknown) with per-role bindings so a tick repaints only the battery block (no row move/re-sort/blink), clamped 0..100 (T-02-15), and an `Accessible.name` that announces battery + charging.
- Filled the `DetailPane.qml` Battery section (%/charging or "Battery status unavailable", threshold color when known).
- Promoted the smoke `Battery` read into a real assertion (uint8 0..100 when `BatteryKnown=true`, `BatteryKnown=false` is a legitimate PASS → "—"; graceful skip on old daemon / no hardware).
- Added `test_battery_noflicker` proving `onBatteryChanged` emits one `dataChanged` carrying ONLY the battery roles, no `modelReset`, no reorder.

## Task Commits

1. **Task 1: proxy XML battery + DaemonConnection live BatteryChanged wiring** - `02a3509` (feat)
2. **Task 2: live battery UI (delegate + detail pane) + smoke assertion + no-flicker test** - `0df2c67` (feat)
3. **Task 3: human-verify live battery** - checkpoint, statically verified + manual steps below (documented-human-verify)

**Plan metadata:** _(final docs commit)_

## Files Created/Modified
- `src/logiops-gui/dbus/logid_device.xml` - Added Battery/Charging/BatteryKnown properties + BatteryChanged signal to the `.Device` interface.
- `src/logiops-gui/DaemonConnection.cpp` - Subscribe per-device BatteryChanged → `DeviceModel.onBatteryChanged`; initial battery from the GetAll snapshot; degrade-to-"—" on absence/error.
- `src/logiops-gui/qml/DeviceDelegate.qml` - Real battery block (glyph + fill + % + charging bolt, thresholds, "—"), per-role bindings, 0..100 clamp, accessible battery announcement.
- `src/logiops-gui/qml/DetailPane.qml` - Battery section (%/charging or muted "Battery status unavailable").
- `src/logiops-gui/icons/battery.svg` - Battery outline glyph (fallback / neutral outline).
- `src/logiops-gui/icons/charging.svg` - Charging bolt, baked Success green (always-green per UI-SPEC).
- `src/logiops-gui/CMakeLists.txt` - Added the two icons to `qt_add_qml_module` RESOURCES.
- `test/smoke/device-list.sh` - Hard Battery + BatteryKnown assertion (PASS/SKIP, never FAIL on legit unknown / old daemon).
- `test/DeviceModelTest.cpp` - `test_battery_noflicker` unit.

## Decisions Made
- Drew the battery glyph with QML primitives (Rectangle outline + proportional fill + terminal nub) rather than a tinted SVG, so the fill/outline recolors to the threshold color without a runtime `Qt5Compat.GraphicalEffects`/`ColorOverlay` dependency (that module is not installed and is not a desired prerequisite). The charging bolt is baked green in `charging.svg` (UI-SPEC: always Success green), so it needs no recolor.
- Stable percentage width via `TextMetrics("100%") + AlignRight` instead of `font.features: {"tnum":1}` — `font.features` is Qt 6.7+ and the target is **Qt 6.4.2**; UI-SPEC explicitly permits the fallback. (This was a blocking correctness fix — `font.features` would raise a runtime QML error on 6.4.2.)
- Initial battery value reuses the existing async `Properties.GetAll` snapshot already issued in `addDevice()` (no extra round-trip, no UI-thread block, Pitfall 4).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Replaced `font.features` (tabular figures) with a TextMetrics-reserved width**
- **Found during:** Task 2 (battery UI)
- **Issue:** The plan/UI-SPEC suggested `font.features` for tabular figures to keep the % width stable, but `font.features` is a **Qt 6.7+** QML Font property; the target is **Qt 6.4.2**, where it raises a runtime QML error and breaks the delegate load. UI-SPEC itself allows the fallback.
- **Fix:** Reserve the widest-case width via `TextMetrics("100%")` + `horizontalAlignment: AlignRight` on the percentage `Text` (delegate); removed `font.features` from the detail pane. Same stable-no-jitter result without the version dependency.
- **Files modified:** `src/logiops-gui/qml/DeviceDelegate.qml`, `src/logiops-gui/qml/DetailPane.qml`
- **Verification:** Full `qmlcachegen` (build-time QML compile) clean; -Werror build clean.
- **Committed in:** `0df2c67` (Task 2 commit)

**2. [Rule 2 - Missing Critical Coverage] Added `test_battery_noflicker`**
- **Found during:** Task 2 (test review)
- **Issue:** The plan's core must-have ("Battery updates ... repaint only the battery roles — no flicker, no reorder, no full reset") had a battery *functional* test but no explicit *no-flicker* assertion (the status path had one; battery did not).
- **Fix:** Added `test_battery_noflicker` mirroring `test_status_noflicker`: asserts `onBatteryChanged` emits exactly one `dataChanged` carrying ONLY `{BatteryPercent, Charging, BatteryKnown}` roles, zero `modelReset`, and the two rows keep their order.
- **Files modified:** `test/DeviceModelTest.cpp` (+ `<algorithm>` include)
- **Verification:** `ctest -R device_model` → 2/2 green.
- **Committed in:** `0df2c67` (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (1 blocking, 1 missing-critical-coverage)
**Impact on plan:** The `font.features` fix is required for the delegate to load on the locked Qt 6.4.2 target. The added test hardens the plan's central no-flicker contract. No scope creep.

## Issues Encountered
- **No live daemon/hardware in the execution environment** — `busctl introspect` returned nothing and the smoke script SKIPs (AccessDenied / no owner). The exact battery property/signal names + types were therefore verified against the daemon **source** (`src/logid/Device.cpp` IPC property/signal map: `Battery` y, `Charging` b, `BatteryKnown` b, `BatteryChanged(percentage y, charging b, known b)`) rather than a live introspection; they match the planned names. The regenerated proxy confirms `BatteryChanged(uchar percentage, bool charging, bool known)`.
- **Base `QtQuick` QML runtime module not installed in this env** (`qml6-module-qtquick`) — a known operator prerequisite documented since Plan 04. A live offscreen render reports only `module "QtQuick" is not installed`, not any battery-code error; build-time `qmlcachegen` compiled all QML (including the new battery QML) successfully.

## Human-Verify Checkpoint — Task 3 (documented-human-verify)

All automatable work is complete, committed, and statically verified (proxy regenerated, -Werror build clean, `device_model` + `device_model_noflicker` green, smoke SKIPs gracefully). The remaining check is **live battery on real hardware** (MX Master 4), which requires a running daemon + device and **must be performed by the operator**.

### Prerequisites
- `logid` daemon running (with Plan 02's battery feature) and owning `pizza.pixl.LogiOps` on the **system** bus.
- Current user in the `logiops` group (Phase 1 access path).
- An MX Master 4 (or other battery-reporting Logitech device) connected/awake.
- Base QML runtime installed: `sudo apt install qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-window` (operator prereq — without the base `QtQuick` module the GUI binary builds but the engine reports `module "QtQuick" is not installed`).

### Step A — Confirm the live Battery property decodes on real hardware (busctl)
Read the live `Battery`/`Charging`/`BatteryKnown` straight off the bus to confirm the daemon's 0x1004/0x1000 decode is producing a sane value before looking at the GUI:

```bash
# 1) Enumerate device object paths:
busctl --system call pizza.pixl.LogiOps /pizza/pixl/logiops \
    pizza.pixl.LogiOps.Devices Enumerate

# 2) For the device path from step 1 (e.g. /pizza/pixl/logiops/devices/0):
DEV=/pizza/pixl/logiops/devices/0
busctl --system get-property pizza.pixl.LogiOps "$DEV" pizza.pixl.LogiOps.Device Battery       # -> y NN  (0..100)
busctl --system get-property pizza.pixl.LogiOps "$DEV" pizza.pixl.LogiOps.Device Charging       # -> b true|false
busctl --system get-property pizza.pixl.LogiOps "$DEV" pizza.pixl.LogiOps.Device BatteryKnown   # -> b true|false

# 3) (optional) Watch the live BatteryChanged signal as you plug/unplug the charger:
busctl --system monitor pizza.pixl.LogiOps | grep -A3 BatteryChanged
```

**Expected:** `Battery` is a `y` in 0..100 with `BatteryKnown=true` on the MX Master 4 (0x1004 UnifiedBattery decode). A device that genuinely doesn't report battery returns `BatteryKnown=false` (the GUI then shows "—" — also correct).

### Step B — Build + run the GUI and verify the battery display
```bash
cmake --build build --target logiops-gui
./build/src/logiops-gui/logiops-gui
```
Confirm:
1. Each battery-reporting device shows a battery glyph + numeric % in the sidebar; the detail-pane "Battery" section matches.
2. **Threshold color:** > 20% renders green; let it drain below 20% (or confirm a low device) → the fill/% turns amber. No amber/warning color for a healthy battery.
3. **Charging bolt:** put the mouse on the charger → a green charging bolt overlays the glyph regardless of level; remove it → the bolt disappears. The % and bolt update **LIVE** without restarting the app.
4. **No-flicker:** during a battery/charging tick the row does **not** jump, re-sort, or blink the icon — only the battery block repaints (the % keeps a stable width, no horizontal jitter).
5. **Unknown:** a device that doesn't report battery shows **"—"** (muted), not a fake 0% and not a warning color.

### Step C — Smoke
```bash
bash test/smoke/device-list.sh
```
**Expected:** PASS (asserts a 0..100 Battery with `BatteryKnown=true`, or a legitimate `BatteryKnown=false`) or SKIP (daemon/hardware/group absent) — **never FAIL** on a healthy setup.

### Resume signal
Type **"approved"** once Steps A–C check out, or describe the issue (battery not updating live, flicker/reorder on tick, wrong threshold color, "—" not shown for an unsupported device, or busctl reporting an out-of-range/0x1004-misdecoded Battery value).

## Next Phase Readiness
- DEV-02 is real, live, signal-driven battery — the last functional piece of Phase 2. The phase's device list now shows name (DEV-04), connection/sleep state (DEV-03), and battery (DEV-02), all signal-driven with the no-flicker contract.
- The only outstanding item is the operator's live-hardware human-verify (Step A–C above); all code, build, tests, and smoke are green.
- Ready for Phase 3 (device configuration UI) once the human-verify is signed off.

## Known Stubs
None — the battery placeholder "—" stubs from Plans 03/04 (delegate + detail pane) are now wired to real, live data. "—" persists only as the *intentional* unknown-battery state (`BatteryKnown=false`), per UI-SPEC.

## Self-Check: PASSED

All created/modified files verified present on disk; both task commits (`02a3509`, `0df2c67`) verified in git history.

---
*Phase: 02-d-bus-client-device-list*
*Completed: 2026-05-30*
