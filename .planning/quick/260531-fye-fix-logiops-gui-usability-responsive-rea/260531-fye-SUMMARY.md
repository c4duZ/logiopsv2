---
quick_id: 260531-fye
description: Fix logiops-gui usability — responsive ReassignPanel for narrow windows + async GestureModel seedFromDaemon
mode: quick
date: 2026-05-31
subsystem: logiops-gui
tags: [qml, responsive, dbus, async, gestures, usability]
key-files:
  modified:
    - src/logiops-gui/qml/config/ButtonsTab.qml
    - src/logiops-gui/qml/config/ReassignPanel.qml
    - src/logiops-gui/qml/config/GestureBuilder.qml
    - src/logiops-gui/GestureModel.cpp
    - src/logiops-gui/GestureModel.h
commits:
  - 4a2b3f1: responsive Buttons tab — full-width reassign overlay when narrow
  - 4a38ecc: make GestureModel::seedFromDaemon fully non-blocking
metrics:
  tasks: 2
  files: 5
  completed: 2026-05-31
---

# Quick Task 260531-fye: logiops-gui Usability (Responsive Reassign + Async Gesture Seed) Summary

Made the Buttons tab usable in a tiling-WM-narrowed window by turning the reassign panel into a full-width overlay below the 720px breakpoint, and removed the last GUI-thread D-Bus block by making `GestureModel::seedFromDaemon` fully asynchronous.

## What Changed

### Task 1 — Responsive Buttons-tab layout (4a2b3f1)

The Buttons tab placed the clickable device render (up to 420px) beside a 360px reassign panel in a `RowLayout`. When a tiling WM / XWayland forced the window below the 720px min, the two overflowed and the panel's right edge (with its controls and Close button) clipped off-window, making reassignment impossible.

- **ButtonsTab.qml**: added `readonly property bool narrow: root.width < Theme.windowMinWidth` and `panelOverlay: narrow && editingRow >= 0`. When `panelOverlay`, the `DeviceRender` collapses to width 0 (animated) and hides, the `BindingList` hides, and the `ReassignPanel` switches from `Layout.preferredWidth: 360` to `Layout.fillWidth` so it occupies the entire tab as an overlay — always fully on-screen. The wide side-by-side layout (render + 0→360 animated panel reveal) is unchanged.
- **ReassignPanel.qml**: bounded the `CategoryRow` label `Row` with a right anchor and gave the label `width` + `elide: Text.ElideRight` so a long category label can't overflow the panel edge.
- **GestureBuilder.qml**: the `ModePill` label changed from a centered free `Text` to a width-anchored `wrapMode: Text.WordWrap` text, so long mode labels ("Do once when moved far enough") wrap inside the pill instead of clipping past it when the panel is narrow.

### Task 2 — Async `GestureModel::seedFromDaemon` (4a38ecc)

The live constructor called `seedFromDaemon()`, which looped 4 directions × 4 candidate `.Gesture.<type>` interfaces issuing **synchronous** `QDBusInterface::call` (`GetAll` / `GetConfig` / `GetThreshold`), each only bounded by a `setTimeout(250)`. That blocked the GUI event loop when the gesture builder opened (worst case multiple 250ms stalls, and a real HID++ hardware read on an asleep device).

Rewrote it fully async, mirroring `DeviceController::introspectInterfaces`:

- `seedFromDaemon()` now only kicks off `probeDirection(dir, 0)` for each of the 4 directions and returns immediately — the constructor makes **zero** blocking D-Bus round-trips.
- `probeDirection(direction, typeIdx)`: builds a `QDBusMessage::createMethodCall(... Properties.GetAll <iface>)`, fires `_bus.asyncCall` + a `QDBusPendingCallWatcher`. On a non-error reply the interface is present (first-present-wins) → records mode/plainMode, emits `configuredChanged`+`previewChanged`, and kicks `seedParam`. On error/absence it chains to the next candidate type.
- `seedParam(direction, type)`: async-reads the mode's granularity getter (`GetConfig` for OnInterval/Axis, `GetThreshold` for OnRelease; None has none), stores it, and re-emits `previewChanged` so an already-open builder updates live.
- Removed the `setTimeout(250)` workarounds (no longer blocking).

The `_live` guard, seeded data shape (mode/plainMode/granularity), and the test recording-subclass seam are all preserved — `previewSentence`/`isConfigured`/`modeOf` keep working unchanged.

## Verification

| Check | Result |
| ----- | ------ |
| ButtonsTab grep (fillWidth/narrow/elide) | Present (narrow, panelOverlay, fillWidth) |
| qmlcache cleared + `cmake --build build --target logiops-gui` | Exit 0 |
| Offscreen smoke (`not a type` / `Unknown method` / `Error:`) | QML OK (no matches) |
| `asyncCall` / `QDBusPendingCallWatcher` in GestureModel.cpp | Present in seed path |
| Blocking `.call(` in seedFromDaemon | None (only a comment mentions it) |
| `setTimeout` / 250ms leftovers | None |
| `cmake --build build --target logiops-gui` (-Werror) | Exit 0 |
| `ctest --test-dir build` | 14/14 passed |

## Deviations from Plan

None — both tasks executed as written.

## Notes

- The wide-layout reveal animation and behavior are intact; the device render now also animates its width-collapse when entering overlay mode for a smooth transition.
- `seedParam` for `OnInterval` reads `GetConfig` arg[0] as the interval (matching the prior synchronous behavior), `OnRelease` reads the threshold reply arg[0], `Axis` reads `GetConfig` arg[1] (multiplier) — same granularity mapping as before, now over async replies.

## Self-Check: PASSED
