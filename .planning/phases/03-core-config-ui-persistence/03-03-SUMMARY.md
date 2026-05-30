---
phase: 03-core-config-ui-persistence
plan: 03
subsystem: gui
tags: [qt6, qml, dbus, dpi, scroll, smartshift, hires, thumbwheel, schema, ipcgull, ctest, capability-gating]

# Dependency graph
requires:
  - phase: 03-core-config-ui-persistence
    plan: 00
    provides: "typed .DPI/.SmartShift/.HiresScroll/.ThumbWheel proxies + phase3 CTest harness + the save() round-trip fidelity test"
  - phase: 03-core-config-ui-persistence
    plan: 01
    provides: "DeviceController (capability flags + live values + optimistic setters) + DeviceControllerFactory + tabbed ConfigTabs shell + Theme tokens + config glyphs"
  - phase: 03-core-config-ui-persistence
    plan: 02
    provides: "categorized action-picker row/glyph pattern (ReassignPanel) reused for the thumbwheel tap mapping"
provides:
  - "Device/profile-scoped DPI-cycle preset list in the daemon schema (config::DpiPreset {value, optional label} + Profile.dpi_presets), the home for the standalone Pointer-tab cycle editor (option-a)"
  - "Daemon .DPI GetPresets/SetPresets IPC (two parallel au/as arrays) reading/writing that schema list so cycle VALUES and LABELS persist device-scoped via Save()"
  - "DeviceController DPI-cycle model (dpiPresets {value,label} list + addPreset/removePreset/setPresetLabel/setPresetValue pushing the full list to .DPI.SetPresets, with [dpiMin,dpiMax] clamp) + setThumbTap simple-action passthrough"
  - "Pointer tab: device-bounded DPI slider (onMoved, no per-pixel storm, no GUI rounding) + tabular-stable readout + labeled add/remove DpiCycleEditor"
  - "Scroll tab: capability-gated SmartShift (threshold + torque, torque gated) / hi-res (toggle + invert) / thumbwheel (divert/invert + tap action) sections"
  - "EXTENDED phase3_save_roundtrip proving preset VALUES + LABELS survive a load -> mutate (relabel + append) -> Save -> re-parse cycle; new phase3_dpi_cycle asserting the .DPI.SetPresets payload path"
affects: [03-04]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Device/profile-scoped preset list as a schema group (config::DpiPreset) registered in Profile's field list; serialized by the same config::set/_save machinery as every other setting (Save round-trips it)"
    - "Array-of-struct avoided on the wire: presets exposed as two PARALLEL arrays (au values / as labels, values[i]<->labels[i]) so the D-Bus signature stays plain arrays like GetDPIs' proven ai out-arg (au out-arg needs the QtTypeName.Out0=QList<uint> annotation)"
    - "Preset persistence path is assert-able with no live bus: the controller's pushPresets() is virtual so a RecordingController subclass captures the exact .DPI.SetPresets {values,labels} payload (the actual node, not a hand-wave)"
    - "DPI slider fires the setter on onMoved (move-end) only, never per-pixel (T-3-03-02); the controller re-reads GetDPI after SetDPI to reflect the daemon's snapped value (no GUI-side rounding)"
    - "Scroll sections each gated with `visible: controller.hasX` so an absent feature's whole section is hidden, mirroring the daemon's UnsupportedFeature (T-3-03-04)"

key-files:
  created:
    - src/logiops-gui/qml/config/DpiCycleEditor.qml
    - tests/phase3/DpiCycleTest.cpp
  modified:
    - src/logid/config/schema.h
    - src/logid/features/DPI.h
    - src/logid/features/DPI.cpp
    - src/logiops-gui/dbus/logid_dpi.xml
    - src/logiops-gui/DeviceController.h
    - src/logiops-gui/DeviceController.cpp
    - src/logiops-gui/qml/config/PointerTab.qml
    - src/logiops-gui/qml/config/ScrollTab.qml
    - src/logiops-gui/qml/config/ConfigTabs.qml
    - src/logiops-gui/CMakeLists.txt
    - tests/phase3/CMakeLists.txt
    - tests/phase3/save_roundtrip_test.cpp

