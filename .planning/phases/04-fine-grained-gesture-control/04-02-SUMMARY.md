---
phase: 04-fine-grained-gesture-control
plan: 02
subsystem: logiops-gui
tags: [gesture, gui, dbus, model, preview, gest-01, gest-04]
requires:
  - "Action.Gesture.SetGesture(direction,type) daemon surface (existing, hardened by Plan 01)"
  - "ButtonsModel two-step async pattern (Phase 3)"
  - "ConfigState markDirty seam (Phase 3)"
provides:
  - "GestureModel: button-scoped 4-direction gesture builder brain (C++)"
  - "Typed PizzaPixlLogiOpsActionGestureInterface proxy"
  - "C++-composed previewSentence + granularity human readout (GEST-04)"
affects:
  - "src/logiops-gui (new model + proxy, no behavior change to existing models)"
tech-stack:
  added: []
  patterns:
    - "Two-step async D-Bus dispatch via virtual seam (performSetGesture/performParamCall)"
    - "Validate-before-dispatch mode allowlist (single source of truth)"
    - "NOTIFYable C++-composed plain-language preview (zero logic in QML)"
key-files:
  created:
    - src/logiops-gui/GestureModel.h
    - src/logiops-gui/GestureModel.cpp
    - src/logiops-gui/dbus/logid_action_gesture.xml
  modified:
    - src/logiops-gui/CMakeLists.txt
decisions:
  - "Mode 2 canonical type is OnRelease, never OnThreshold (interface-name collision -> InvalidGesture)"
  - "Nothing maps to None (makeGesture branches on interface_name None), never NoPress"
  - "GestureModel is a button-scoped QObject (4 directions of one button), not a per-button list model"
metrics:
  duration: "~7m"
  tasks: 3
  files: 4
  completed: 2026-05-31
---

# Phase 4 Plan 02: Gesture Builder GUI Brain Summary

Button-scoped `GestureModel` mirroring `ButtonsModel`'s two-step async dispatch: drives the daemon's existing `Action.Gesture.SetGesture(direction,type)` mode switch, sequences the per-mode `.Gesture.<mode>.Set*` param setters behind it, enforces a locked mode allowlist (OnInterval/OnRelease/Axis/None) with validate-before-dispatch, and composes the GEST-04 plain-language preview sentence + granularity human readout entirely in C++. Turns `phase4_gesture_model` GREEN.

## What Was Built

- **`dbus/logid_action_gesture.xml`** — trimmed introspection for the stable `pizza.pixl.LogiOps.Action.Gesture` interface with the single `SetGesture(direction, type)` method. Comments kept free of `--` and bare `<` (Phase 2 lesson). Generates `PizzaPixlLogiOpsActionGestureInterface`.
- **CMake wiring** — `qt_add_dbus_interface(... logid_action_gesture_proxy)` and `GestureModel.cpp` added to `target_sources(logiops-gui-lib ...)`. Only the stable parent interface gets a typed proxy; the dynamic `.Gesture.<mode>` child interfaces are driven via generic `QDBusInterface` (mirrors `ButtonsModel::performParamCall`).
- **`GestureModel.{h,cpp}`** — button-scoped QObject representing the 4 cardinal directions of one selected button:
  - Live ctor `(buttonPath, bus, parent)` + no-bus test ctor `(parent)`.
  - Protected virtual seam `performSetGesture` / `performParamCall` (the recording-subclass test override point).
  - `setMode(direction, plainMode)` validates `direction ∈ {up,down,left,right}` and maps `plainMode` through the locked allowlist BEFORE any dispatch; rejects unknown direction/mode with `editRejected` and zero dispatch.
  - `setGranularity` (clamps negatives, keeps the 0 reset sentinel, routes to SetInterval/SetThreshold/SetMultiplier per current mode) and `setGestureAction` (SetAction on the discrete modes).
  - NOTIFYable `previewSentence`, `granularityReadout`, `activeDirection`; `markDirty()` on each successful edit when `ConfigState` is wired.
  - `previewSentence` composed in C++ from `{activeDirection, mode, action, granularity}` using the UI-SPEC §Copywriting templates verbatim; `granularityPhrase` maps a raw value to "small flick" … "long swipe".

## Mode Allowlist (locked, single source of truth)

| Plain language | Daemon type sent |
|----------------|------------------|
| Repeat while moving | `OnInterval` |
| Do once when moved far enough | `OnRelease` (never `OnThreshold`) |
| Adjust proportionally | `Axis` |
| Nothing | `None` (never `NoPress`) |

`OnThreshold` and `NoPress` appear ONLY in an explanatory comment documenting why they are never sent; no code path dispatches them.

## Verification

- `ctest --test-dir build -R phase4_gesture_model --output-on-failure` — **PASS** (two-step order, mode allowlist + rejection, preview sentence "Moving up does nothing.").
- Full suite: `ctest --test-dir build` — **14/14 PASS** (includes `phase4_gesture_math`).
- `cmake --build build` — clean, no `error:` (daemon CI is -Werror).
- Typed `Action.Gesture` proxy generates from the trimmed XML.

## Threat Model Coverage

- **T-04-02-01** (mitigate): `setMode` validates direction + maps mode through the allowlist before any D-Bus call; rejects with a signal and zero dispatch.
- **T-04-02-02** (mitigate): single static allowlist; never sends `OnThreshold`/`NoPress` (asserted by the model test + grep).
- **T-04-02-03** (mitigate): `setGranularity` rejects negatives; keeps the documented 0-reset sentinel; sends Axis multiplier as a double.
- **T-04-02-04** (mitigate): `performParamCall` sequenced AFTER `performSetGesture` lands via a Properties.GetAll hop on the ordered connection (exact `ButtonsModel` mirror) — the `.Gesture.<mode>` setter never hits UnknownMethod.
- **T-04-02-05** (accept): preview/readout strings carry only plain-language copy + the user's own chosen action; no daemon internals or HID++ units leak.

## Deviations from Plan

Tasks 2 and 3 were planned as two TDD steps but share a single test binary (`phase4_gesture_model_test`) that does not link until BOTH the dispatch/allowlist AND the preview members exist. They were therefore delivered in one `GestureModel.{h,cpp}` and committed as a single `feat(04-02)` commit covering both. No behavioral content was dropped — the dispatch/allowlist (Task 2) and the preview composition + granularity readout (Task 3) are both present and both assert GREEN. No auto-fixes (Rules 1-3) were needed; the existing daemon surface and ButtonsModel pattern were sufficient.

## Self-Check: PASSED

- FOUND: src/logiops-gui/GestureModel.h
- FOUND: src/logiops-gui/GestureModel.cpp
- FOUND: src/logiops-gui/dbus/logid_action_gesture.xml
- FOUND commit 024a5cd (proxy XML + CMake)
- FOUND commit e3c8667 (GestureModel)
