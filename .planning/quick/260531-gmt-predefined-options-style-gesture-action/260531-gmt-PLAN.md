---
quick_id: 260531-gmt
description: Predefined Options+-style gesture action picker (replaces raw key capture; keeps custom keystroke as advanced)
mode: quick
date: 2026-05-31
---

# Quick Task 260531-gmt

## Objective

For gesture actions, replace the raw "press a key" capture with a curated, Options+-style list of PREDEFINED actions the app already maps to the correct evdev keys (so the user never needs the physical key, e.g. there is no Volume-Up key on their keyboard). Each predefined action is a keystroke combo sent through the EXISTING `GestureModel::setGestureKeypress(direction, evdevNames)` (which does the two-step SetAction("Keypress") + SetKeys on the gesture node). Keep the current raw key-capture as an "Advanced: custom keystroke…" option. Scope = the FULL set (media + brightness + desktop/window + edit/nav + system); user accepted that several DE-dependent ones (desktop switch, overview, snap, screenshot, brightness, zoom) use sensible GNOME/KDE defaults and may vary by environment.

## Tasks

### Task 1 — C++ provider of predefined gesture actions

<files>
- src/logiops-gui/GestureActions.h (NEW)
- src/logiops-gui/GestureActions.cpp (NEW)
- src/logiops-gui/CMakeLists.txt (add the source to logiops-gui-lib)
- src/logiops-gui/main.cpp (register as a QML context property, mirror how `keyNames` / `deviceModel` are registered)
</files>

<read_first>
- src/logiops-gui/KeyNameMapper.cpp — confirms the evdev name format is the libevdev `KEY_*` names (e.g. "KEY_LEFTCTRL", "KEY_VOLUMEUP", "KEY_LEFTMETA"); the predefined keys MUST use these exact names.
- src/logiops-gui/main.cpp — how context properties (keyNames, deviceModel, configState, deviceControllerFactory) are constructed and `engine.rootContext()->setContextProperty(...)`.
- src/logiops-gui/GestureModel.h — `setGestureKeypress(const QString& direction, const QStringList& evdevNames)` is the sink.
</read_first>

<action>
Create a small QObject `GestureActions` (no business logic beyond static data) that exposes, to QML, a categorized list of predefined actions. Each action = { id, label, category, keys (QStringList of evdev KEY_* names) }. Expose via a Q_INVOKABLE returning a structured model (e.g. `Q_INVOKABLE QVariantList categories()` returning [{ name, actions:[{label, keys}] }]), suitable for a QML Repeater/ListView. Register it in main.cpp as context property `gestureActions`.

The action set (use these EXACT evdev KEY_* names; combos are ordered modifiers… then key):
- **Media:** Volume Up [KEY_VOLUMEUP] · Volume Down [KEY_VOLUMEDOWN] · Mute [KEY_MUTE] · Play/Pause [KEY_PLAYPAUSE] · Next Track [KEY_NEXTSONG] · Previous Track [KEY_PREVIOUSSONG]
- **Brightness:** Brightness Up [KEY_BRIGHTNESSUP] · Brightness Down [KEY_BRIGHTNESSDOWN]
- **Desktops & Windows:** Desktop Left [KEY_LEFTCTRL,KEY_LEFTALT,KEY_LEFT] · Desktop Right [KEY_LEFTCTRL,KEY_LEFTALT,KEY_RIGHT] · Show Desktop [KEY_LEFTMETA,KEY_D] · Activities/Overview [KEY_LEFTMETA] · Maximize [KEY_LEFTMETA,KEY_UP] · Snap Left [KEY_LEFTMETA,KEY_LEFT] · Snap Right [KEY_LEFTMETA,KEY_RIGHT]
- **Edit & Navigation:** Copy [KEY_LEFTCTRL,KEY_C] · Paste [KEY_LEFTCTRL,KEY_V] · Back [KEY_LEFTALT,KEY_LEFT] · Forward [KEY_LEFTALT,KEY_RIGHT] · Zoom In [KEY_LEFTCTRL,KEY_EQUAL] · Zoom Out [KEY_LEFTCTRL,KEY_MINUS]
- **System:** Screenshot [KEY_PRINT]

Add a short doc comment that the Desktops/Windows/Brightness/Zoom/Screenshot defaults are DE-dependent (GNOME/KDE common bindings) and may need per-environment tweaking — these are sensible defaults, not guaranteed universal.
</action>