key-decisions:
  - "Task 0 resolved option-a (device/profile-scoped preset list): the Pointer-tab cycle editor has NO button context, so cycle VALUES and per-preset LABELS persist device-scoped in the schema (config::DpiPreset on Profile.dpi_presets) + a .DPI GetPresets/SetPresets pair, NOT silently bound to the first CycleDPI button. Both value and label survive Save (proven by the extended round-trip test)."
  - "Presets are marshalled as two parallel arrays (au values + as labels) instead of an array-of-struct a(us), to keep the wire signature within the proven plain-array pattern (GetDPIs' ai) and avoid relying on unverified ipcgull vector-of-tuple struct marshalling. values[i] pairs with labels[i]; empty label string => no label key in the schema."
  - "The DPI feature already held a reference to profile.dpi; it now also holds a std::reference_wrapper to profile.dpi_presets, re-pointed in setProfile(), so SetPresets writes the exact node Save() serializes (parallels the existing _config reference pattern)."
  - "Thumbwheel TAP uses a simple-action picker (SmartShift toggle / Hi-res toggle / Disabled) via DeviceController.setThumbTap -> .ThumbWheel.SetTap. VERIFIED the daemon builds a simple action at .../thumb_wheel/tap, so NO daemon change for tap. left/right are gesture-typed and deferred to Phase 4 per CONTEXT."
  - "setThumbTap added to DeviceController (Rule 3): QML must not issue raw D-Bus calls (the 'QML renders only' rule), so the thumbwheel proxy passthrough lives in the controller."

patterns-established:
  - "Device-scoped config additions: add a schema group + register it in the parent group's field list + a Get/Set IPC pair on the owning feature + extend the Plan-00 round-trip test to prove Save persists it"
  - "Pointer/Scroll tabs plug into the same ConfigTabs Loader slot pattern as Buttons; the categorized action-row structure is reused for the thumbwheel tap mapping"

requirements-completed: [DPI-01, DPI-02, DPI-03, SCR-01, SCR-02, SCR-03]

# Metrics
duration: 20min
completed: 2026-05-30
---

# Phase 3 Plan 03: Pointer & Scroll Tabs + Device-Scoped DPI Presets Summary

**Built the Pointer tab (device-bounded DPI slider + labeled add/remove cycle editor) and the capability-gated Scroll tab (SmartShift/hi-res/thumbwheel), and resolved the DPI-cycle persistence gap (Task 0 option-a) by adding a device/profile-scoped `{value,label}` preset list to the daemon schema with a `.DPI GetPresets/SetPresets` IPC pair — proven to round-trip both values AND labels through `Save()`.**

## Performance

- **Duration:** ~20 min
- **Completed:** 2026-05-30
- **Tasks:** 3 in-environment (Task 0 was the resolved decision gate; Task 4 is on-hardware verification, returned to the phase gate)
- **Files modified:** 14 (2 created, 12 modified)

## Accomplishments

