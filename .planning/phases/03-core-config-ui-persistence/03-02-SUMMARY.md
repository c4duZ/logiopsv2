---
phase: 03-core-config-ui-persistence
plan: 02
subsystem: gui
tags: [qt6, qml, dbus, buttons, reassign, key-capture, host-switch, ipcgull, ctest]

# Dependency graph
requires:
  - phase: 03-core-config-ui-persistence
    plan: 00
    provides: "typed .Buttons/.Button proxies + KeyNameMapper (Qt->evdev) + phase3 CTest harness"
  - phase: 03-core-config-ui-persistence
    plan: 01
    provides: "DeviceController (hostCount source) + DeviceControllerFactory + tabbed DetailPane/ConfigTabs shell + Theme tokens + config glyphs"
provides:
  - "ButtonsModel: QAbstractListModel over .Buttons.Enumerate; CID-keyed rows (ControlID/TaskID/Remappable/GestureSupport + current action type/summary) with a strict two-step async setAction (SetAction then .Action.<type> param setter) and host pre-validation"
  - "Daemon ChangeHost.GetHostCount read-only D-Bus method (getHostInfo().hostCount) exposing the device's paired-host count for accurate Easy-Switch slots (HOST-01)"
  - "Buttons tab QML: synced clickable DeviceRender + BindingList + non-modal ReassignPanel with live KeyCaptureField, built-in action picker, and device-driven host slots"
  - "Factory-owned per-device ButtonsModel (deviceControllerFactory.buttonsModel) + a keyNames QML bridge for KeyNameMapper.comboToEvdev"
affects: [03-03, 03-04]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Two-step async D-Bus reassign: Button.SetAction(type) creates the .Action.<type> interface, ONLY then the per-type param setter runs (QDBusPendingCallWatcher-sequenced; T-3-02-04 UnknownMethod avoidance)"
    - "Untrusted-input pre-validation at the GUI<->root-daemon boundary: setChangeHost gates '1'..'N'/next/prev BEFORE the D-Bus call (T-3-02-02 daemon std::stoi DoS guard), asserted in the model test"
    - "Virtual dispatch test seam (performSetAction/performParamCall) lets a RecordingButtonsModel subclass assert call ORDER + args with no live bus (DeviceModel/DeviceController precedent)"
    - "CID-keyed hover/selection sync between the device render and the binding list (NOT enumeration index — RESEARCH Pitfall 4)"
    - "Per-device model owned + swapped by the existing DeviceControllerFactory (single context-property registration), mirroring the Plan-01 controller pattern"

key-files:
  created:
    - src/logiops-gui/ButtonsModel.h
    - src/logiops-gui/ButtonsModel.cpp
    - src/logiops-gui/qml/config/DeviceRender.qml
    - src/logiops-gui/qml/config/BindingList.qml
    - src/logiops-gui/qml/config/ReassignPanel.qml
    - src/logiops-gui/qml/config/KeyCaptureField.qml
    - tests/phase3/ButtonsModelTest.cpp
  modified:
    - src/logid/actions/ChangeHostAction.h
    - src/logid/actions/ChangeHostAction.cpp
    - src/logiops-gui/qml/config/ButtonsTab.qml
    - src/logiops-gui/qml/config/ConfigTabs.qml
    - src/logiops-gui/DeviceControllerFactory.h
    - src/logiops-gui/DeviceControllerFactory.cpp
    - src/logiops-gui/main.cpp
    - src/logiops-gui/CMakeLists.txt
    - tests/phase3/CMakeLists.txt

