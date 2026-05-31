---
phase: 03-core-config-ui-persistence
plan: 04
subsystem: ui
tags: [qt6, qml, dbus, polkit, profiles, persistence, configstate, qdbuspendingcallwatcher]

# Dependency graph
requires:
  - phase: 03-core-config-ui-persistence (Plan 00)
    provides: ".Config.Save round-trip fidelity gate; phase3 test harness (no-bus CTest pattern)"
  - phase: 03-core-config-ui-persistence (Plan 01)
    provides: "DeviceController + DeviceControllerFactory (per-device swap), tabbed DetailPane shell, Theme tokens (destructive/dialogMaxWidth/motion)"
  - phase: 03-core-config-ui-persistence (Plan 02/03)
    provides: "Buttons/Pointer/Scroll tab setters that ConfigState.markDirty observes; ConfigTabs StackLayout"
provides:
  - "ProfilesModel: manual profile create/name/switch/remove over .Device.GetProfiles/SetProfile/RemoveProfile (PROF-01)"
  - "ConfigState: global unsaved-changes tracker + async polkit-gated .Config.Save driver + .Device.ClearProfile restore (CONF-01/CONF-02)"
  - "Profiles tab + SaveToolbar (unsaved pill, async Save CTA, transient Saved, mapped errors) + RestoreDialog QML"
  - "Two UI-SPEC Save error paths (polkit-denied vs daemon-down) mapped in C++ and surfaced inline"
affects: [phase-04-gestures, phase-05-per-app-profiles, persistence, save, profiles]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Optimistic dirty state held GUI-side between Saves (no per-config changed signal exists -> no polling)"
    - "Async polkit-gated Save via QDBusPendingCallWatcher with D-Bus-error -> UI-copy mapping (never blocks UI thread)"
    - "Per-device model swapped by DeviceControllerFactory in lock-step with the controller (ProfilesModel joins ButtonsModel)"
    - "Virtual daemon-write hooks (performSetProfile/performSave/performClearProfile) for no-bus unit testing with simulated replies"

key-files:
  created:
    - src/logiops-gui/ProfilesModel.h
    - src/logiops-gui/ProfilesModel.cpp
    - src/logiops-gui/ConfigState.h
    - src/logiops-gui/ConfigState.cpp
    - src/logiops-gui/qml/config/ProfilesTab.qml
    - src/logiops-gui/qml/config/SaveToolbar.qml
    - src/logiops-gui/qml/config/RestoreDialog.qml
    - tests/phase3/ProfilesModelTest.cpp
  modified:
    - src/logiops-gui/dbus/logid_device.xml
    - src/logiops-gui/DeviceControllerFactory.h
    - src/logiops-gui/DeviceControllerFactory.cpp
    - src/logiops-gui/main.cpp
    - src/logiops-gui/CMakeLists.txt
    - src/logiops-gui/qml/DetailPane.qml
    - src/logiops-gui/qml/config/ConfigTabs.qml
    - tests/phase3/CMakeLists.txt

key-decisions:
  - "Extended logid_device.xml (one PizzaPixlLogiOpsDeviceInterface) with the profile + ClearProfile surface instead of a second XML — a second XML for the same D-Bus interface name collides on the generated class name"
  - "ConfigState maps the D-Bus error to the final UI string in C++ (single source of truth); the SaveToolbar re-declares the two canonical strings for auditability and binds to configState.lastError"
  - "On a denied/failed Save, dirty STAYS true (T-3-04-03) so the user is never told a change persisted when it did not"
  - "renameProfile = SetProfile(new) + RemoveProfile(old) since the daemon exposes no rename primitive (verified Device.cpp:340-355)"

patterns-established:
  - "Persistence brain (ConfigState) is one global QObject; every tab setter calls markDirty(); a successful Save clears it + fires saved()"
  - "No-retry-storm Save: save() is a no-op while saving is true (T-3-04-02), guarding the daemon's server_lock held through the polkit prompt"

requirements-completed: [PROF-01, CONF-01, CONF-02]

# Metrics
duration: 38min
completed: 2026-05-30
---

# Phase 3 Plan 04: Profiles + Persistence Summary

**Manual-profile create/name/switch over .Device.SetProfile, a global unsaved-changes tracker driving an async polkit-gated .Config.Save with the two distinct UI-SPEC error paths, and a confirmation-gated Restore-defaults via .Device.ClearProfile — closing the configure-then-keep persistence loop.**

