---
quick_id: 260531-fye
description: Fix logiops-gui usability — responsive ReassignPanel for narrow windows + async GestureModel seedFromDaemon
mode: quick
date: 2026-05-31
---

# Quick Task 260531-fye

## Objective

Unblock the Phase 4 gesture UAT: the GUI is unusable when the window is narrow (tiling WM / XWayland forces it below the 720px min, so the Buttons tab's device-render + 360px reassign panel sit side-by-side and overflow — the reassign panel's controls clip off-screen and become unclickable). Also finish the gesture-builder freeze fix: `GestureModel::seedFromDaemon` still does synchronous blocking D-Bus calls on the GUI thread (only bounded by a 250ms-per-call timeout).

## Tasks

### Task 1 — Responsive Buttons-tab layout (the unblocker)

<files>
- src/logiops-gui/qml/config/ButtonsTab.qml (the RowLayout: DeviceRender + BindingList on the left, ReassignPanel on the right)
- src/logiops-gui/qml/config/ReassignPanel.qml (inner content must already fillWidth/scroll — verify CategoryRows, GestureBuilder, mode pills, preview do not exceed the panel width)
- src/logiops-gui/qml/Theme.qml (sidebarWidth 280, actionPanelWidth 360, actionPanelMin 320, deviceRenderMax 420, windowMinWidth 720 — reference only)
</files>

<read_first>
- src/logiops-gui/qml/config/ButtonsTab.qml — current layout: a RowLayout/SplitView placing the device render (Layout.preferredWidth up to deviceRenderMax 420) beside the ReassignPanel (Layout.preferredWidth actionPanelWidth 360, animates 0->360). When the tab's available width < ~720, these overflow and the panel's right edge clips off-window.
- src/logiops-gui/qml/config/ReassignPanel.qml — the panel is a Rectangle with a ColumnLayout(anchors.fill, margins) + a ScrollView(ColumnLayout width: parent.width). Confirm every child uses Layout.fillWidth and text elides/wraps so nothing exceeds the panel's own width.
</read_first>

<action>
Make the Buttons tab usable when the window is narrower than (render + panel). Approach (pick the cleanest that fits the existing structure):
- When the reassign panel is open (editingRow >= 0) AND the tab width is below a breakpoint (e.g. width < Theme.windowMinWidth, ~720), have the ReassignPanel take the FULL tab width as an overlay on top of the device render (anchors.fill of the tab, or Layout.fillWidth with the render hidden/0-width), instead of sitting beside it. When wide, keep the current side-by-side render + 360px panel. The panel must always be fully on-screen and its Close button reachable.
- Ensure the ReassignPanel's inner ScrollView actually scrolls vertically (the content — categories + the expanded GestureBuilder — can be taller than the panel) and that NO child exceeds the panel width: every row/pill/preview Text uses Layout.fillWidth with wrapMode: Text.WordWrap or elide: Text.ElideRight as appropriate, so labels like "Do once when moved far enough" and the preview sentence never clip horizontally.
- Keep the existing reveal animation/behavior intact when wide. Do not regress the normal (wide) layout.
Concrete: add a `readonly property bool narrow: <tab>.width < Theme.windowMinWidth` (or a dedicated Theme token) and switch the panel between side-panel and full-width-overlay on it.
</action>

<verify>
- `grep -nE "fillWidth|ElideRight|WordWrap|narrow" src/logiops-gui/qml/config/ButtonsTab.qml` shows the responsive switch + width-constrained children.
- Clear qmlcache then rebuild: `rm -rf build/src/logiops-gui/.rcc build/src/logiops-gui/CMakeFiles/logiops-gui.dir/.rcc build/src/logiops-gui/logiops && cmake --build build --target logiops-gui` exits 0.
- Offscreen smoke clean: `sg logiops -c 'timeout 6 env QT_QPA_PLATFORM=offscreen ./build/src/logiops-gui/logiops-gui 2>&1' | grep -iE "not a type|Unknown method|Error:"` prints nothing.
</verify>

<done>The reassign panel is fully reachable and its controls clickable when the window is narrow (no off-screen clipping); wide layout unchanged; build + smoke clean.</done>

### Task 2 — Async GestureModel::seedFromDaemon

<files>
- src/logiops-gui/GestureModel.cpp (seedFromDaemon — currently synchronous blocking QDBusInterface::call with a 250ms timeout)
- src/logiops-gui/GestureModel.h (may need a small helper/state for the async flow)
- src/logiops-gui/DeviceController.cpp (reference: introspectInterfaces uses QDBusMessage::createMethodCall + _bus.asyncCall + QDBusPendingCallWatcher — mirror this non-blocking pattern)
- tests/phase4/GestureModelTest.cpp (keep green; the recording-subclass seam is test-only and unaffected)
</files>

<read_first>
- src/logiops-gui/GestureModel.cpp — seedFromDaemon() loops directions × candidate Gesture.<type> interfaces doing blocking GetAll/GetConfig/GetThreshold; called from the live constructor. These block the GUI event loop.
- src/logiops-gui/DeviceController.cpp — the canonical async pattern in this codebase: build a QDBusMessage, _bus.asyncCall, attach a QDBusPendingCallWatcher, parse in the finished lambda, emit a *Changed signal so QML re-binds.
</read_first>

<action>
Convert seedFromDaemon to fully NON-BLOCKING: replace the synchronous `.call()` chain with `_bus.asyncCall(...)` + QDBusPendingCallWatcher (mirror DeviceController::introspectInterfaces). For each of the 4 directions, async-probe the gesture node's present interface and its params; when a reply lands, populate that direction's DirectionState (mode/plainMode/granularity) and emit configuredChanged(direction) + previewChanged() so the already-open builder updates live. The constructor must return immediately without any blocking D-Bus round-trip. Keep the `_live` guard. Preserve the existing seeded data shape so previewSentence/isConfigured/modeOf keep working. Remove the now-unnecessary 250ms setTimeout calls (no longer blocking).
</action>

<verify>
- `grep -n "asyncCall\|QDBusPendingCallWatcher" src/logiops-gui/GestureModel.cpp` shows the async conversion; `grep -c "\.call(" src/logiops-gui/GestureModel.cpp` shows the blocking GetAll/GetConfig/GetThreshold `.call(` are gone from seedFromDaemon (other intentional blocking, if any, justified in a comment).
- `cmake --build build --target logiops-gui` exits 0 (-Werror clean).
- `ctest --test-dir build` is 14/14 green.
</verify>

<done>Opening the gesture builder makes no synchronous D-Bus round-trip on the GUI thread; existing config still seeds in (async) and the builder updates live; build + full suite green.</done>

## must_haves

- truth: The reassign panel is fully usable (no clipped/unreachable controls) when the GUI window is narrow.
- truth: Opening the gesture builder never blocks the GUI event loop on D-Bus.
- artifact: src/logiops-gui/qml/config/ButtonsTab.qml (responsive switch)
- artifact: src/logiops-gui/GestureModel.cpp (async seedFromDaemon)
- key_link: ButtonsTab narrow-mode -> ReassignPanel full-width overlay
- key_link: GestureModel constructor -> async seed (no blocking call)