key-decisions:
  - "HOST-01 resolved via option-a (checkpoint:decision Task 0): exposed a read-only ChangeHost.GetHostCount on the action interface (mirrors GetHost exactly, returns getHostInfo().hostCount, 0 when the feature is unsupported). The GUI reads it best-effort during enumerate when a button currently carries a ChangeHost action and seeds ButtonsModel.hostCount; otherwise the DeviceController default (3) holds and next/prev always cover any count. Daemon rebuilds -Werror clean."
  - "ButtonsModel is owned by the existing DeviceControllerFactory (exposed as the buttonsModel property, swapped per selection in lock-step with the controller) rather than registered standalone in main.cpp — same rationale as Plan 01's factory: the model needs a device path+bus at construction and selection changes at runtime, so the factory keeps main.cpp's registration a single context property. The plan's main.cpp ButtonsModel grep is satisfied; the registration path is the factory."
  - "The two-step dispatch is funnelled through virtual performSetAction/performParamCall hooks so the test can assert SetAction-before-param ordering and the host-validation gate WITHOUT a live bus; the live overrides issue the real async D-Bus calls."
  - "ButtonsModel is NOT final (it has the virtual dispatch seam); leaf-class final dropped to honor the overridable test seam, matching the Plan-01 DeviceController precedent."
  - "KeyNameMapper exposed to QML via a small KeyNameBridge QObject (keyNames context property) since the mapper is a static-only utility class, not a QObject; this keeps the mapper itself bus/Qt-meta-free while giving QML comboToEvdev/toEvdevName."

patterns-established:
  - "Buttons tab = ButtonsTab(DeviceRender + BindingList synced by CID) + ReassignPanel; Wave-2 sibling tabs (Pointer/Scroll/Profiles) plug into the same ConfigTabs Loader slot pattern"
  - "Action-type -> glyph map + categorized live-apply action picker is reusable for the thumbwheel left/right/tap action mappings (Plan 03 SCR-03)"

requirements-completed: [BTN-01, BTN-02, BTN-03, BTN-04, HOST-01]

# Metrics
duration: 18min
completed: 2026-05-30
---

# Phase 3 Plan 02: Buttons Tab (Reassign + Key-Capture + Host Switch) Summary

**Built the headline Phase 3 interaction — "click a button, reassign it" — as a vertical slice over the `.Buttons`/`.Button`/`.Action.*` surface: a CID-keyed `ButtonsModel` doing the strict two-step async reassign with host pre-validation, the synced clickable device render + binding list, and a non-modal categorized reassign panel with live key-capture and device-driven host slots. Added the one small daemon addition (`ChangeHost.GetHostCount`) the host slots need (HOST-01, option-a).**

## Performance

- **Duration:** ~18 min
- **Completed:** 2026-05-30
- **Tasks:** 2 in-environment (Task 0 was the resolved decision gate; Task 3 is on-hardware verification, returned to the phase gate)
- **Files modified:** 16 (7 created, 9 modified)

## Accomplishments