- **Task 0 resolved → option-a (device-scoped):** the standalone Pointer-tab cycle editor has no button context, so the cycle VALUES and per-preset LABELS now live device-scoped in the schema rather than being silently bound to one button's `CycleDPI` action.
- **Daemon schema + IPC (DPI-02/03):** added `config::DpiPreset` (`{int value, std::optional<std::string> label}`) and `Profile.dpi_presets` (registered in the Profile group field list, serialized by the same `config::set`/`_save` machinery as everything else). Added `.DPI GetPresets`/`SetPresets` (two parallel `au`/`as` arrays) that read/write that list via a new `std::reference_wrapper` to `profile.dpi_presets` re-pointed in `setProfile()`. **Daemon rebuilds `-Werror` clean.**
- **EXTENDED `phase3_save_roundtrip` (the load-bearing proof):** the fixture now ships three labeled presets (`800/Low`, `1600/Medium`, `3200/High`); the test asserts they load with value+label, then mutates the in-memory list exactly like the GUI's `SetPresets` payload would (relabel `1600 Medium → Office` AND append `6400/Gaming`), serializes via the exact `config::set + writeFile` path `Save()` runs, re-parses, and asserts the 4-entry list survives with BOTH values and labels intact and in order.
- **DeviceController DPI-cycle model:** `dpiPresets` (`{value,label}` map list) + `addPreset/removePreset/setPresetLabel/setPresetValue`, each pushing the FULL list to `.DPI.SetPresets`; values clamp to `[dpiMin,dpiMax]` before emit (T-3-03-01, daemon also snaps). `setDpi` now re-reads `GetDPI` after `SetDPI` to reflect the snapped value (no GUI-side rounding). `setThumbTap` passthrough to `.ThumbWheel.SetTap`.
- **`phase3_dpi_cycle` (new unit):** a `RecordingController` overrides the virtual `pushPresets()` to capture the EXACT `.DPI.SetPresets {values,labels}` payload with no live bus, and asserts add/remove/relabel/setValue update the model AND push the full parallel-array list, plus `[dpiMin,dpiMax]` clamp on both `addPreset` and `setPresetValue`, plus the `GetPresets` seed path. (No assertion stops at a generic "round-trip through CycleDPI.SetDPIs" — it names `.DPI.SetPresets`.)
- **Pointer tab (DPI-01..03):** a `Slider` bound to `controller.dpiMin/dpiMax/dpiStep/dpi` with `onMoved` (move-end, not per-pixel — T-3-03-02) calling `setDpi`; hairline unfilled track, accent fill, accent thumb with a focus ring; a tabular-stable `TextMetrics` readout showing the value + `" DPI"`. Below it the `DpiCycleEditor`: `{value,label}` rows (`SpinBox` bounded by the device + label `TextField`), a destructive per-row remove, a `"+ Add preset"` secondary button, `accentTint` active chip (matching the live DPI), and the exact empty copy *"No DPI presets yet. Add one to cycle through sensitivities."*
- **Scroll tab (SCR-01..03):** three sections each `visible`-gated on the introspected capability (absent → whole section hidden, mirroring `UnsupportedFeature`): SmartShift on/off `Switch` + threshold + torque sliders (greyed while OFF; torque gated on `hasTorque`); hi-res toggle + invert `CheckBox`; thumbwheel divert/invert `Switch`es + a TAP simple-action picker (SmartShift toggle / Hi-res toggle / Disabled) reusing the categorized action-row structure.
- **Builds + tests:** daemon `-Werror` clean; `logiops-gui` builds clean (all new QML compiled by `qmlcachegen`); full phase3 CTest suite **6/6 GREEN** including the extended round-trip and the new DPI-cycle unit.

## Task Commits

