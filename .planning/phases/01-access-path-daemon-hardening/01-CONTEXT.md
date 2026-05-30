# Phase 1: Access Path & Daemon Hardening - Context

**Gathered:** 2026-05-30
**Status:** Ready for planning

<domain>
## Phase Boundary

A non-root user can safely reach the `logid` D-Bus service (`pizza.pixl.LogiOps` on the system bus) without widening the root attack surface, and privileged config writes are authorization-gated. This phase delivers the access/security foundation that gates every subsequent GUI phase: relaxed D-Bus policy, polkit-gated persistence, systemd sandboxing, and HID input hardening. No GUI is built here — only a non-root smoke test proving the access path works.

In scope: D-Bus system-bus policy change; polkit action + daemon-side authorization check on `save()`; systemd unit hardening; HID report length-checks; selected CONCERNS fixes. Out of scope: any Qt/QML code, device-list UI, feature configuration.
</domain>

<decisions>
## Implementation Decisions

### D-Bus Access Model
- Relax the system-bus policy (`src/logid/logiops-dbus.conf.in`) to grant send/receive to a dedicated `logiops` group; keep `own=root` (daemon still owns the name as root).
- Group name: `logiops`.
- Packaging adds the installing user to the `logiops` group and documents manual `usermod -aG logiops <user>`; group membership creation handled in packaging (Phase 9 enforces, but the policy + group are introduced here).
- Read/live-control is free to the group; only privileged config writes (`save()`) are polkit-gated.

### Privileged Write / polkit
- Gate exactly `Configuration::save()` (the `/etc/logid.cfg` write), not every live mutation.
- polkit auth level: `auth_admin_keep` (admin authentication, cached for the session).
- Enforcement: the D-Bus `save`/config-write handler queries polkit (checks the authorization for the calling subject) before writing.
- Fail-safe: if polkit is unavailable, deny the write and log — never write unauthorized.

### Daemon Hardening (systemd)
- Add to `src/logid/logid.service.in`: `NoNewPrivileges=yes`, `ProtectSystem=strict`, `ProtectHome=yes`, `PrivateTmp=yes`, `RestrictAddressFamilies=AF_UNIX AF_NETLINK`, `ReadWritePaths=/etc/logid.cfg`, and a minimal `CapabilityBoundingSet`.
- Keep `User=root` (needs hidraw/uinput access) but drop capabilities to the minimum required for hidraw/uinput/udev. Dedicated-user + udev-rules approach is deferred (bigger change).
- Verify hardening is in effect via `systemctl show` / `systemd-analyze security`.

### HID Input Hardening (CONCERNS fixes)
- Explicit bounds/length checks before indexing attacker-controlled report fields in `src/logid/backend/hidpp/Device.cpp` (filter lambdas), `src/logid/backend/hidpp10/ReceiverMonitor.cpp`, and `Report::isError10`/`isError20` (`src/logid/backend/hidpp/Report.cpp`). No `assert` for security-relevant checks (compiled out in release).
- Fix CONCERNS #1: sliced exception in `src/logid/util/ExceptionHandler.cpp` (`throw error;` → bare `throw;`).
- Fix CONCERNS #8: release-mode `assert` on read length in `src/logid/backend/raw/RawDevice.cpp` → explicit check.
- Fix CONCERNS #5: format-string logging at `src/logid/logid.cpp:96` (`logPrintf(WARN, e.what())` → use a `"%s"` format).

### Claude's Discretion
- Exact systemd capability set, precise polkit action id/wording, and the exact bounds-check helper shape are at Claude's discretion, guided by the research RESEARCH flag (re-verify live D-Bus `group=` policy syntax and current polkit `auth_admin_keep` conventions before implementing).
</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- D-Bus policy template: `src/logid/logiops-dbus.conf.in` (installed as `/usr/share/dbus-1/system.d/pizza.pixl.LogiOps.conf`) — currently grants own/send/receive to `user="root"` only.
- systemd unit template: `src/logid/logid.service.in` (`User=root`, no hardening).
- IPC identity constants: `src/logid/ipc_defs.h` (`SERVICE_ROOT_NAME=pizza.pixl.LogiOps`, root node `/pizza/pixl/logiops`).
- Config persistence: `Configuration::save()` in `src/logid/Configuration.cpp:57-76`, exposed over D-Bus via `src/logid/DeviceManager.cpp:41`.
- ipcgull D-Bus server (GDBus/glib backend): `src/ipcgull/` — where a polkit check on the save method would hook in.

### Established Patterns
- C++20, CMake, `make()` factories, exception-based error handling routed through `util/ExceptionHandler`, custom `logPrintf` logger (`util/log.h`). Match these (see `.planning/codebase/CONVENTIONS.md`).
- Bus selection is compile-time (`USE_USER_BUS`); default system bus (`src/logid/logid.cpp:154`). Keep system bus as the default path.

### Integration Points
- The relaxed D-Bus policy + polkit action are the seam every later GUI phase depends on.
- Security baseline (CONCERNS #2/#3) is referenced in `.planning/codebase/CONCERNS.md` — hardening must land before the first daemon feature extension (Phase 4).
</code_context>

<specifics>
## Specific Ideas

- The non-root smoke test (success criterion 1) should call a real method on `pizza.pixl.LogiOps` as a `logiops`-group user and change a device setting, proving the access path end-to-end.
- Denying the polkit prompt must leave `/etc/logid.cfg` byte-unchanged (success criterion 2).
- Research flag (carry into plan-phase): re-verify exact D-Bus policy `group=` syntax and current polkit defaults (`auth_admin_keep`) against live `dbus-daemon`/`polkit` docs before hardening — web tools were unavailable during project research.
</specifics>

<deferred>
## Deferred Ideas

- Dedicated non-root daemon user + udev rules for hidraw/uinput access (instead of root + dropped caps) — larger refactor, revisit if the root+caps approach proves insufficient.
- Verifying `Configuration::save()` round-trip fidelity (libconfig comment preservation) — flagged for Phase 3 where persistence UI lands.
</deferred>