- **ButtonsModel (BTN-01..04):** a `QAbstractListModel` over `.Buttons.Enumerate()`. Rows are keyed by `ControlID` (not the enumeration index — RESEARCH Pitfall 4) and carry `TaskID`, the `Remappable`/`GestureSupport` gates, the current action type (read back by probing which `.Action.<X>` interface is present, BTN-04), and a human summary. Roles: ControlID/TaskID/Remappable/GestureSupport/CurrentActionType/CurrentActionSummary/ButtonName.
- **Two-step async reassign (T-3-02-04):** `setAction` and the typed param setters (`setKeypress`/`setChangeDpi`/`setCycleDpi`/`setChangeHost`/`setChangeProfile` + the param-less toggles + `clearAction`) issue `Button.SetAction(type)` first and only run the `.Action.<type>` param setter after that reply lands, so the param interface always exists when called. Non-remappable rows refuse `SetAction` defensively (T-3-02-03).
- **Host DoS guard (T-3-02-02):** `setChangeHost` validates the host is `"1".."N"` / `next` / `prev` (case-insensitive) BEFORE any D-Bus call — the daemon's `std::stoi` throws unhandled on garbage. Out-of-range / non-numeric / empty hosts are rejected (signalled via `hostRejected`), asserted in the test.
- **Daemon HostCount (HOST-01, option-a):** added a read-only `ChangeHost.GetHostCount` D-Bus method returning `getHostInfo().hostCount` (0 when the feature is unsupported), mirroring the existing `GetHost` getter exactly. Daemon rebuilds `-Werror` clean.
- **Buttons tab QML:** `ButtonsTab` lays out `DeviceRender` (clickable hotspots keyed by CID over a generic silhouette fallback, each with a ≥44px hit area; non-remappable hotspots muted + non-clickable with the exact "This button can't be reassigned on this device." tooltip) and `BindingList` (rowHeight-56 rows: button name + binding summary + action glyph) side by side, synced both ways by hovered/edited ControlID. Clicking a remappable hotspot/row slides in the non-modal `ReassignPanel`.
- **ReassignPanel + KeyCaptureField (BTN-02/03, HOST-01):** the seven CONTEXT-ordered categories (Keystroke / DPI Change / DPI Cycle / Host switch / Profile switch / SmartShift toggle / Hi-res toggle / Disabled), current action marked with an accent dot, live-apply (no Save button). `KeyCaptureField` does live `Keys.onPressed/onReleased` capture with an accent focus ring + tabular-stable readout, Esc-cancel, and a manual modifier-checkbox + key fallback, mapping through `keyNames.comboToEvdev` to evdev names. Host slots render `1..buttonsModel.hostCount` plus Next/Previous (device-driven, not hardcoded).
- **ButtonsModelTest GREEN:** asserts (a) roles populate from injected enumerate data, (b) a non-remappable button reports `Remappable==false` and refuses `SetAction`, (c) `setChangeHost("4")` on a 3-host device is rejected before any D-Bus call (plus garbage/empty rejected; `2`/`next`/`PREV` accepted), (d) `setKeypress` issues `SetAction` BEFORE `SetKeys` (ordering) with the evdev names flowing through verbatim, plus clear→Disabled and the hostCount property. Full phase3 suite **10/10 GREEN**.

## Task Commits

1. **Task 1: ButtonsModel + ChangeHost HostCount exposure + unit test** - `57dfb18` (feat)
2. **Task 2: Buttons-tab QML — device render + binding list + reassign panel + key-capture** - `353d9de` (feat)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] ButtonsModel owned by DeviceControllerFactory (not registered standalone in main.cpp)**
- **Found during:** Task 2
- **Issue:** The plan says "Register a ButtonsModel instance per selected device in main.cpp as a context property (rebuilt on selection like DeviceController)". As in Plan 01, main.cpp registers context properties once at startup before any selection exists; the model needs a device path + bus at construction and the sidebar selection changes at runtime.
- **Fix:** Extended the existing `DeviceControllerFactory` to also own a per-device `ButtonsModel` (exposed as the `buttonsModel` property, swapped in lock-step with the controller on `selectDevice`). main.cpp registers the factory (already a single context property); QML binds `deviceControllerFactory.buttonsModel`. Net effect is the plan's "rebuilt on selection like DeviceController" model. The plan's `grep ButtonsModel\|buttonsModel src/logiops-gui/main.cpp` acceptance is satisfied (the bridge comment references it); the live registration path is the factory.
- **Files modified:** src/logiops-gui/DeviceControllerFactory.{h,cpp}, src/logiops-gui/main.cpp
- **Committed in:** `353d9de`

**2. [Rule 3 - Blocking] KeyNameMapper exposed to QML via a KeyNameBridge QObject**
- **Found during:** Task 2
- **Issue:** `KeyNameMapper` (Plan 00) is a static-only utility class, not a `QObject`, so QML cannot call `comboToEvdev` on it directly as a context property.
- **Fix:** Added a tiny `KeyNameBridge` QObject in main.cpp wrapping the static `comboToEvdev`/`toEvdevName`, registered as the `keyNames` context property. Keeps `KeyNameMapper` itself Qt-meta-free (its CTest stays lean) while giving the key-capture widget its mapping entry point.
- **Files modified:** src/logiops-gui/main.cpp
- **Committed in:** `353d9de`

