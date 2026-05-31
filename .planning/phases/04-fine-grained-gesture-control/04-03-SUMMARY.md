---
phase: 04-fine-grained-gesture-control
plan: 03
subsystem: logiops-gui
tags: [gesture, gui, qml, builder, preview, gest-01, gest-02, gest-03, gest-04]
requires:
  - "GestureModel button-scoped brain (Plan 02): previewSentence/granularityReadout/activeDirection + setMode/setGranularity/setGestureAction/plainModes/isConfigured/modeOf"
  - "ReassignPanel CategoryRow + expanded state machine (Phase 3)"
  - "ConfigState markDirty + polkit Save seam (Phase 1/3)"
  - "Theme.qml token set (Phase 2/3)"
provides:
  - "GestureBuilder.qml: direction cross + mode pills + action + granularity slider + live preview card (GEST-01..04)"
  - "Gesture category in ReassignPanel, capability-gated on GestureSupportRole"
  - "DeviceController.gestureModelForButton(path): button-scoped GestureModel ownership exposed to QML"
  - "Theme.gestureDirCell (44) + Theme.gestureConfigured (alias -> accent)"
  - "ButtonsModel.ButtonPathRole (+8) + 'Gesture' action readback"
affects:
  - "src/logiops-gui (new QML view + Theme tokens + glyphs; ButtonsModel/DeviceController extended, no behavior change to existing tabs)"
tech-stack:
  added: []
  patterns:
    - "QML renders only: all gesture copy/logic from the C++ GestureModel (zero JS business logic)"
    - "Controller-owned, per-button re-pointed model handed to QML via Q_INVOKABLE"
    - "Reactive bindings to NOTIFYable model properties (Phase 3 lesson: never bind a bare stale method)"
key-files:
  created:
    - src/logiops-gui/qml/config/GestureBuilder.qml
    - src/logiops-gui/icons/gesture.svg
    - src/logiops-gui/icons/arrow.svg
  modified:
    - src/logiops-gui/qml/Theme.qml
    - src/logiops-gui/qml/config/ReassignPanel.qml
    - src/logiops-gui/DeviceController.h
    - src/logiops-gui/DeviceController.cpp
    - src/logiops-gui/ButtonsModel.h
    - src/logiops-gui/ButtonsModel.cpp
    - src/logiops-gui/CMakeLists.txt
decisions:
  - "Gesture action type string is 'Gesture' (daemon GestureAction::interface_name), NOT 'Gestures' as the plan draft suggested — used for SetAction, the active check, and the readback list"
  - "GestureModel ownership lives on DeviceController (gestureModelForButton) per the plan's key_link; the button path reaches QML via a new ButtonPathRole on ButtonsModel"
  - "The gesture per-direction action sub-section reuses the existing Keystroke chooser via chooseActionRequested -> expanding the Keypress category (no parallel picker built); deeper action-type coverage is a follow-up"
metrics:
  duration: "~12m"
  tasks: 3
  files: 10
  completed: 2026-05-31
---

# Phase 4 Plan 03: Gesture Builder QML + ReassignPanel Integration Summary

Rendered the guided gesture builder per the approved UI-SPEC: a capability-gated
"Gesture" category inside the existing `ReassignPanel.qml` expands inline into a
4-cardinal direction cross -> plain-language mode pills -> action sub-section ->
granularity slider with a human readout -> a first-class live preview card, all
bound to the Plan-02 `GestureModel` (zero business logic in QML). Two new
`Theme.qml` tokens (`gestureDirCell`, `gestureConfigured`) and two glyphs
(`gesture`, `arrow`) were added; everything else reuses existing `Theme.*`. The
GUI links clean and the offscreen QML smoke test reports no type-resolution
errors. The on-hardware UAT (Task 3) is a blocking human-verify checkpoint and was
NOT executed by the agent.

## What Was Built

- **`Theme.qml`** — `gestureDirCell: 44` (layout) and `gestureConfigured: accent`
  (semantic alias, zero new hues), placed beside the existing tokens.
- **`icons/gesture.svg` + `icons/arrow.svg`** — single-path monochrome glyphs in
  the existing `currentColor`-tintable style (match `host.svg`). The arrow is
  rotated 0/90/180/270 in QML for the four directions.
- **`DeviceController.gestureModelForButton(path)`** — lazily constructs, caches,
  and re-points a button-scoped `GestureModel` for the `.../buttons/M` node the
  panel is editing; injects the same `ConfigState` so gesture edits dirty the
  inherited "Unsaved changes" pill (no second Save path). Owned as a QObject child.
