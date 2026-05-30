---
phase: 02-d-bus-client-device-list
fixed_at: 2026-05-30T00:00:00Z
review_path: .planning/phases/02-d-bus-client-device-list/02-REVIEW.md
iteration: 1
findings_in_scope: 3
fixed: 3
skipped: 0
status: all_fixed
---

# Phase 2: Code Review Fix Report

**Fixed at:** 2026-05-30
**Source review:** .planning/phases/02-d-bus-client-device-list/02-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 3 (WR-01, WR-02, WR-03)
- Fixed: 3
- Skipped: 0

Verification: full `cmake -S . -B build -DBUILD_GUI=ON -DCMAKE_CXX_FLAGS="-Werror"`
configure + `cmake --build build` is clean (no warnings/errors). All 5 ctest
tests pass: `hidpp_bounds`, `device_model`, `device_model_noflicker`,
`battery_feature`, `unified_battery`.

## Fixed Issues

### WR-01: GUI hardcodes systemBus(), ignoring the USE_USER_BUS dev build

**Files modified:** `src/logiops-gui/DaemonConnection.cpp`, `src/logiops-gui/CMakeLists.txt`
**Commit:** 229b392
**Applied fix:** Added a `daemonBus()` helper that returns `sessionBus()` under
`#ifdef USE_USER_BUS` and `systemBus()` otherwise, mirroring the daemon's bus
selection in `src/logid/logid.cpp` (~line 154). The constructor initializer now
uses `daemonBus()` instead of the unconditional `systemBus()`. System bus stays
the production default. Added a `target_compile_definitions(logiops-gui-lib
PUBLIC USE_USER_BUS)` guard so the define is explicit on the GUI target and not
solely dependent on the root `add_definitions` ordering.

### WR-02: NameOwnerChanged "owner changed" (coalesced fast restart) can desync proxies

**Files modified:** `src/logiops-gui/DaemonConnection.cpp`
**Commit:** ed4d2f9
**Applied fix:** Connected `QDBusServiceWatcher::serviceOwnerChanged(name, old,
new)` in addition to `serviceRegistered`/`serviceUnregistered`. A coalesced
restart (both owners non-empty) now routes to `onDaemonUp` (non-empty new owner)
or `onDaemonDown` (empty new owner). `onDaemonUp` already tears down all proxies
before re-enumerating, so the handler is idempotent with `serviceRegistered` and
never double-subscribes.

### WR-03: ConnectionState "Offline" is never reachable, but the whole UI keys off it

**Files modified:** `src/logiops-gui/DeviceModel.h`, `src/logiops-gui/DeviceModel.cpp`, `src/logiops-gui/DaemonConnection.cpp`
**Commit:** 25e60e7
**Applied fix:** Implemented the UI-SPEC contract (02-UI-SPEC.md lines 182, 198:
"connection loss = dim-in-place (offline state), NOT removal — only genuine
unpair/unplug-of-receiver removes the row") — i.e. the reviewer's preferred
option (a), drive Offline rather than delete the dead branch.

- `DeviceModel`: added `onConnectionStateChanged(path, state)` which sets an
  explicit `ConnectionState` and emits the same targeted, no-flicker
  `dataChanged({ConnectionStateRole})` on the one row (with a no-op guard when
  the state is unchanged). `onStatusChanged` now routes through it, mapping the
  daemon's `Active` bool to `Online`/`Sleeping`.
- `DaemonConnection`: a device that is present on the bus but unreachable (its
  `Properties.GetAll` read fails — vanished mid-enumerate / receiver lost) is now
  registered `Offline` (dimmed, battery "—") instead of being faked `Online` or
  silently dropped. This makes the delegate's Offline badge / 55% dimming /
  "offline" accessibility text genuinely reachable.

`onDeviceRemoved` still performs a true `beginRemoveRows`/`endRemoveRows` for a
genuine unpair, so the no-flicker / no-reset model contract is preserved and the
`device_model` + `device_model_noflicker` (+ `battery_feature` / `unified_battery`)
tests stay green.

> Note: WR-03 touches connection-state mapping logic. The model-level no-flicker
> and add/remove invariants are covered by the passing `device_model` /
> `device_model_noflicker` units. The new Offline-on-unreachable path in
> `DaemonConnection::addDevice` is not yet exercised by an automated test (it
> requires a live/mocked bus reply error), so confirm the live behavior manually
> when a device drops off an unreachable receiver.

## Deferred (out of scope — INFO findings)

The following INFO findings from 02-REVIEW.md were explicitly out of scope for
this fix pass and are acknowledged as deferred:

- **INFO-01:** Clamp battery percentage to 0..100 at the daemon decoder
  (defense-in-depth at the trust boundary).
- **INFO-02:** Reconcile the BatteryStatus (0x1000) vs UnifiedBattery (0x1004)
  "charge complete" charging semantics with a clarifying comment.
- **INFO-03:** `kindFromPid` defaults unknown PIDs to `Mouse`; consider returning
  `UnknownKind` for genuinely unclassified PIDs.
- **INFO-04:** `copyUsermodCommand` hardcodes the group name "logiops"; centralize
  in `ipc_defs.h`.
- **INFO-05:** `addDevice` error path / `recomputePopulation` reconciliation on
  every watcher-lambda exit (RAII/`QScopeGuard`).

---

_Fixed: 2026-05-30_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