<verify>
- `test -f src/logiops-gui/GestureActions.h && test -f src/logiops-gui/GestureActions.cpp`
- `grep -n "gestureActions" src/logiops-gui/main.cpp` shows the context-property registration.
- `grep -c "KEY_" src/logiops-gui/GestureActions.cpp` >= 20 (the action set present).
- `cmake --build build --target logiops-gui` exits 0 (-Werror clean).
</verify>

<done>GestureActions provider compiles, is registered as a QML context property, and carries the full categorized action set with correct KEY_* names.</done>

### Task 2 — QML predefined-action picker in the gesture builder

<files>
- src/logiops-gui/qml/config/GestureBuilder.qml (the "Choose what this direction does" action button + the chooseActionRequested signal)
- src/logiops-gui/qml/config/ReassignPanel.qml (currently onChooseActionRequested expands the raw Keystroke capture; redirect to the predefined picker, keep raw capture as the advanced fallback)
- (optional) a small new GesturePicker.qml component if cleaner — your call
</files>

<read_first>
- src/logiops-gui/qml/config/GestureBuilder.qml — the Action sub-section (showsAction) + the "Choose what this direction does" Button that emits chooseActionRequested(direction). Note: the picker applies ONLY to action-bearing modes (OnInterval "Repeat while moving" / OnRelease "Do once when moved far enough"); Axis "Adjust proportionally" maps movement to an axis and takes no keystroke action — do not show the keystroke picker there.
- src/logiops-gui/qml/config/ReassignPanel.qml — onChooseActionRequested sets activeGestureDirection + expanded="Keypress" today (raw capture via KeyCaptureField -> setGestureKeypress). The bound `gestureModel.setGestureKeypress(direction, names)` is the sink to reuse.
- (read the just-fixed ScrollView width pattern: contentWidth: availableWidth — keep the picker width-constrained so it never clips.)
</read_first>

<action>
Replace the gesture action flow so "Choose what this direction does" presents the PREDEFINED `gestureActions` list (categorized — a scrollable, width-constrained list of buttons/rows grouped by category name), shown inline in the gesture builder (or in the reassign panel scope, consistent with the existing inline-expand pattern — NOT a blocking modal). Selecting a predefined action calls `gestureModel.setGestureKeypress(activeGestureDirection, action.keys)` and returns to the builder with the action's label reflected (the preview sentence + an "Action: <label>" readout). Add a final "Custom keystroke…" item at the bottom that opens the EXISTING raw KeyCaptureField path (so power users can still capture an arbitrary combo) — keep that path working unchanged. The picker must be width-constrained (fillWidth + wrap/elide) so nothing clips in the 360px panel. QML renders only — the action data and key lists come from the `gestureActions` context property; the keystroke dispatch is the existing C++ setGestureKeypress.
</action>

<verify>
- `grep -n "gestureActions" src/logiops-gui/qml/config/*.qml` shows the QML consuming the provider.
- `grep -n "setGestureKeypress\|Custom keystroke" src/logiops-gui/qml/config/ReassignPanel.qml` shows predefined selection wired to setGestureKeypress AND the custom-keystroke fallback retained.
- Clear qmlcache + rebuild: `rm -rf build/src/logiops-gui/.rcc build/src/logiops-gui/CMakeFiles/logiops-gui.dir/.rcc build/src/logiops-gui/logiops && cmake --build build --target logiops-gui` exits 0.
- Offscreen smoke clean: `sg logiops -c 'timeout 6 env QT_QPA_PLATFORM=offscreen ./build/src/logiops-gui/logiops-gui 2>&1' | grep -iE "not a type|Unknown method|Error:"` prints nothing.
- `ctest --test-dir build` stays 14/14 green.
</verify>

<done>Choosing a gesture's action shows the predefined Options+-style list (no raw key needed); selecting one binds it via setGestureKeypress; "Custom keystroke…" still opens the raw capture; content fits the panel; build + suite green.</done>

## must_haves

- truth: A user can assign a gesture action (e.g. Volume Up) by picking it from a predefined list, without pressing any physical media key.
- truth: The predefined actions map to the correct libevdev KEY_* combos.
- truth: A "Custom keystroke…" advanced option still opens the raw capture.
- artifact: src/logiops-gui/GestureActions.{h,cpp} (the provider)
- artifact: src/logiops-gui/qml/config/* (the predefined picker)
- key_link: gestureActions context property -> QML picker -> gestureModel.setGestureKeypress
