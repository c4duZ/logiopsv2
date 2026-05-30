---
phase: 02-d-bus-client-device-list
reviewed: 2026-05-30T00:00:00Z
depth: standard
files_reviewed: 8
files_reviewed_list:
  - src/logid/backend/hidpp20/features/BatteryStatus.cpp
  - src/logid/backend/hidpp20/features/UnifiedBattery.cpp
  - src/logid/features/DeviceBattery.cpp
  - src/logid/Device.cpp
  - src/logiops-gui/DaemonConnection.cpp
  - src/logiops-gui/DeviceModel.cpp
  - src/logiops-gui/main.cpp
  - src/logiops-gui/qml/DeviceDelegate.qml
findings:
  critical: 0
  warning: 3
  info: 5
  total: 8
status: issues_found
---

# Phase 2: Code Review Report

**Reviewed:** 2026-05-30
**Depth:** standard
**Files Reviewed:** 8
**Status:** issues_found

## Summary

Phase 2 adds the daemon-side HID++ battery features (0x1000 BatteryStatus, 0x1004 UnifiedBattery) and a Qt6/QML D-Bus GUI client. Overall the code is careful and well-documented, and the security-critical concerns from the prompt are largely handled correctly:

**Untrusted-HID bounds discipline (daemon, A):** SOUND. Both `decodeStatus` decoders guard `type() != Long`, `feature() == 0`, and `end - begin < 3` before touching `begin[0]`/`begin[2]`. A real incoming `Report` always has `_data` resized to a full Short/Long length (`Report.cpp:160-187`), and `paramBegin/paramEnd` derive from that allocation, so the `< 3` check can never under-read and the `begin[2]` access is always within the 16-byte Long param region. No off-by-one: the decoders read indices 0 and 2, and require `>= 3` bytes (indices 0,1,2 valid). The `getStatus()` re-wrap path is also bounded — `callFunction` returns at most 16 param bytes (sliced from a received Long report) which fits the fresh 16-byte Long report, so the `setParams` copy cannot overflow even with `NDEBUG` disabling the assert. No OOB read or write found in the battery decode path.

**Qt model invariants (GUI, B):** Mostly correct. `begin/endInsertRows` and `begin/endRemoveRows` are correctly paired and single-row-scoped, `dataChanged` carries targeted role lists, and every `_rows` access is guarded by `rowForPath() >= 0` or an explicit bounds check in `data()`. `clear()` is the only reset. The main concerns are a model/view contract mismatch on `ConnectionStateRole` vs the `Accessible.name` binding, and the GUI's hardcoded `systemBus()` ignoring the documented `USE_USER_BUS` dev build.

The GUI never touches hardware directly — confirmed (all access is via generated D-Bus proxies). No leaked `QDBusPendingCallWatcher` (all `deleteLater()` on finish), no double-subscribe (watcher created once; proxies torn down before re-enumerate), no UI-thread-blocking calls (all `asyncCall` / pending watchers). AccessDenied vs DaemonDown discrimination is implemented.

## Warnings

### WR-01: GUI hardcodes systemBus(), ignoring the USE_USER_BUS dev build

**File:** `src/logiops-gui/DaemonConnection.cpp:47`
**Issue:** `_bus(QDBusConnection::systemBus())` is unconditional. The daemon supports a `-DUSE_USER_BUS=ON` build (STACK.md, `CMakeLists.txt:15`) that moves the entire IPC surface to the **session** bus for non-root development. With the bus hardcoded to system, a developer running a user-bus daemon will get an `Enumerate` failure that surfaces as `DaemonDown` (no owner on the system bus) even though the daemon is running fine on the session bus — a confusing dev-time false negative, and it makes the GUI untestable without root. The header comment ("never session") states the production intent, but production-only is a build-time decision, not a hardcode.
**Fix:** Select the bus at compile time to mirror the daemon:
```cpp
#ifdef USE_USER_BUS
    , _bus(QDBusConnection::sessionBus()) {
#else
    , _bus(QDBusConnection::systemBus()) {
#endif
```
and propagate the `USE_USER_BUS` define into the `logiops-gui` target in CMake. Keep system bus as the default.

### WR-02: NameOwnerChanged "owner changed" (not just up/down) can desync proxies

**File:** `src/logiops-gui/DaemonConnection.cpp:50-55, 243-249`
**Issue:** `QDBusServiceWatcher` with `WatchForOwnerChange` emits `serviceRegistered`, `serviceUnregistered`, AND `serviceOwnerChanged` (old→new owner, both non-empty) when the daemon restarts fast enough that D-Bus coalesces the transition into a single owner change. Only `serviceRegistered`/`serviceUnregistered` are connected. If the daemon is replaced by a new instance without an intervening empty-owner window (e.g. socket-activated restart, or two transitions within one dispatch), neither slot fires, leaving the GUI bound to stale proxies that talk to the dead owner — devices freeze with no reconnect. This is exactly the "no double-subscribe / correct reconnect" concern from the prompt, on the rarer coalesced-restart edge.
**Fix:** Also handle the general signal and treat a non-empty new owner as up, empty as down:
```cpp
connect(_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
        [this](const QString&, const QString&, const QString& newOwner) {
            if (newOwner.isEmpty()) onDaemonDown({});
            else onDaemonUp({});
        });
```
`onDaemonUp` already tears down before re-enumerating, so this is idempotent and will not double-subscribe.

