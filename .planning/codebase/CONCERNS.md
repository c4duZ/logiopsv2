# Concerns

Technical debt, bugs, security considerations, and fragile areas in the logiops codebase. This is a C++ daemon (`logid`) that runs as **root** and parses input from untrusted HID hardware, so security and input-handling robustness dominate the high-severity items.

## Severity Summary

| # | Concern | Severity | Location |
|---|---------|----------|----------|
| 1 | Sliced exception makes type-specific catches dead code | High | `src/logid/util/ExceptionHandler.cpp:26-38` |
| 2 | Runs as root with no sandboxing / privilege drop | High | `src/logid/logid.service.in:10`, `src/logid/logid.cpp:132-183` |
| 3 | Unbounded indexing of attacker-controlled HID reports | High | `src/logid/backend/hidpp/Device.cpp:121-126`, `hidpp10/ReceiverMonitor.cpp:39-41,166-167`, `hidpp/Report.cpp:268-293` |
| 4 | Root-triggered config write over D-Bus, no D-Bus policy shipped | High | `src/logid/Configuration.cpp:57-76`, `src/logid/DeviceManager.cpp:41` |
| 5 | `printf`-style logger with runtime format string | Medium | `src/logid/logid.cpp:96`, `src/logid/util/log.h:24` |
| 6 | Fragile IOMonitor lock-yield threading | Medium | `src/logid/backend/raw/IOMonitor.cpp:77-167` |
| 7 | uinput device destroyed/recreated per new event code | Medium | `src/logid/InputDevice.cpp:162-177` |
| 8 | Release-mode `assert` on read length | Medium | `src/logid/backend/raw/RawDevice.cpp:227-244` |
| 9 | `EPIPE` write retries silently exhaust | Low | `src/logid/backend/raw/RawDevice.cpp:215-221` |
| 10 | Vendored, incomplete `ipcgull` D-Bus library | Low | `src/ipcgull/src/server_gdbus.cpp` |
| 11 | Numerous open TODOs in HID++ 1.0 receiver/pairing code | Low | `src/logid/backend/hidpp10/Receiver.cpp:42,74,110` |
| 12 | Effectively zero automated tests for the daemon | Medium | `src/logid/` (see `TESTING.md`) |

## Security

### Runs as root with no hardening (High)

The shipped systemd unit runs the daemon as root: `src/logid/logid.service.in:10` (`User=root`). There is no privilege drop, no Linux capabilities bounding set, and no systemd sandboxing directives (`ProtectSystem`, `NoNewPrivileges`, `RestrictAddressFamilies`, etc.) anywhere in `src/logid/logid.cpp:132-183`. A bug reachable from device input or D-Bus runs with full root.

### Unbounded indexing of attacker-controlled HID reports (High)

Incoming HID++ reports are indexed before any length validation:
- Raw filter lambdas index `report[Offset::Type / DeviceIndex / SubID]` on the un-padded incoming vector at `src/logid/backend/hidpp/Device.cpp:121-126` and `src/logid/backend/hidpp10/ReceiverMonitor.cpp:39-41,166-167`.
- `Report::isError10` / `isError20` read `_data[3..5]` directly at `src/logid/backend/hidpp/Report.cpp:268-293`.

A malicious or malfunctioning USB/Bluetooth device can deliver a short report and trigger an out-of-bounds read. Input from hardware should be treated as untrusted and length-checked before indexing.

### Root-triggered config write over D-Bus (High)

`Configuration::save()` writes `/etc/logid.cfg` as root at `src/logid/Configuration.cpp:57-76`, and this is exposed over D-Bus via `src/logid/DeviceManager.cpp:41`. No D-Bus policy file ships in-tree to restrict who may invoke it, so the access-control surface depends entirely on the deploying distro's default policy.

### Format-string logging (Medium)

`src/logid/logid.cpp:96` calls `logPrintf(WARN, e.what())` — a runtime-controlled string passed as a `printf`-style format. The logger itself is flagged for replacement in `src/logid/util/log.h:24`.

## Correctness Bugs

### Sliced exception (High)

`src/logid/util/ExceptionHandler.cpp:26-38` re-throws with `throw error;` where `error` is caught as a base reference. This **slices** the exception to `std::exception`, so the subsequent type-specific `catch` clauses are dead code and never match. The fix is a bare `throw;` to rethrow the active exception preserving its dynamic type.

### Release-mode assert on read length (Medium)

`src/logid/backend/raw/RawDevice.cpp:227-244` uses `assert` to validate a read length. With `NDEBUG` (release builds) the assert compiles out, removing the check entirely. Validation that must hold in production should not rely on `assert`.

### Silent EPIPE retry exhaustion (Low)

`src/logid/backend/raw/RawDevice.cpp:215-221` retries on `EPIPE` but silently exhausts its retries without surfacing a clear error.

## Fragile / Hard-to-Maintain Areas

- **IOMonitor lock-yield threading** — `src/logid/backend/raw/IOMonitor.cpp:77-167` uses a lock-then-yield pattern that is delicate and easy to break under concurrent device events.
- **uinput churn** — `src/logid/InputDevice.cpp:162-177` destroys and recreates the uinput device whenever a new event code is needed, which is heavy and can drop events.
- **Vendored `ipcgull`** — `src/ipcgull/src/server_gdbus.cpp` (~1099 lines) is an in-tree, incomplete D-Bus binding library with many TODOs; it is core to the IPC surface but only lightly exercised.

## Technical Debt / TODOs

- HID++ 1.0 receiver and pairing logic carries numerous open TODOs: `src/logid/backend/hidpp10/Receiver.cpp:42,74,110`.
- Logger replacement is explicitly noted as pending: `src/logid/util/log.h:24`.

## Testing Gap (Medium)

There is effectively **no automated test suite** for the `src/logid/` daemon. The only test target in the tree is `src/ipcgull/tests/server_test/main.cpp` (a manual IPC demo). Verification relies on multi-distro compile CI and manual hardware testing tracked in `TESTED.md`. See `TESTING.md` for detail.

---
*Mapped: 2026-05-30*