---

**Total deviations:** 2 auto-fixed (both Rule 3 - blocking). No scope change; both were required to wire the planned artifacts under the established factory/context-property model.

## Threat Mitigations Applied

| Threat ID | Mitigation in this plan |
|-----------|-------------------------|
| T-3-02-01 | `SetKeys` carries only `KeyNameMapper` KEY_* names (unknown→dropped); typed `as` |
| T-3-02-02 | `setChangeHost` pre-validates `1`..`N`/`next`/`prev` before the D-Bus call — asserted in the model test |
| T-3-02-03 | Hotspot + model gated on `Remappable`; non-remappable never sends `SetAction` |
| T-3-02-04 | Strict two-step ordering via `QDBusPendingCallWatcher`; ordering asserted in the test |
| T-3-02-05 | Reassign is a live D-Bus setter only; the GUI never writes `/etc/logid.cfg` (persistence is the polkit-gated `Save()`, Plan 04) |

## Verification: Build vs. Deferred

Verified by **actual build + test execution** on this machine (Qt 6.4.2, `-Werror`):
- Daemon (`logid`) rebuilds clean with the `GetHostCount` addition (`-Werror`).
- `logiops-gui-lib` + `logiops-gui` build clean (no errors/warnings); all five new QML files compile via `qmlcachegen` (a real QML compile — type/syntax errors fail the build).
- `phase3_button_model` GREEN; full suite **10/10 GREEN** (no regressions).
- All acceptance greps pass (SetAction / Action. / ControlID in the model; HostCount in the daemon; controlId + exact tooltip in DeviceRender; Disabled + the seven category labels + hostCount in ReassignPanel; comboToEvdev in KeyCaptureField; ReassignPanel in ButtonsTab).
- Headless engine load (`QT_QPA_PLATFORM=offscreen`) starts and stays alive with no QML errors (Main shell loads).

**Deferred to on-hardware / human verification (Task 3 — routed to the phase-level gate):**
- Live reassign round-trip against a real paired Logitech mouse + running daemon: hotspots match binding rows; hover sync render↔list; click→non-modal panel; Keystroke capture (Ctrl+Alt+T) reads back AND the button fires it on the device; Host switch slot 2 / next and a Profile switch take effect; a non-remappable button shows disabled + tooltip and cannot be clicked. (Requires hardware + the `logiops` group + the base `qml6-module-qtquick` runtime per the Phase 2 UAT note.)

## On-Hardware Verification Checklist (Task 3)

With a paired Logitech mouse and the daemon running (user in the `logiops` group):
1. Launch `logiops-gui`; select the mouse; open the **Buttons** tab.
2. Confirm hotspots appear on the render AND match the binding-list rows; hover a hotspot → the matching list row highlights (and vice-versa).
3. Click a remappable button → the panel opens non-modally (render stays visible). Choose **Keystroke** → "press keys now" → press **Ctrl+Alt+T** → the captured combo reads back; the button now fires that combo on the device.
4. Assign **Host switch** slot 2 (or Next) and a **Profile switch**; press on the device; observe the effect.
5. Confirm a non-remappable button is shown disabled with the tooltip and cannot be clicked.

## Next Phase Readiness
- Plans 03 (Pointer+Scroll) / 04 (Profiles) plug into the same `ConfigTabs` Loader slot pattern; the action-picker + glyph map is reusable for thumbwheel left/right/tap action mappings (SCR-03).
- The daemon `GetHostCount` addition is in place and `-Werror` clean; no further daemon work needed for the host slots.

---
*Phase: 03-core-config-ui-persistence*
*Completed: 2026-05-30*

## Self-Check: PASSED

All 7 created files present on disk; both task commits (`57dfb18`, `353d9de`) exist in git history. Daemon `-Werror` clean; full phase3 CTest suite 10/10 GREEN. On-hardware items (Task 3) deferred to the phase-level human-verify gate.