### WR-03: ConnectionState "Offline" is never reachable, but the whole UI keys off it

**File:** `src/logiops-gui/DeviceModel.cpp:136,158` and `src/logiops-gui/qml/DeviceDelegate.qml:21-24,43`
**Issue:** `connectionState` is only ever set to `Online` or `Sleeping` (from the `active` bool on add and on `StatusChanged`). The `Offline` (=2) state is defined in the enum and the delegate renders a distinct badge, 55% dimming, and "offline" accessibility text for it — but nothing ever produces it. When a device is removed from the bus (`DeviceRemoved`) the row is deleted, not marked offline, so a device that physically disconnects vanishes rather than showing "Offline". This is a logic gap: either the Offline state is dead code (remove it), or — more likely the intent given DEV-03 — `DeviceRemoved` should mark the row Offline for unpaired/asleep receivers rather than dropping it. As written, a user cannot distinguish "device gone" from "never present". Flagging as a warning because the UI promises a state the data layer can never deliver.
**Fix:** Decide the contract. If removal-means-offline is intended, in `removeDevice`/`onDeviceRemoved` set `connectionState = Offline` + `dataChanged({ConnectionStateRole})` instead of `beginRemoveRows`. If offline is genuinely out of scope for Phase 2, drop the `Offline` enum value and its delegate branches to avoid shipping unreachable UI paths.

## Info

### INFO-01: Battery percentage is not clamped to 0..100 in the daemon decoder

**File:** `src/logid/backend/hidpp20/features/BatteryStatus.h:88-91`, `UnifiedBattery.h:101-104`
**Issue:** A malicious/buggy device can report `percentage = 0xFF` (255). The decoder passes it through verbatim into `setBattery`/the D-Bus `Battery` (uint8) property. Not a memory-safety issue, and the delegate clamps to 0..100 (T-02-15), but the D-Bus surface and any other client see an out-of-range value. Defense-in-depth says clamp at the trust boundary (the decoder), not only in one view.
**Fix:** `status.percentage = pct > 100 ? 100 : pct;` in both decoders. The wire spec only defines 0..100.

### INFO-02: BatteryStatus charging-status comment/code mismatch on "almost full"

**File:** `src/logid/backend/hidpp20/features/BatteryStatus.h:58-61,92`
**Issue:** The header comment maps status `{1,2,3,4}` to charging and lists `2 = almost full`, `3 = full/charge complete`. Treating `3 = full/charge complete` as "charging" is debatable (a full battery on a charger is arguably charging; off charger it is not — the 0x1000 enum cannot distinguish). The 0x1004 decoder makes the opposite-looking choice (`3 = complete` counted as charging, `4 = error` not). The two features therefore use slightly different "charging" semantics for the "complete" state. Cosmetic for v1 but worth a comment reconciling the two so a future reader doesn't assume a bug.
**Fix:** Add a one-line note in both headers stating the deliberate divergence, or normalize both to exclude the "charge complete" state from `charging` if the UI should show a full-on-charger device as not-charging.

### INFO-03: kindFromPid defaults every unknown PID to Mouse, silently mislabeling keyboards

**File:** `src/logiops-gui/DeviceModel.cpp:78-90`
**Issue:** The `default:` branch returns `Mouse` for any PID not in the tiny keyboard table, so an untested keyboard shows a mouse icon. The `UnknownKind` enum value exists but is never returned. Acknowledged as deferred to Fase 3 in the comment, so info-level only.
**Fix:** None required for v1. Consider returning `UnknownKind` (neutral glyph) for PIDs outside both known ranges rather than guessing Mouse, to avoid confidently-wrong icons.

### INFO-04: copyUsermodCommand assumes the group name "logiops"

**File:** `src/logiops-gui/DaemonConnection.cpp:259-265`
**Issue:** The onboarding command hardcodes `usermod -aG logiops`. The actual group is defined by the installed D-Bus policy (`logiops-dbus.conf.in`) and could differ per packaging. If a distro ships a different group, the copied command is wrong. Low impact (it is guidance text), but it duplicates a value that lives authoritatively in the policy file.
**Fix:** Centralize the group name in `ipc_defs.h` (e.g. `#define LOGIOPS_DBUS_GROUP "logiops"`) and reuse it both in the policy template generation and here, mirroring the existing `SERVICE_ROOT_NAME` reuse pattern.

### INFO-05: addDevice error-path registers a row even after the device may have been removed

**File:** `src/logiops-gui/DaemonConnection.cpp:171-181`
**Issue:** In the GetAll error branch the code re-checks `_deviceProxies.contains(key)` before `onDeviceAdded`, which is correct. But it then unconditionally calls `recomputePopulation()` whether or not the row was added, and the success branch at line 215 does the same. Harmless (recompute is idempotent), but the early-return at line 183 (`!contains` → return) skips `recomputePopulation`, so a device removed mid-enumerate leaves population state momentarily stale until the next add/remove. Minor.
**Fix:** Call `recomputePopulation()` once on every exit path of the watcher lambda (or move it to a small RAII/`QScopeGuard`) so the population state is always reconciled after a GetAll completes, including the "removed before reply" case.

---

_Reviewed: 2026-05-30_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