1. **Task 1: device-scoped DPI presets (schema + .DPI IPC + controller + extended round-trip + DpiCycleTest)** - `9760849` (feat)
2. **Task 2: Pointer tab — DPI slider + labeled cycle editor (DPI-01..03)** - `0d7bced` (feat)
3. **Task 3: Scroll tab — capability-gated SmartShift/hi-res/thumbwheel (SCR-01..03)** - `8867f12` (feat)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Presets marshalled as two parallel arrays, not an array-of-struct**
- **Found during:** Task 1
- **Issue:** The plan implies `{value,label}` pairs; the natural D-Bus shape is `a(us)` (array of struct). ipcgull's marshalling of a `std::vector<std::tuple<...>>` (vector whose element is a tuple/struct) is not exercised anywhere in the codebase, so relying on it was a build risk.
- **Fix:** Exposed `GetPresets`/`SetPresets` as two PARALLEL arrays — `values` (`au` / `QList<uint>`) and `labels` (`as` / `QStringList`), with `values[i]` paired to `labels[i]`. This stays within the proven plain-array pattern (`GetDPIs`' `ai` out-arg) and needs only the `QtTypeName.Out0/In0 = QList<uint>` annotation. The `{value,label}` model is reconstructed on both ends.
- **Files modified:** src/logid/features/DPI.{h,cpp}, src/logiops-gui/dbus/logid_dpi.xml, src/logiops-gui/DeviceController.{h,cpp}, tests/phase3/DpiCycleTest.cpp
- **Committed in:** `9760849`

**2. [Rule 3 - Blocking] `setThumbTap` added to DeviceController for the thumbwheel tap mapping**
- **Found during:** Task 3
- **Issue:** The Scroll-tab thumbwheel tap must call `.ThumbWheel.SetTap(type)`, but QML must not issue raw D-Bus calls (the Phase 2 "QML renders only" rule) and the DeviceController had no tap entry point.
- **Fix:** Added `Q_INVOKABLE setThumbTap(const QString& type)` to DeviceController, calling the owned `_thumbProxy->SetTap(type)`. No daemon change (VERIFIED `SetTap` builds a simple action at `.../thumb_wheel/tap`).
- **Files modified:** src/logiops-gui/DeviceController.{h,cpp}
- **Committed in:** `8867f12`

---

**Total deviations:** 2 auto-fixed (both Rule 3 - blocking). No scope change; both were required to land the planned artifacts faithfully.

## Threat Mitigations Applied

| Threat ID | Mitigation in this plan |
|-----------|-------------------------|
| T-3-03-01 | Slider bounded by `dpiMin/dpiMax`; preset values clamp to `[dpiMin,dpiMax]` before emit (asserted in DpiCycleTest); daemon also snaps via `getClosestDPI` |
| T-3-03-02 | DPI setter fires on `onMoved` (move-end) only, not per-pixel — asserted by the QML code shape |
| T-3-03-03 | Plan-00 round-trip test EXTENDED to assert preset VALUES and LABELS survive Save (relabel + append); GREEN |
| T-3-03-04 | Every Scroll section gated `visible: controller.hasX`; absent → hidden, no setter ever issued on an absent interface |

## Verification: Build vs. Deferred

Verified by **actual build + test execution** on this machine (Qt 6.4.2, `-Werror`):
- Daemon (`logid`) rebuilds clean with the schema `dpi_presets` field + `.DPI GetPresets/SetPresets` IPC (`-Werror`).
- `logiops-gui-lib` + `logiops-gui` build clean; all new/changed QML compiled by `qmlcachegen` (type/syntax errors fail the build).
- `phase3_dpi_cycle` GREEN (asserts the `.DPI.SetPresets` payload path + clamp); EXTENDED `phase3_save_roundtrip` GREEN (preset values+labels survive Save); full suite **6/6 GREEN** (no regressions).
- All acceptance greps pass (schema `preset`/`label`; `addPreset`/`dpiPresets` in the controller header; `SetDPI` + no GUI rounding in the controller; `dpiMin`/`onMoved` in PointerTab; `No DPI presets yet` + `destructive` in DpiCycleEditor; `hasSmartShift`/`hasThumbwheel`/`hasTorque`/`SetTap` in ScrollTab).

**Deferred to on-hardware / human verification (Task 4 — routed to the phase-level gate):** see checklist below.

## On-Hardware Verification Checklist (Task 4)

With a paired Logitech mouse and the daemon running (user in the `logiops` group):
1. **Pointer tab:** drag the DPI slider to both extremes → it clamps to the device min/max and the readout shows the SNAPPED value; pointer speed changes live.
2. **DPI presets:** add three presets with labels (e.g. Low / Medium / High); confirm they persist in the editor; **Save and restart the daemon** → confirm the preset VALUES and LABELS survive (device-scoped, option-a). If a button is set to Cycle DPI, cycling steps through them.
3. **Scroll tab:** confirm only the sections the device supports are shown. Toggle SmartShift and adjust threshold/torque (torque slider only if the device reports TorqueSupport); toggle hi-res and invert; set thumbwheel divert/invert and a tap action (SmartShift toggle / Hi-res toggle). Verify each affects scrolling on the device.

## Next Phase Readiness
- Plan 04 (Profiles + Save flow) plugs into the same `ConfigTabs` Loader slot pattern; the device-scoped `dpi_presets` schema field is part of the Profile, so per-profile preset lists come for free when Profiles land.
- The `.DPI GetPresets/SetPresets` surface and the extended round-trip proof are in place; persistence of the new field is de-risked (the Plan-00 fidelity gate now covers it).

---
*Phase: 03-core-config-ui-persistence*
*Completed: 2026-05-30*

## Self-Check: PASSED

Both created files (`DpiCycleEditor.qml`, `DpiCycleTest.cpp`) present on disk; all three task commits (`9760849`, `0d7bced`, `8867f12`) exist in git history. Daemon `-Werror` clean; full phase3 CTest suite 6/6 GREEN. Artifact `min_lines` satisfied (PointerTab 131, ScrollTab 285, DpiCycleEditor 136). On-hardware items (Task 4) deferred to the phase-level human-verify gate.