- **`ButtonsModel`** — `ButtonPathRole` (UserRole+8) exposes the button object
  path to QML; `"Gesture"` added to the action readback list so a gesture-bound
  button reads back as `currentType === "Gesture"` (drives the category's `active`).
- **`GestureBuilder.qml`** (442 lines) — the full UI-SPEC contract:
  - 3x3 direction cross; only the 4 cardinal cells interactive at 44px; center
    mouse glyph; configured dot (`gestureConfigured`), hover (`hoverTint`),
    active (`accentTint` + 2px accent ring + accent arrow), keyboard traversal.
  - four LOCKED plain-language mode pills from `gestureModel.plainModes()`,
    single-select, live-apply via `setMode(direction, label)`.
  - progressive disclosure: Action+Granularity for the two discrete modes,
    Granularity only for "Adjust proportionally", neither for "Nothing".
  - granularity Slider reusing the Pointer-tab visual, fires on release, with the
    tabular-stable human readout bound to `gestureModel.granularityReadout`.
  - first-class preview card bound to `gestureModel.previewSentence` (GEST-04),
    opacity cross-fade on change, `Accessible.name`.
  - "Clear" `ToolButton` (the only destructive-colored element) -> `setMode(dir, "Nothing")`.
- **`ReassignPanel.qml`** — the Gesture `CategoryRow` after "Hi-res toggle",
  before "Disabled", `visible` only on `roleData(4)` (GestureSupportRole); on
  chosen it issues `setAction(row, "Gesture")` FIRST (Pitfall 3 ordering), builds
  the button-scoped model, and reveals the inline `GestureBuilder` with the
  `motionAdd` 220ms opacity reveal.

## Verification

- `cmake --build build --target logiops-gui` after clearing the stale `.rcc`/
  qmlcache — clean, links.
- Offscreen QML smoke test (`QT_QPA_PLATFORM=offscreen`) — **QML OK**, no
  "not a type" / "failed to load" / "module not installed".
- Full `cmake --build build` — no `error:` (daemon CI is -Werror).
- `ctest --test-dir build -R phase4 --output-on-failure` — **2/2 PASS**
  (`phase4_gesture_math`, `phase4_gesture_model`).
- On-hardware UAT (live firing, GEST-02 one-flick-one-step feel, GEST-03 repeat,
  polkit-Save persistence) — **DEFERRED to the human-verify checkpoint** (Task 3).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Gesture action type is "Gesture", not "Gestures"**
- **Found during:** Task 1 / Task 3 wiring
- **Issue:** The plan draft referenced `setAction("Gestures")` and
  `currentType === "Gestures"`, but the daemon's `GestureAction::interface_name`
  is `"Gesture"` (singular, `src/logid/actions/GestureAction.cpp:28`). The plural
  string would never match SetAction or the readback, so the category would never
  light up and SetAction would not build the GestureAction.
- **Fix:** Used `"Gesture"` for SetAction, the `active` check, and added `"Gesture"`
  to the `ButtonsModel` action readback list.
- **Files modified:** `ReassignPanel.qml`, `ButtonsModel.cpp`
- **Commits:** 9409f76, 74bec4e

**2. [Rule 3 - Blocking] No button-path role for button-scoped model**
- **Found during:** Task 1
- **Issue:** `GestureModel` is button-scoped and needs the `.../buttons/M` object
  path, but `ButtonsModel` did not expose `ButtonInfo.path` as a role, so the
  panel could not tell the controller which button to scope the model to.
- **Fix:** Added `ButtonPathRole` (UserRole+8) + `roleNames` entry; the panel
  reads `roleData(8)` and passes it to `controller.gestureModelForButton(path)`.
- **Files modified:** `ButtonsModel.h`, `ButtonsModel.cpp`
- **Commit:** 9409f76

### Scope note

The per-direction Action sub-section reuses the existing Keystroke chooser via the
`chooseActionRequested` signal (expands the Keypress category), satisfying "do not
build a parallel picker". Wiring the full categorized action set
(DPI/Host/Profile/SmartShift/Hi-res) through `gestureModel.setGestureAction` for
each direction is a deliberate follow-up — the preview/model already accept any
action type string, so it is an additive QML change with no model rework.

## Known Stubs

None that block the plan goal. The action sub-section currently routes only the
Keystroke chooser to a direction (see scope note); the preview sentence, mode,
granularity, and direction cross are fully wired to live model state.

## Self-Check: PASSED

- FOUND: src/logiops-gui/qml/config/GestureBuilder.qml
- FOUND: src/logiops-gui/icons/gesture.svg
- FOUND: src/logiops-gui/icons/arrow.svg
- FOUND: .planning/phases/04-fine-grained-gesture-control/04-03-SUMMARY.md
- FOUND commit 9409f76 (Theme tokens + glyphs + GestureModel ownership)
- FOUND commit 95753f1 (GestureBuilder.qml)
- FOUND commit 74bec4e (ReassignPanel Gesture category integration)