## Performance

- **Duration:** ~38 min
- **Started:** 2026-05-30
- **Completed:** 2026-05-30
- **Tasks:** 2 auto tasks executed + committed (Task 3 is the on-hardware human-verify gate)
- **Files modified:** 16 (8 created, 8 modified)

## Accomplishments
- **ProfilesModel** (QAbstractListModel over `.Device.GetProfiles`): `createProfile`/`switchProfile` → `SetProfile` (creates-if-absent + switches live), `removeProfile` → `RemoveProfile` (refuses the last profile), `renameProfile` = create-new+switch+drop-old; NameRole/IsActiveRole/IsDefaultRole; async refresh on every write (PROF-01).
- **ConfigState** (global QObject): `markDirty()` raises a persistent unsaved indicator; `save()` fires `.Config.Save` async via `QDBusPendingCallWatcher` (no UI freeze, no retry-storm); success → `dirty=false` + `saved()`; `AccessDenied`/"Not authorized" → auth-declined copy, `ServiceUnknown`/`NoReply` → daemon-down copy (dirty stays set on error); `restoreDefaults()` → `.Device.ClearProfile` (CONF-01/CONF-02).
- **QML**: ProfilesTab (live-switch pills + inline create/rename + exact empty-state copy), SaveToolbar mounted in the DetailPane header above the tabs (warning "Unsaved changes" pill, accent Save CTA with BusyIndicator, transient charging "Saved" tick, inline mapped error, Restore-defaults entry), RestoreDialog (modal confirm with the exact "can't be undone" copy + destructive confirm).
- **ProfilesModelTest** (`phase3_profiles`) GREEN: createProfile issues SetProfile, markDirty→dirty + successful save clears it, both error-string mappings, restoreDefaults issues ClearProfile — all driven through no-bus recording subclasses.
- Full phase3 suite 7/7 GREEN; full project suite 12/12 GREEN; GUI builds `-Werror` clean; offscreen QML load emits zero QML errors/warnings.

## Task Commits

1. **Task 1: ProfilesModel + ConfigState + unit test** - `1160a5b` (feat)
2. **Task 2: Profiles tab + SaveToolbar + RestoreDialog QML** - `f0bc1e0` (feat)

**Task 3:** `checkpoint:human-verify` (on-hardware) — see "On-Hardware Verification (Task 3)" below; not auto-executable.

## Files Created/Modified
- `src/logiops-gui/ProfilesModel.{h,cpp}` - manual profile model over `.Device` profile surface
- `src/logiops-gui/ConfigState.{h,cpp}` - dirty tracking + async Save driver + ClearProfile restore + error mapping
- `src/logiops-gui/dbus/logid_device.xml` - extended with GetProfiles/SetProfile/RemoveProfile/ClearProfile + ActiveProfile/DefaultProfile
- `src/logiops-gui/DeviceControllerFactory.{h,cpp}` - owns/swaps the per-device ProfilesModel; full-include of model headers for moc metatype completeness
- `src/logiops-gui/main.cpp` - registers `configState` global context property (bus-selected like the rest of the GUI)
- `src/logiops-gui/qml/config/ProfilesTab.qml` - profile pills, create/rename/remove (PROF-01)
- `src/logiops-gui/qml/config/SaveToolbar.qml` - unsaved pill + async Save + Saved tick + error + restore entry (CONF-01/02)
- `src/logiops-gui/qml/config/RestoreDialog.qml` - confirmation dialog (CONF-02)
- `src/logiops-gui/qml/DetailPane.qml` - mounts SaveToolbar above the tabs; wires `configState.setDevicePath` on selection
- `src/logiops-gui/qml/config/ConfigTabs.qml` - loads the real ProfilesTab (replaces the Plan 01 placeholder)
- `tests/phase3/ProfilesModelTest.cpp` + `tests/phase3/CMakeLists.txt` - `phase3_profiles` unit

