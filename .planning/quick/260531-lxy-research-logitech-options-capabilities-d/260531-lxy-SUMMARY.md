---
quick_id: 260531-lxy
description: Fix button reassign (restore-default, mouse-button actions, read-back, two-step chaining) per Options+ research
mode: quick
date: 2026-05-31
subsystem: logiops-gui (Buttons tab)
tags: [buttons, reassign, dbus, options-plus-parity, qml]
key-files:
  modified:
    - src/logiops-gui/ButtonsModel.h
    - src/logiops-gui/ButtonsModel.cpp
    - src/logiops-gui/qml/config/ReassignPanel.qml
decisions:
  - "restoreDefault = SetAction(\"Default\") (un-divert), kept DISTINCT from clearAction = SetAction(\"None\") (diverted-but-inert)"
  - "Mouse buttons route through existing Keypress + BTN_* (no daemon change) rather than adding a ChangeButton daemon action"
  - "performParamCall chains off the real SetAction reply and fires only on success; reconcile re-probes the daemon's present .Action.<X> after each reassign"
  - "Both plan tasks shipped in one atomic commit because they edit the same three files with intertwined shared state (restoreDefault depends on the reconcile plumbing)"
metrics:
  duration: ~8m
  tasks: 2
  files: 3
  completed: 2026-05-31
---

# Quick Task 260531-lxy: Button-Reassign Fixes Summary

GUI-only fixes from the Options+ research diagnosis: a real "Restore default" (un-divert) distinct from "Disabled" (dead), assignable Middle/Back/Forward/Left/Right mouse buttons via the existing Keypress+BTN_* path, SetAction-chained param calls (fire only on success), and post-reassign read-back so the binding list reflects the daemon's actual applied action instead of an optimistic guess.

## What Changed

### Task 1 — Restore-default + mouse-button actions
- `ButtonsModel::restoreDefault(row)` — issues `SetAction("Default")`. The daemon's `makeAction` resets the config (`Action.cpp:78-80`) and `RemapButton` drops the `TemporaryDiverted` bit, so the device handles the control natively. Explicitly DISTINCT from `clearAction` (`"None"` → diverted-but-inert).
- `ButtonsModel::setMouseButton(row, btnName)` — re-emits a mouse button by routing through the proven `Keypress` two-step with a single `BTN_*` evdev name (`BTN_MIDDLE/BACK/FORWARD/LEFT/RIGHT`). No daemon change: `BTN_*` are `EV_KEY` codes the `InputDevice` already registers and `KeypressAction::toKeyCode` resolves.
- `mouseButtonSummary()` + a `"Default"` case in `defaultSummary()` give human labels ("Middle click", "Default (native)").
- `ReassignPanel.qml`: new **Mouse button** category (Middle/Back/Forward/Right/Left) and a **Restore default** row with helper copy clarifying Restore default (normal hardware function) vs Disabled (does nothing).

### Task 2 — Two-step chaining + read-back reconciliation
- `performSetAction` registers its `QDBusPendingCallWatcher` per button path in `_pendingSetAction`.
- `performParamCall` chains the per-type setter off that ACTUAL SetAction reply and fires **only on success** (`!reply.isError()`), replacing the old independent `Properties.GetAll` probe that merely assumed ordering and fired even when SetAction failed (silent param-drop at a nonexistent interface).
- `reconcileFromDaemon(row)` (virtual; no-op on the test path via the `_live` guard) chains a read-back after the relevant reply lands; `doReconcile` async-probes the present `.Action.<X>` interface (the same discovery `enumerate()` uses) and corrects the row to the daemon's real applied action. Param-less setters reconcile off the SetAction reply; parametric setters reconcile after the param reply.
- The no-action case preserves the user-facing `Default (native)` vs `Disabled` distinction (the daemon can't tell them apart over D-Bus — both carry no action).
- `ReassignPanel.roleData` binds to `buttonsModel.count` (NOTIFYable) instead of `rowCount()` (research Bug 4).

## Verification

- Clean qmlcache rebuild (`logiops-gui`) succeeded; offscreen smoke: **QML OK**.
- `ctest --test-dir build`: **14/14 passed**. The phase3 button-model test is unchanged and green — the recording subclass still observes `SetAction` before `SetKeys`/`SetHost`, and `reconcileFromDaemon` is inert on the no-bus test path, so call counts/ordering assertions are unaffected.
- `-Werror` clean (no warnings emitted during the build).
- Plan grep checks pass: `restoreDefault`/`BTN_MIDDLE`/`Mouse button`/`Restore default` present; `QDBusPendingCallWatcher`/`reconcile`/`doReconcile` present.

## Deviations from Plan

**1. [Single combined commit instead of two per-task commits]**
- **Reason:** Both tasks edit the same three files (`ButtonsModel.h`, `ButtonsModel.cpp`, `ReassignPanel.qml`) with intertwined shared state — `restoreDefault` (Task 1) depends on the `reconcileFromDaemon` plumbing (Task 2), and both share new header members (`_pendingSetAction`, `rowForPath`, `doReconcile`). A line-level split into two commits that each compile cleanly was not achievable, so both tasks were committed atomically as `41c94a6` with the per-task breakdown in the commit body.

**2. [Rule 1 - Bug] Fixed ReassignPanel.roleData calling rowCount() as a function (research Bug 4)**
- **Found during:** Task 2.
- **Fix:** Bound to the NOTIFYable `buttonsModel.count` property so the guard re-evaluates on row changes.
- **Files modified:** `ReassignPanel.qml`. **Commit:** `41c94a6`.

No auth gates. No daemon code touched (`Action.cpp` read-only confirmation only).

## Self-Check: PASSED
- FOUND: src/logiops-gui/ButtonsModel.cpp (modified)
- FOUND: src/logiops-gui/ButtonsModel.h (modified)
- FOUND: src/logiops-gui/qml/config/ReassignPanel.qml (modified)
- FOUND: commit 41c94a6
