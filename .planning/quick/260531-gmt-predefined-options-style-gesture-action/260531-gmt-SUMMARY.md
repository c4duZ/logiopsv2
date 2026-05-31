---
quick_id: 260531-gmt
description: Predefined Options+-style gesture action picker (curated keystroke list replacing raw key capture; custom keystroke kept as advanced fallback)
mode: quick
date: 2026-05-31
status: complete
key-files:
  created:
    - src/logiops-gui/GestureActions.h
    - src/logiops-gui/GestureActions.cpp
    - src/logiops-gui/qml/config/GesturePicker.qml
  modified:
    - src/logiops-gui/CMakeLists.txt
    - src/logiops-gui/main.cpp
    - src/logiops-gui/qml/config/GestureBuilder.qml
commits:
  - 3719ad6 feat(260531-gmt): add GestureActions predefined-action provider
  - 89f37e8 feat(260531-gmt): inline predefined gesture-action picker in builder
metrics:
  tasks: 2
  files: 6
  duration: ~5m
---

# Quick Task 260531-gmt: Predefined Options+-style Gesture Action Picker Summary

Gesture actions can now be assigned from a curated, categorized list (Volume Up,
Mute, Show Desktop, Copy, etc.) instead of forcing the user to physically press a
key, dispatched through the existing `GestureModel::setGestureKeypress`; the raw
key capture remains as a "Custom keystroke…" advanced fallback.

## What Was Built

### Task 1 — C++ predefined-action provider (`3719ad6`)
- New `GestureActions` QObject (`GestureActions.{h,cpp}`) exposing
  `Q_INVOKABLE QVariantList categories()` — an ordered list of
  `{ name, actions: [ { label, keys: [KEY_* ...] } ] }`.
- Full categorized set with EXACT libevdev names (combos ordered modifiers→key):
  - **Media:** Volume Up/Down, Mute, Play/Pause, Next/Previous Track.
  - **Brightness:** Brightness Up/Down.
  - **Desktops & Windows:** Desktop Left/Right (Ctrl+Alt+Arrow), Show Desktop
    (Super+D), Activities/Overview (Super), Maximize (Super+Up), Snap Left/Right
    (Super+Arrow).
  - **Edit & Navigation:** Copy, Paste, Back (Alt+Left), Forward (Alt+Right),
    Zoom In (Ctrl+Equal), Zoom Out (Ctrl+Minus).
  - **System:** Screenshot (Print).
- Doc comment flags the Desktops/Windows/Brightness/Zoom/Screenshot defaults as
  DE-dependent (GNOME/KDE common bindings, may need per-environment tweaking);
  Media is universal.
- Registered as the `gestureActions` QML context property in `main.cpp`, mirroring
  the `keyNames` bridge registration. Added to `logiops-gui-lib` in `CMakeLists.txt`.

### Task 2 — QML predefined-action picker (`89f37e8`)
- New `GesturePicker.qml`: renders `gestureActions.categories()` as a scrollable,
  width-constrained column grouped by category header, each action a fillWidth
  eliding row. A final "Custom keystroke…" row at the bottom. Emits
  `actionPicked(keys, label)` for predefined choices and `customRequested()` for
  the advanced fallback. The ScrollView pins `contentWidth: availableWidth` and
  caps height at 280px so the full set never clips or overflows the 360px panel.
- `GestureBuilder.qml` action sub-section now toggles the inline `GesturePicker`
  (consistent with the existing inline-expand pattern, not a modal). Selecting a
  predefined action calls `gestureModel.setGestureKeypress(activeDirection, keys)`
  and shows an "Action: <label>" readout; `pickerOpen`/`pickedActionLabel` reset on
  direction switch so a choice never leaks across directions. The picker is gated
  by `showsAction` (OnInterval/OnRelease only) — Axis never shows the keystroke
  picker.
- "Custom keystroke…" re-emits the existing `chooseActionRequested(direction)`,
  which `ReassignPanel` already routes to the raw `KeyCaptureField` scoped to the
  gesture direction (`onKeysCaptured` → `setGestureKeypress`). That power-user path
  is unchanged.

## Key Link
`gestureActions` context property → `GesturePicker` (categories) → user selection →
`gestureModel.setGestureKeypress(direction, action.keys)` (existing two-step
SetAction("Keypress") + SetKeys on the gesture node).

## Verification
- `cmake --build build --target logiops-gui` exits 0, -Werror clean (both tasks).
- qmlcache cleared + rebuilt after the QML/QML-module change; `GesturePicker_qml`
  compiled into the cache and linked.
- Offscreen smoke (`QT_QPA_PLATFORM=offscreen`, broad grep): **QML OK** (no
  "not a type" / "Unknown method" / "Error:").
- `ctest --test-dir build`: **14/14 passed**.
- grep checks: `gestureActions` consumed in QML; `setGestureKeypress` wired in both
  the predefined path (GestureBuilder) and the custom fallback (ReassignPanel);
  "Custom keystroke…" present.

## Deviations from Plan
None — plan executed as written. Implementation choice: built the optional standalone
`GesturePicker.qml` component (the plan explicitly left this to executor discretion)
rather than overloading the shared Keystroke category, keeping GestureBuilder focused
and the predefined picker self-contained.

## Self-Check: PASSED
- Files: GestureActions.h, GestureActions.cpp, GesturePicker.qml — all FOUND.
- Commits: 3719ad6, 89f37e8 — both FOUND.