## Decisions Made
- Extended the single `PizzaPixlLogiOpsDeviceInterface` (via logid_device.xml) with the profile + restore surface rather than minting a second proxy — a second XML for the same D-Bus interface name `pizza.pixl.LogiOps.Device` generates a colliding class name. ProfilesModel + ConfigState reuse the existing `logid_device_proxy`.
- ConfigState owns the D-Bus-error → UI-string mapping (single source of truth); the SaveToolbar binds `configState.lastError` and re-declares the two canonical strings for in-QML auditability.
- On Save failure, `dirty` is intentionally left set (T-3-04-03 repudiation mitigation) so the UI never implies a change persisted when it did not.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Single device proxy instead of a separate profiles XML**
- **Found during:** Task 1 (build)
- **Issue:** A dedicated `logid_device_profiles.xml` generated a class named `PizzaPixlLogiOpsDeviceInterface` (derived from the D-Bus interface name, not the file basename), colliding with the existing device proxy of the same name → "expected type-specifier" / redefinition.
- **Fix:** Removed the separate XML; added the profile methods/properties to `logid_device.xml` so one proxy class carries the whole `.Device` surface. ProfilesModel + ConfigState include `logid_device_proxy.h`.
- **Files modified:** dbus/logid_device.xml, ProfilesModel.cpp, ConfigState.cpp, CMakeLists.txt
- **Verification:** GUI builds `-Werror` clean.
- **Committed in:** 1160a5b (Task 1 commit)

**2. [Rule 3 - Blocking] Full-include model headers in DeviceControllerFactory.h**
- **Found during:** Task 1 (build)
- **Issue:** `Q_PROPERTY(ProfilesModel*)` made moc instantiate `Q_DECLARE_METATYPE(ProfilesModel*)`, which requires the complete type; the forward declaration sufficed for the pointer member but not the metatype (the aggregated mocs TU compiles moc_*.cpp alphabetically, so ProfilesModel was still incomplete at the factory's moc).
- **Fix:** Included ButtonsModel.h / DeviceController.h / ProfilesModel.h in DeviceControllerFactory.h so the metatypes are complete.
- **Files modified:** DeviceControllerFactory.h
- **Verification:** mocs_compilation.cpp compiles; GUI builds clean.
- **Committed in:** 1160a5b (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (both Rule 3 blocking, build-time). 
**Impact on plan:** Both were necessary to compile; no behavior change vs the plan's intent (same interfaces, same methods). No scope creep.

## Issues Encountered
None beyond the two blocking build issues above (both resolved inline).

## Known Stubs
None. ProfilesModel + ConfigState are wired to live D-Bus on the daemon's real `.Device`/`.Config` interfaces; QML binds to the C++ properties/models. The only non-live path is the deliberate no-bus test seam (recording subclasses) and the headless `performSave()` guard (returns a deterministic daemon-down error when `_live` is false), which never runs in the GUI.

## Threat Flags
None new. The plan's `<threat_model>` surface (Save bypass, Save-blocks-daemon DoS, denied-Save repudiation, wrong-profile ClearProfile, unknown-key drop) is fully covered: the GUI only calls the daemon's polkit-gated `Save()` (never touches `/etc/logid.cfg`); Save is async + no-retry-storm + button-disabled-while-saving; a denied Save keeps `dirty` set with explicit copy; ClearProfile is confirmation-gated on the active profile only.

## On-Hardware Verification (Task 3 — human-verify gate)

In-environment verification is complete (build `-Werror` clean, full CTest 12/12 GREEN including `phase3_profiles`, offscreen QML loads with zero errors). The remaining checks require a paired Logitech mouse + running daemon + `logiops`-group membership and are returned for the phase gate:

1. Create a profile "Test" and switch to it — confirm it applies live.
2. Change a setting (e.g. DPI) — confirm the "Unsaved changes" pill appears.
3. Click Save — confirm a polkit prompt appears with NO UI freeze; approve it; confirm the pill clears to a brief "Saved" tick.
4. Persistence: change + do NOT save + restart daemon → reverts; redo + Save (approve) + restart → persists.
5. Trigger Save and DENY the polkit prompt — confirm the auth-declined copy shows and the change is still applied live.
6. Restore defaults: open the dialog, confirm — verify the device config section resets.

## Next Phase Readiness
- Phase 3 config + persistence loop is functionally complete pending the on-hardware gate. Phase 4 (gestures) can build on the same ConfigState dirty/Save model — any new gesture setter calls `configState.markDirty()`.
- No blockers introduced.

---
*Phase: 03-core-config-ui-persistence*
*Completed: 2026-05-30*

## Self-Check: PASSED

All created files present (ProfilesModel/ConfigState .cpp, three QML files, ProfilesModelTest, SUMMARY) and both task commits (1160a5b, f0bc1e0) exist in history.
