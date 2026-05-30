# Phase 1: Access Path & Daemon Hardening - Research

**Researched:** 2026-05-30
**Domain:** Linux D-Bus system-bus access control, polkit authorization, systemd sandboxing, C++ HID input hardening
**Confidence:** HIGH for in-tree code facts (every file read this session); MEDIUM for external syntax (D-Bus `group=` policy, polkit `.policy` XML, libpolkit-gobject API, systemd capability names) — web verification unavailable this session, flagged per item below.

> **Web tools were unavailable.** Every external-syntax claim (D-Bus policy attributes, polkit XML, `polkit_authority_*` C API, systemd directive names) is tagged `[ASSUMED]` from training knowledge (cutoff 2026-01) and listed in the Assumptions Log for the planner/implementer to double-check against live `man dbus-daemon`, `man polkit`, `polkit-gobject-1` headers, and `man systemd.exec` before locking.

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**D-Bus Access Model**
- Relax the system-bus policy (`src/logid/logiops-dbus.conf.in`) to grant send/receive to a dedicated `logiops` group; keep `own=root`.
- Group name: `logiops`.
- Packaging adds the installing user to the `logiops` group and documents manual `usermod -aG logiops <user>`; group creation handled in packaging (Phase 9 enforces; the policy + group are introduced here).
- Read/live-control is free to the group; only privileged config writes (`save()`) are polkit-gated.

**Privileged Write / polkit**
- Gate exactly `Configuration::save()` (the `/etc/logid.cfg` write), not every live mutation.
- polkit auth level: `auth_admin_keep` (admin auth, cached for the session).
- Enforcement: the D-Bus save/config-write handler queries polkit (checks authorization for the calling subject) before writing.
- Fail-safe: if polkit is unavailable, **deny** the write and log — never write unauthorized.

**Daemon Hardening (systemd)**
- Add to `src/logid/logid.service.in`: `NoNewPrivileges=yes`, `ProtectSystem=strict`, `ProtectHome=yes`, `PrivateTmp=yes`, `RestrictAddressFamilies=AF_UNIX AF_NETLINK`, `ReadWritePaths=/etc/logid.cfg`, and a minimal `CapabilityBoundingSet`.
- Keep `User=root` (needs hidraw/uinput) but drop capabilities to the minimum for hidraw/uinput/udev. Dedicated-user + udev-rules approach is **deferred**.
- Verify via `systemctl show` / `systemd-analyze security`.

**HID Input Hardening (CONCERNS fixes)**
- Explicit bounds/length checks before indexing attacker-controlled report fields in `backend/hidpp/Device.cpp` (filter lambdas), `backend/hidpp10/ReceiverMonitor.cpp`, and `Report::isError10`/`isError20` (`backend/hidpp/Report.cpp`). No `assert` for security-relevant checks.
- Fix CONCERNS #1: sliced exception in `util/ExceptionHandler.cpp` (`throw error;` → bare `throw;`).
- Fix CONCERNS #8: release-mode `assert` on read length in `backend/raw/RawDevice.cpp` → explicit check.
- Fix CONCERNS #5: format-string logging at `logid.cpp:96` (`logPrintf(WARN, e.what())` → `"%s"` format).

### Claude's Discretion
- Exact systemd capability set, precise polkit action id/wording, and exact bounds-check helper shape — guided by re-verifying live D-Bus `group=` policy syntax and current polkit `auth_admin_keep` conventions before implementing.

### Deferred Ideas (OUT OF SCOPE)
- Dedicated non-root daemon user + udev rules for hidraw/uinput access (instead of root + dropped caps) — larger refactor.
- Verifying `Configuration::save()` round-trip fidelity (libconfig comment preservation) — deferred to Phase 3.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| ACCESS-01 | Non-root user (in `logiops` group) can connect to the `logid` D-Bus service and control devices | D-Bus policy relaxation (Section "D-Bus Policy Relaxation"); current policy at `src/logid/logiops-dbus.conf.in` grants `user="root"` only — replace with `group="logiops"` `<allow>` block. No daemon code change needed for live control. |
| ACCESS-02 | Privileged config persistence (`Configuration::save()` → `/etc/logid.cfg`) is gated behind polkit | Section "polkit Integration". **Crux:** ipcgull `function::operator()(args)` does NOT thread the D-Bus caller identity to the handler — a minimal ipcgull change is required (documented). polkit `.policy` action + `libpolkit-gobject-1` check + fail-safe-deny. |
| ACCESS-03 | Daemon sandboxed with systemd hardening so widening D-Bus access does not widen the root attack surface | Section "systemd Hardening" — exact directives + minimal `CapabilityBoundingSet`; flags directives that can break hidraw/uinput/udev (`DeviceAllow`, `ProtectSystem`, `PrivateDevices`). |
| ACCESS-04 | Incoming HID report fields are length-checked before indexing | Section "HID Length-Checks" — exact current indexing lines (with offsets `Type=0`, `DeviceIndex=1`, `SubID=2`), `min` lengths, and the guard pattern. |
</phase_requirements>

---

## Summary

This is a **security-hardening + access-control phase with zero GUI work**. Five independent workstreams: (1) one-line-ish D-Bus policy file change to open the bus to a `logiops` group; (2) polkit authorization on exactly `Configuration::save()`; (3) systemd unit sandboxing; (4) HID report length-checks; (5) three CONCERNS bug fixes (#1 sliced exception, #5 format-string, #8 release assert).

Four of the five are mechanical and low-risk. **The polkit integration is the only architecturally hard piece**, and the difficulty is precise: the daemon exposes `Save` over D-Bus through the vendored `ipcgull` library, whose method-dispatch abstraction (`ipcgull::function::operator()(const variant_tuple& args)`) collapses every call to **arguments only — the D-Bus sender / caller bus name is captured at the GDBus callback (`gdbus_method_call`, `server_gdbus.cpp:303`, currently `[[maybe_unused]] const gchar* sender`) but is never propagated to the C++ handler.** polkit needs that caller identity to build a `PolkitSubject`. So ACCESS-02 cannot be done purely in `Configuration.cpp` — it requires a minimal change to ipcgull to thread the caller's unique bus name through to the `save` handler (or a narrower workaround, both documented below).

**Primary recommendation:** Do the four mechanical workstreams first (independent, parallelizable, each independently verifiable). For polkit, thread the GDBus `sender` string from `gdbus_method_call` into the `Configuration::IPC` save handler via the smallest possible ipcgull extension, then call `polkit_authority_check_authorization_sync()` against a `polkit_system_bus_name_new(sender)` subject, fail-safe-deny on any error/null authority. Link `polkit-gobject-1` via `pkg_check_modules`.

---

## Standard Stack

### Core (all already in-tree or system libs)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| GLib/GIO (`glib-2.0`, `gio-2.0`) | system | GDBus backend under ipcgull | Already the IPC backend (`server_gdbus.cpp`) `[VERIFIED: src/ipcgull]` |
| libconfig++ (`config++`) | system | `/etc/logid.cfg` read/write | Already linked `[VERIFIED: src/logid/CMakeLists.txt:89]` |
| ipcgull (vendored, static) | submodule | D-Bus object/interface layer | In-tree, will be **modified** this phase `[VERIFIED]` |

### Supporting (NEW dependency this phase)
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `polkit-gobject-1` (libpolkit-gobject-1) | system (Debian: `libpolkit-gobject-1-dev`) | Daemon-side authorization check for `save()` | ACCESS-02 only |

**Installation (build deps to add for ACCESS-02):**
```bash
# Debian/Ubuntu
sudo apt install libpolkit-gobject-1-dev
# Fedora
sudo dnf install polkit-devel
# Arch
sudo pacman -S polkit
```

**Version note:** `polkit-gobject-1` pkg-config module is the stable GObject binding shipped with polkit for years; the `polkit_authority_check_authorization_sync` API has been stable since polkit 0.101-ish. `[ASSUMED — verify the exact pkg-config module name `polkit-gobject-1` and that `check_authorization_sync` exists in the installed polkit on target distros]`. **Note: on THIS research machine `pkg-config --exists polkit-gobject-1` returned not-found — the dev package is not installed here, so the planner must add an install step and CI must install it.** `[VERIFIED: pkg-config probe this session]`

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `polkit-gobject-1` C API in-process | Shell out to `pkcheck(1)` | Simpler to call, but adds a process spawn under a sandboxed (NoNewPrivileges) root daemon, harder to pass the subject correctly, and worse fail-safe semantics. In-process GObject API is the standard for a long-running daemon. |
| Thread caller into ipcgull | polkit `.rules` keyed on the action only | polkit rules can't see *which* D-Bus method without the daemon checking; you still need the daemon-side `check_authorization` call. No way around threading the caller. |

---

## D-Bus Policy Relaxation (ACCESS-01)

**File:** `src/logid/logiops-dbus.conf.in` (installed to `/usr/share/dbus-1/system.d/pizza.pixl.LogiOps.conf` via `src/logid/CMakeLists.txt:113-120`). It is a plain copy via `configure_file` (no `@VAR@` substitution currently used in the file). `[VERIFIED]`

**Current content (full file):** `[VERIFIED: read this session]`
```xml
<!DOCTYPE busconfig PUBLIC
 "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <policy context="default">
    <deny receive_sender="pizza.pixl.LogiOps"/>
  </policy>
  <policy user="root">
    <allow own="pizza.pixl.LogiOps"/>
    <allow send_destination="pizza.pixl.LogiOps"/>
    <allow receive_sender="pizza.pixl.LogiOps"/>
  </policy>
</busconfig>
```

**Target content:** keep `own` root-only, add a `group="logiops"` policy granting `send_destination` + `receive_sender`:
```xml
<!DOCTYPE busconfig PUBLIC
 "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <policy context="default">
    <deny receive_sender="pizza.pixl.LogiOps"/>
  </policy>

  <!-- Daemon owns the name as root -->
  <policy user="root">
    <allow own="pizza.pixl.LogiOps"/>
    <allow send_destination="pizza.pixl.LogiOps"/>
    <allow receive_sender="pizza.pixl.LogiOps"/>
  </policy>

  <!-- Non-root members of the logiops group may call and receive signals,
       but may NOT own the name -->
  <policy group="logiops">
    <allow send_destination="pizza.pixl.LogiOps"/>
    <allow receive_sender="pizza.pixl.LogiOps"/>
  </policy>
</busconfig>
```

**Key syntax facts** `[ASSUMED — verify against `man dbus-daemon` section "Configuration File" / system.d policy semantics]`:
- `<policy group="NAME">` is a valid policy selector alongside `user=` / `context=`. Rules inside apply to processes whose **effective gid OR supplementary groups** include `NAME`.
- `send_destination` / `receive_sender` take the **bus name**, not an object path.
- `own` is deliberately omitted from the group policy so only root can claim the name.
- The `<deny receive_sender>` in the default policy is **overridden** by the later explicit `<allow>` (D-Bus policy is last-match-wins within applicable groups; group/user policies are higher priority than `context="default"`). `[ASSUMED — confirm precedence ordering; place the group `<allow>` after the default deny as shown]`.

**Gotcha:** D-Bus reloads `system.d` policy automatically on file change in modern dbus-daemon, but a `systemctl reload dbus` (or reboot) is the reliable way to pick it up in a test. The newer location `/usr/share/dbus-1/system.d/` is correct for vendor-shipped policy (vs `/etc/dbus-1/system.d/` for admin overrides). `[VERIFIED: install dir in CMakeLists.txt; ASSUMED: auto-reload behavior]`

**Group must exist** before a user can be added. Group creation is packaging's job (Phase 9), but for this phase's smoke test the tester must `groupadd logiops && usermod -aG logiops <user>` and re-login (group membership is established at session start). Document this in the plan's setup steps.

---

## polkit Integration (ACCESS-02) — the hard part

### The crux: ipcgull does not expose the caller to the handler

Call path for `Save`, traced in-tree:
1. `Configuration::IPC` registers the method: `{"Save", {config, &Configuration::save}}` — `src/logid/Configuration.cpp:73-76`. `Configuration::save()` takes **no arguments** and returns `void`. `[VERIFIED]`
2. ipcgull wraps the member fn as `ipcgull::function`, ultimately a `std::function<variant_tuple(const variant_tuple&)>` — see `function.h:96-121` (`_fn_generator<void>`) and `function.cpp:24-26`. **The wrapper signature is args-in/args-out only; there is no caller parameter.** `[VERIFIED]`
3. At dispatch, `gdbus_method_call(...)` receives `const gchar* sender` (the caller's unique bus name, e.g. `:1.42`) but it is declared `[[maybe_unused]]` and the handler is invoked as `f_it->second(args)` with no sender — `server_gdbus.cpp:301-354`. `[VERIFIED]`

**Conclusion:** ACCESS-02 cannot be implemented purely inside `Configuration.cpp`. The `sender` string lives only in `gdbus_method_call` and must be threaded to the save handler. This is the single load-bearing finding of this research.

### Recommended approach: thread the caller bus name through ipcgull (minimal change)

The least invasive design that keeps ipcgull generic:

**Option A (recommended) — a per-call "caller context" the handler can read.**
Add to ipcgull an optional way to learn the current caller. Smallest viable shape:
- In `server_gdbus.cpp::gdbus_method_call`, before invoking `f_it->second(args)`, store `sender` into a thread-local (or into the `internal` struct guarded by the existing `server_lock`, since dispatch already holds `std::lock_guard<std::recursive_mutex> lock(i->server_lock)` at `server_gdbus.cpp:312`). Expose a public accessor like `ipcgull::current_caller()` returning the bus name string, valid only during a method call.
- `Configuration::save()` (or a thin wrapper) calls `ipcgull::current_caller()` to get the sender, then runs the polkit check.

Rationale: dispatch is already serialized by `server_lock` (recursive_mutex), so a single "current caller" slot set on entry and cleared on exit is safe against concurrent dispatch on the GLib main loop. `[VERIFIED: lock at server_gdbus.cpp:312; ASSUMED: GDBus dispatches method calls on a single main-loop thread — confirm there is one GMainContext thread]`

**Option B — change the `Save` method to be polkit-aware via a special argument type.**
ipcgull would need a recognized "caller" pseudo-arg injected by the dispatcher. This is a larger, more invasive `function.h` template change (every `_fn_generator` specialization). **Not recommended** — Option A is far smaller.

**Option C (fallback, no ipcgull change) — gate at a different layer.**
Have the GUI (later phases) call `Save` only after itself doing a polkit check via `pkexec`/an agent, and keep the daemon trusting the group. **Rejected:** violates the locked "enforcement: the D-Bus handler queries polkit" decision and the fail-safe-deny requirement — a non-GUI group member could call `Save` directly. Document as rejected.

**Plan guidance:** implement Option A. The ipcgull change is ~10-20 lines across `server_gdbus.cpp` + one new public header declaration + one `.cpp` definition. Treat it as its own task with its own verification (a method handler can read a non-empty caller string).

### The polkit C API call (libpolkit-gobject-1)

Inside the gated save path, after obtaining `sender`: `[ASSUMED — verify all symbol names/signatures against `polkit/polkit.h` headers on target; these are from training knowledge]`

```cpp
// Source: training knowledge of libpolkit-gobject-1 — VERIFY against installed headers.
#include <polkit/polkit.h>

static bool check_save_authorized(const std::string& caller_bus_name) {
    GError* error = nullptr;

    // 1. Get the system authority (blocking).
    PolkitAuthority* authority =
        polkit_authority_get_sync(nullptr /*GCancellable*/, &error);
    if (!authority) {
        // FAIL-SAFE: polkit unavailable -> deny.
        logPrintf(WARN, "polkit authority unavailable, denying save: %s",
                  error ? error->message : "unknown");
        if (error) g_error_free(error);
        return false;
    }

    // 2. Build the subject from the D-Bus caller's unique bus name.
    PolkitSubject* subject =
        polkit_system_bus_name_new(caller_bus_name.c_str());

    // 3. Check the authorization for our action, allowing interactive auth.
    PolkitAuthorizationResult* result =
        polkit_authority_check_authorization_sync(
            authority, subject,
            "pizza.pixl.logiops.save-config",        // action id (see .policy)
            nullptr /*PolkitDetails*/,
            POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
            nullptr /*GCancellable*/, &error);

    bool authorized = false;
    if (result) {
        authorized = polkit_authorization_result_get_is_authorized(result);
        g_object_unref(result);
    } else {
        // FAIL-SAFE on error.
        logPrintf(WARN, "polkit check failed, denying save: %s",
                  error ? error->message : "unknown");
        if (error) g_error_free(error);
    }

    g_object_unref(subject);
    g_object_unref(authority);
    return authorized;   // default-deny on any path that didn't set true
}
```

Then in the save flow:
```cpp
void Configuration::save() {
    const std::string caller = ipcgull::current_caller();   // Option A
    if (caller.empty() || !check_save_authorized(caller)) {
        logPrintf(WARN, "Unauthorized save() denied.");
        throw std::runtime_error("Not authorized to save configuration");
        // throwing surfaces as a D-Bus error to the caller (server_gdbus.cpp:381-386)
    }
    // ... existing write logic (Configuration.cpp:58-71) ...
}
```

**Critical correctness notes:**
- **Fail-safe-deny is the default of every branch** — null authority, null result, exception → return false / throw. This satisfies the locked "if polkit is unavailable, deny."
- `POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION` is what makes `auth_admin_keep` actually prompt; without it polkit returns "not authorized but challenge possible" and no prompt appears. `[ASSUMED — verify flag name]`
- The `_sync` call **blocks**. It runs on the GLib main-loop thread during dispatch. A blocking authorization prompt on that thread is acceptable for a rare save action but flag it: if it deadlocks the bus, the alternative is the async API + deferring the D-Bus reply. **Risk — see Risks section.** `[ASSUMED]`
- `polkit_system_bus_name_new(sender)` is the correct subject constructor for a D-Bus caller identified by unique name. (`PolkitUnixProcess` is an alternative but requires resolving the caller PID, which is more work and racy.) `[ASSUMED — verify `polkit_system_bus_name_new` symbol]`

### The polkit `.policy` action file (NEW)

Create `src/logid/logiops-policy.policy.in` (or `pizza.pixl.logiops.policy.in`), installed to `/usr/share/polkit-1/actions/`. `[ASSUMED — verify the install dir `/usr/share/polkit-1/actions/` and `.policy` XML schema against `man polkit` / polkit DTD]`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE policyconfig PUBLIC
 "-//freedesktop//DTD PolicyKit Policy Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/PolicyKit/1/policyconfig.dtd">
<policyconfig>
  <vendor>LogiOps</vendor>
  <vendor_url>https://github.com/c4duZ/logiopsv2</vendor_url>

  <action id="pizza.pixl.logiops.save-config">
    <description>Save Logitech device configuration</description>
    <message>Authentication is required to save the Logitech configuration to /etc/logid.cfg</message>
    <defaults>
      <allow_any>auth_admin_keep</allow_any>
      <allow_inactive>auth_admin_keep</allow_inactive>
      <allow_active>auth_admin_keep</allow_active>
    </defaults>
  </action>
</policyconfig>
```

- The `<action id>` MUST exactly match the string passed to `check_authorization_sync` (`pizza.pixl.logiops.save-config`).
- `auth_admin_keep` = require admin authentication, cache the grant for ~5 minutes for the session, per the locked decision. `[ASSUMED — verify `auth_admin_keep` is the current spelling; historically valid values: `no`, `yes`, `auth_self`, `auth_admin`, `auth_self_keep`, `auth_admin_keep`]`
- Using all three `<allow_*>` set to `auth_admin_keep` is conservative (prompts even for an active local session). If you want active local users to auth as themselves rather than admin, that's a discretion call — but the locked decision says `auth_admin_keep`, so keep admin.

### CMake linkage

In `src/logid/CMakeLists.txt`, alongside the existing `pkg_check_modules` block (lines 74-77): `[VERIFIED: existing pattern]`
```cmake
pkg_check_modules(POLKIT REQUIRED polkit-gobject-1)
# ...
include_directories(... ${POLKIT_INCLUDE_DIRS})
target_link_libraries(logid ... ${POLKIT_LIBRARIES})
```
And install the policy file (mirror the dbus-conf install block at lines 113-120):
```cmake
set(POLKIT_ACTION_INSTALL_DIR "/usr/share/polkit-1/actions")
configure_file(logiops-policy.policy.in
        ${CMAKE_BINARY_DIR}/pizza.pixl.logiops.policy)
install(FILES ${CMAKE_BINARY_DIR}/pizza.pixl.logiops.policy
        DESTINATION ${POLKIT_ACTION_INSTALL_DIR} COMPONENT cp)
```
**ipcgull also links GIO already** (via the GDBus backend), so `g_object_unref`/GError are available without new linkage in the daemon; but `polkit-gobject-1` pulls glib/gobject transitively anyway. `[VERIFIED: ipcgull uses gio-2.0]`

---

## systemd Hardening (ACCESS-03)

**File:** `src/logid/logid.service.in` (configured to `logid.service`, `src/logid/CMakeLists.txt:104`). Current `[Service]` is bare: `Type=simple`, `ExecStart=...`, `User=root`. `[VERIFIED]`

**Target `[Service]` block** (additions; keep `Type=simple`, `ExecStart`, `User=root`):
```ini
[Service]
Type=simple
ExecStart=${CMAKE_INSTALL_PREFIX}/bin/logid
User=root

# --- Hardening (Phase 1) ---
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
ReadWritePaths=/etc/logid.cfg
RestrictAddressFamilies=AF_UNIX AF_NETLINK
CapabilityBoundingSet=
ProtectControlGroups=yes
ProtectKernelTunables=yes
ProtectKernelLogs=yes
ProtectKernelModules=yes
RestrictRealtime=yes
LockPersonality=yes
MemoryDenyWriteExecute=yes
```

### Capability analysis — the minimal `CapabilityBoundingSet`

What the daemon actually does at the OS level (from `INTEGRATIONS.md` + code): opens `/dev/hidraw*` and `/dev/uinput`, issues HID ioctls, creates a uinput virtual device, opens a udev **netlink** monitor, owns a D-Bus **system bus** name. `[VERIFIED]`

- Access to `/dev/hidraw*` and `/dev/uinput` is governed by **file permissions**, not capabilities — when running as **root (uid 0)** the daemon already passes DAC checks, so it needs **no capability** for device access. `[ASSUMED — verify uinput/hidraw need no CAP_* for a uid-0 process; this is the standard understanding]`
- udev netlink monitor (`AF_NETLINK`) for uid 0 needs no special cap. `[ASSUMED]`
- Therefore the **minimal set is empty**: `CapabilityBoundingSet=` (drop ALL). This is the strongest and is the recommended target. If hidraw/uinput break in testing, the most likely needed addition is **none** (because root bypasses DAC); but if a future dedicated-user move happens (deferred), `CAP_DAC_OVERRIDE` or device ACLs would matter. **Verify empty set works on real hardware before locking.** `[ASSUMED — empty is the goal; fall back to listing only what testing proves necessary]`

### Directives that can BREAK hidraw/uinput/udev — DO NOT add blindly

| Directive | Effect | Verdict |
|-----------|--------|---------|
| `PrivateDevices=yes` | Gives the service a private `/dev` with **only** a minimal set — **hides `/dev/hidraw*` and `/dev/uinput`** | **DO NOT USE** — breaks the daemon's whole purpose `[ASSUMED — strongly believed]` |
| `DeviceAllow=` (with `DevicePolicy=closed`) | Whitelists specific device nodes | Optional tightening, but `/dev/hidraw*` nodes are dynamic/numerous; safer to leave default `DevicePolicy=auto`. If used, must allow `char-hidraw` and `/dev/uinput`. **Risk of breakage — leave out for v1.** |
| `ProtectSystem=strict` | Mounts entire FS read-only **including `/etc`** | **Safe ONLY because** `ReadWritePaths=/etc/logid.cfg` re-opens the one needed path. Without that `ReadWritePaths`, `save()` would fail. Pairing is mandatory. `[VERIFIED reasoning; ASSUMED exact semantics]` |
| `ProtectHome=yes` | Hides `/home`, `/root`, `/run/user` | Safe — daemon doesn't read homes. |
| `RestrictAddressFamilies=AF_UNIX AF_NETLINK` | D-Bus uses `AF_UNIX`; udev monitor uses `AF_NETLINK` | Both required and both listed — correct. Omitting `AF_NETLINK` would **break udev hotplug**. `[VERIFIED: udev netlink in INTEGRATIONS.md]` |
| `IPAddressDeny=any` | No IP networking | Could add (daemon has no network) but not required by the locked set. |
| `SystemCallFilter=@system-service` | seccomp allowlist | Powerful but risky with ioctls; **defer** — not in the locked set. |

**`ReadWritePaths=/etc/logid.cfg`** must point at the actual configured path. Default is `/etc/logid.cfg` (`logid.cpp` `default_config`), but the daemon accepts `-c <path>`; the unit's `ExecStart` uses no `-c`, so default holds. If a packager overrides the path, this directive must track it. `[VERIFIED: default path]`

**Note:** `ProtectSystem=strict` makes `/usr` read-only too — fine, the binary only executes. `PrivateTmp=yes` is harmless (daemon uses no tmp). `MemoryDenyWriteExecute` / `LockPersonality` are belt-and-suspenders; drop them if any JIT/observed crash appears (none expected for this C++ daemon).

---

## HID Length-Checks (ACCESS-04)

Reports arrive as `const std::vector<uint8_t>& report` from `RawDevice::_readReports()` (`RawDevice.cpp:228-244`), where `report` is built from exactly the `read()` length (`std::vector<uint8_t> report(buf, buf + len)`) — so **a short USB/BT report yields a short vector**, and the **filter lambdas index it before any length check**. Offsets are: `Type=0`, `DeviceIndex=1`, `SubID=2`, `Feature=2`, `Address=3`, `Function=3`, `Parameters=4` (`Report.h:35-43`). `[VERIFIED]`

### Site 1 — `backend/hidpp/Device.cpp:121-126` (filter lambda)
Current `[VERIFIED]`:
```cpp
_raw_handler = _raw_device->addEventHandler(
        {[index = _index](const std::vector<uint8_t>& report) -> bool {
            return (report[Offset::Type] == Report::Type::Short ||
                    report[Offset::Type] == Report::Type::Long) &&
                   (report[Offset::DeviceIndex] == index);
        },
         ...});
```
Guard: indexes up to `Offset::DeviceIndex` (=1), so needs `size() > 1` i.e. `>= 2`. Safer: require the full header.
```cpp
[index = _index](const std::vector<uint8_t>& report) -> bool {
    if (report.size() < hidpp::Report::HeaderLength)   // HeaderLength = 4
        return false;
    return (report[Offset::Type] == Report::Type::Short ||
            report[Offset::Type] == Report::Type::Long) &&
           (report[Offset::DeviceIndex] == index);
}
```

### Site 2 — `backend/hidpp10/ReceiverMonitor.cpp:39-45` (connect filter)
Current indexes `Offset::Type` (0) then `Offset::SubID` (2). Add `if (report.size() < hidpp::Report::HeaderLength) return false;` at the top of the lambda (`ReceiverMonitor.cpp:39`). `[VERIFIED current code]`

### Site 3 — `backend/hidpp10/ReceiverMonitor.cpp:165-170` (`waitForDevice` filter)
Current indexes `Offset::SubID` (2) and `Offset::DeviceIndex` (1). Same guard at lambda top (`ReceiverMonitor.cpp:165`). `[VERIFIED]`

### Site 4 — `backend/hidpp/Report.cpp:269-294` (`isError10`/`isError20`)
**Already safe by construction** because the `Report(const std::vector<uint8_t>&)` ctor (`Report.cpp:160-175`) **resizes `_data` to `HeaderLength + LongParamLength` (=20) first**, then truncates to short/long — so `_data[3..5]` are always in-bounds by the time `isError10/20` run. `[VERIFIED]` **However**, the CONTEXT explicitly lists these, and defense-in-depth is cheap: add an early `if (_data.size() <= Offset::Parameters + 1) return false;` (i.e. need indices 3,4,5 ⇒ `size() >= 6`) guarding the `_data[3]/_data[4]/_data[5]` reads. Document that the ctor already pads, so this is belt-and-suspenders, NOT the primary fix. The primary OOB surface is Sites 1-3 (raw vectors, no padding).

### Recommended shared helper (Claude's discretion on shape)
A tiny inline in `hidpp` (e.g. in `Report.h` or a new `report_bounds.h`):
```cpp
namespace logid::backend::hidpp {
    // Returns true if `report` is long enough to safely read the HID++ header.
    inline bool hasHidppHeader(const std::vector<uint8_t>& report) {
        return report.size() >= Report::HeaderLength;   // 4 bytes: Type,Index,SubID/Feature,Addr/Func
    }
}
```
Use `if (!hasHidppHeader(report)) return false;` at the top of each filter lambda. Keeps the guard consistent and self-documenting. **No `assert`** anywhere (locked).

---

## CONCERNS Fixes (bundled into this phase)

### #1 — Sliced exception (`util/ExceptionHandler.cpp:27-31`) `[VERIFIED]`
Current:
```cpp
void ExceptionHandler::Default(std::exception& error) {
    try {
        throw error;          // <-- SLICES to std::exception
    } catch (backend::hidpp10::Error& e) { ... }
      catch (backend::hidpp20::Error& e) { ... }
      ...
}
```
Fix: the function must rethrow the **active** exception so dynamic type is preserved. Bare `throw;` only works inside a `catch`, and `Default` is *called from* a catch handler (it takes `std::exception&`). The correct fix:
```cpp
void ExceptionHandler::Default(std::exception& error) {
    try {
        throw;                // rethrow the currently-handled exception (preserves dynamic type)
    } catch (backend::hidpp10::Error& e) {
        logPrintf(WARN, "HID++ 1.0 error ignored on task: %s", e.what());
    } catch (backend::hidpp20::Error& e) {
        logPrintf(WARN, "HID++ 2.0 error ignored on task: %s", e.what());
    } catch (std::system_error& e) {
        logPrintf(WARN, "System error ignored on task: %s", e.what());
    } catch (std::exception& e) {
        logPrintf(WARN, "Error ignored on task: %s", e.what());
    }
}
```
**Caveat to verify:** bare `throw;` requires an exception to currently be "in flight" (`std::current_exception()` non-null) when `Default` runs. Confirm every caller invokes `Default` from within a `catch` block. Check `util/ExceptionHandler.h` and `util/task.cpp` callers. If any caller invokes it *outside* an active exception, bare `throw;` calls `std::terminate`. `[VERIFIED bug; ASSUMED all callers are inside catch — MUST verify callers]` Also switch the log args from `error.what()` to `e.what()` (cosmetic, the caught object is the real one).

### #5 — Format-string log (`logid.cpp:97`) `[VERIFIED]`
Current: `logPrintf(WARN, e.what());` (line 97, the CONTEXT cite "96" is off-by-one — it is **line 97** in the current tree). Fix:
```cpp
logPrintf(WARN, "%s", e.what());
```
**Audit for siblings:** grep the tree for `logPrintf(<LEVEL>, <expr>)` where the 2nd arg is not a string literal. Known offender is `logid.cpp:97`. Also note `ExceptionHandler.cpp` originally passed `error.what()` as an *argument* (safe) not as the format (safe) — fine. Do a `grep -n "logPrintf([A-Z]*, [a-z]" ` sweep to be sure. `[VERIFIED primary site]`

### #8 — Release-mode assert on read length (`RawDevice.cpp:233`) `[VERIFIED]`
Current in `_readReports()`:
```cpp
while (-1 != (len = ::read(_fd, buf, max_data_length))) {
    assert(len <= max_data_length);                       // compiled out under NDEBUG
    std::vector<uint8_t> report(buf, buf + len);
    ...
}
```
`max_data_length = 32` (`RawDevice.h:49`). `read()` into a `max_data_length` buffer cannot return more than `max_data_length`, so this assert is about a kernel invariant — but per the locked decision, replace with an explicit guard so it holds in release:
```cpp
while (-1 != (len = ::read(_fd, buf, max_data_length))) {
    if (len < 0 || len > max_data_length) {
        logPrintf(WARN, "Ignoring HID read of unexpected length %zd on %s",
                  (ssize_t)len, _path.c_str());
        continue;   // do not construct a report from a bogus length
    }
    std::vector<uint8_t> report(buf, buf + len);
    ...
}
```
(`len` is `ssize_t`; the `-1 != len` loop guard already excludes the error case, but the explicit `len < 0` is harmless defense.) Note `assert` requires `#include <cassert>` which is currently relied upon; after removal it may be unused — leave includes alone unless `-Werror` complains. `[VERIFIED]`

Also note **`Report::setParams` at `Report.cpp:263` uses `assert(_params.size() <= ...)`** — out of scope (not attacker-controlled, internal invariant), leave it.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Authorization decision for `save()` | A custom uid/gid check in the daemon | polkit `check_authorization_sync` | polkit is the standard Linux authority; handles admin auth, session caching (`_keep`), agent prompting. A hand-rolled check can't prompt and can't honor distro policy. |
| Mapping a D-Bus caller to a subject | Resolving the caller PID and reading `/proc` | `polkit_system_bus_name_new(sender)` | PID-from-bus-name is racy (caller may exit/reconnect); polkit's bus-name subject is the supported path. |
| Sandboxing the daemon | A custom seccomp/chroot wrapper | systemd unit directives | systemd `ProtectSystem`/`NoNewPrivileges`/`CapabilityBoundingSet` are declarative, auditable via `systemd-analyze security`, and standard. |
| HID report bounds | Per-field ad-hoc checks scattered everywhere | One `hasHidppHeader()` helper + the ctor's existing pad | Consistency; the raw filter lambdas are the only true OOB surface. |

**Key insight:** every piece of this phase has a canonical Linux mechanism (D-Bus policy, polkit, systemd) — the only bespoke code is the *glue* (threading the caller through ipcgull, the bounds helper, the three bug fixes).

---

## Common Pitfalls

### Pitfall 1: polkit prompt never appears
**What goes wrong:** `check_authorization_sync` returns not-authorized with no dialog.
**Why:** missing `POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION`, or no polkit **authentication agent** running in the caller's session (headless/ssh has none).
**Avoid:** always pass the interaction flag; document that a session agent is required; fail-safe-deny is correct when none exists. `[ASSUMED]`

### Pitfall 2: `ProtectSystem=strict` breaks `save()`
**What goes wrong:** `/etc/logid.cfg` write fails with EROFS after hardening.
**Why:** strict mounts `/etc` read-only.
**Avoid:** the paired `ReadWritePaths=/etc/logid.cfg` (and ensure the file's parent allows the write — libconfig `writeFile` writes in place; if it writes a temp+rename, the *directory* `/etc` must be writable. **VERIFY libconfig's write strategy** — if it does temp+rename, `ReadWritePaths=/etc` may be needed instead of just the file). `[ASSUMED — verify libconfig writeFile in-place vs temp+rename; this changes the ReadWritePaths target]`

### Pitfall 3: group membership not picked up
**What goes wrong:** smoke test as a freshly-added `logiops` user still gets "access denied."
**Why:** supplementary groups are set at login; `usermod -aG` doesn't affect the current session.
**Avoid:** re-login / `newgrp logiops` / new session before testing. Also `systemctl reload dbus` after policy install.

### Pitfall 4: caller string empty / wrong
**What goes wrong:** polkit check always denies because `current_caller()` is empty.
**Why:** ipcgull threading not wired, or the slot is read outside the dispatch window.
**Avoid:** verify the ipcgull change independently (a debug log of the sender on every call) before wiring polkit.

### Pitfall 5: bare `throw;` outside an active exception
**What goes wrong:** `std::terminate` instead of logging.
**Why:** `ExceptionHandler::Default` called when no exception is in flight.
**Avoid:** confirm all callers invoke it from inside a `catch`. (See #1 caveat.)

---

## Validation Architecture

> `workflow.nyquist_validation = true` (config.json) → this section is REQUIRED. Each success criterion maps to an **observable** check. The daemon currently has **no automated test framework** (CONCERNS #12) — verification for this phase is primarily **runtime black-box scripts + static build assertions**, not unit tests.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | **None in-tree** (CONCERNS #12). C++ unit tests not wired into the build. Verification = shell smoke scripts + `systemd-analyze` + build success. |
| Config file | none — see Wave 0 |
| Quick run command | `cmake --build build 2>&1 \| tee build.log` (compile must succeed with `-Wall -Wextra`; CI adds `-Werror`) |
| Full suite command | Run the per-criterion shell checks below (manual/CI script). |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| ACCESS-01 | A `logiops`-group non-root user can call a method on `pizza.pixl.LogiOps` and change a device setting | smoke (runtime) | run as group user: `gdbus call --system --dest pizza.pixl.LogiOps --object-path /pizza/pixl/logiops/devices/... --method ...` (introspect first with `busctl introspect --system pizza.pixl.LogiOps /pizza/pixl/logiops`) → exit 0, setting changes | ❌ Wave 0 (write `tests/smoke/access01_nonroot_call.sh`) |
| ACCESS-02a | Authorized `Save` writes `/etc/logid.cfg` | smoke | as admin/active: call `...Config.Save`; assert mtime changes | ❌ Wave 0 |
| ACCESS-02b | **Denying** the polkit prompt leaves `/etc/logid.cfg` **byte-unchanged** | smoke | `sha256sum /etc/logid.cfg` before; call `Save` and cancel the auth dialog; assert `sha256sum` identical AND a "denied" log line in `journalctl -u logid` | ❌ Wave 0 |
| ACCESS-02c | polkit **absent/unreachable** → deny (fail-safe) | smoke | stop polkit / mock null authority; call `Save`; assert no write + D-Bus error returned | ❌ Wave 0 |
| ACCESS-03 | Hardening is in effect | static+runtime | `systemd-analyze security logid.service` (expect improved exposure score; assert `NoNewPrivileges=yes`, `CapabilityBoundingSet=` empty, `ProtectSystem=strict` via `systemctl show logid -p NoNewPrivileges,ProtectSystem,CapabilityBoundingSet,RestrictAddressFamilies`) AND daemon still enumerates a device (hidraw/uinput not broken) | ❌ Wave 0 |
| ACCESS-04 | A malformed **short** report does not OOB-read | unit-ish/runtime | feed a 1-3 byte vector through each filter lambda (or via `Report` ctor) under ASan; assert no crash/OOB. Minimal harness or an ASan build + crafted input. | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** `cmake --build build` succeeds (`-Wall -Wextra` clean).
- **Per wave merge:** run the relevant smoke script(s) for the criteria touched.
- **Phase gate:** all six checks green + `systemd-analyze security logid.service` shows the hardening applied + daemon still controls a real device.

### Observability specifics (how each criterion is *seen*)
- **ACCESS-01:** non-root `gdbus`/`busctl` call returns success instead of `org.freedesktop.DBus.Error.AccessDenied`. Tools present on this machine: `busctl`, `gdbus`, `dbus-send`. `[VERIFIED tooling present]`
- **ACCESS-02b (the headline criterion):** `sha256sum /etc/logid.cfg` identical across a denied save, plus a WARN journal line. This is the cleanest observable for "deny leaves config unchanged."
- **ACCESS-03:** `systemd-analyze security logid.service` (present on this machine) prints a per-directive exposure table; assert the locked directives appear and the score drops vs. baseline. Then confirm `logid` still enumerates the mouse (functional non-regression).
- **ACCESS-04:** best run under an **AddressSanitizer** build (`-fsanitize=address`) feeding short vectors — ASan turns a latent OOB into a hard, observable failure; with the guards in place it stays clean.

### Wave 0 Gaps
- [ ] `tests/smoke/access01_nonroot_call.sh` — non-root group-user D-Bus call
- [ ] `tests/smoke/access02_save_denied_unchanged.sh` — sha256 before/after a denied save
- [ ] `tests/smoke/access02_save_authorized.sh` + `access02_polkit_absent.sh`
- [ ] `tests/smoke/access03_hardening.sh` — `systemctl show` / `systemd-analyze security` assertions + device-still-works check
- [ ] `tests/access04_short_report` — minimal ASan harness or crafted-input path exercising the three filter lambdas
- [ ] CI: install `libpolkit-gobject-1-dev` (build) + run smoke scripts in a container that can talk to a system bus (note: full D-Bus/polkit smoke is hard in CI containers — at minimum compile + `systemd-analyze` static checks; hardware-dependent checks stay manual per `TESTED.md`)

*(No existing test infrastructure covers any of this — all are new.)*

---

## Security Domain

`security_enforcement` not present in config.json → treated as enabled. This entire phase **is** the security work, so the relevant controls are the phase deliverables.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No app-level auth; OS identity only |
| V3 Session Management | no | n/a |
| V4 Access Control | **yes** | D-Bus group policy (coarse) + polkit `auth_admin_keep` on `save()` (fine). This is ACCESS-01/02. |
| V5 Input Validation | **yes** | HID report length-checks before indexing (ACCESS-04); the daemon treats hardware input as untrusted. |
| V6 Cryptography | no | No secrets/crypto in scope |
| (Platform hardening, beyond ASVS) | **yes** | systemd sandboxing + capability dropping (ACCESS-03) |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Out-of-bounds read on short/malformed HID report | Tampering / DoS / Info-disclosure | Length-check before indexing (ACCESS-04); the raw filter lambdas are the live OOB surface |
| Unauthorized root-owned file write via D-Bus `save()` | Elevation of Privilege | polkit `check_authorization` + fail-safe-deny (ACCESS-02) |
| Widened D-Bus audience reaching an unsandboxed root process | Elevation of Privilege | systemd `NoNewPrivileges`/`ProtectSystem=strict`/empty `CapabilityBoundingSet` (ACCESS-03) |
| Format-string injection via `e.what()` as printf format | Tampering / RCE-class | `"%s"` format (CONCERNS #5) |
| Type-confusion in error handling (sliced exception) masking real errors | (correctness → masks Tampering signals) | bare `throw;` (CONCERNS #1) |
| Security check elided in release builds | (defense bypass) | replace `assert` with explicit runtime check (CONCERNS #8 / ACCESS-04) |

---

## Environment Availability

| Dependency | Required By | Available (this machine) | Version | Fallback |
|------------|------------|--------------------------|---------|----------|
| `polkit-gobject-1` (dev/pkg-config) | ACCESS-02 build | **✗ NOT installed** | — | Install `libpolkit-gobject-1-dev`; **blocking for build until added** |
| `pkexec` runtime | ACCESS-02 manual test | ✓ | present | — |
| `busctl` | ACCESS-01/02/03 smoke | ✓ | present | — |
| `gdbus` | ACCESS-01 smoke | ✓ | present | — |
| `dbus-send` | smoke | ✓ | present | — |
| `systemd-analyze` | ACCESS-03 verify | ✓ | present | — |
| `glib-2.0`/`gio-2.0` | ipcgull (existing) | ✓ (build already uses) | — | — |
| `logiops` group | ACCESS-01 test | **✗ absent** (`plugdev` exists, gid 46) | — | `groupadd logiops` for the test; packaging creates it (Phase 9) |

**Missing with no fallback (blocking):**
- `libpolkit-gobject-1-dev` — must be installed before the ACCESS-02 task compiles; add to build instructions + CI.

**Missing with fallback:**
- `logiops` group — create manually for the smoke test (`groupadd logiops; usermod -aG logiops <user>`).

---

## Code Examples (verified in-tree, for the planner to reference)

### Where `Save` is registered (the polkit insertion point)
```cpp
// src/logid/Configuration.cpp:73-77  [VERIFIED]
Configuration::IPC::IPC(Configuration* config) :
        ipcgull::interface(SERVICE_ROOT_NAME ".Config", {
                {"Save", {config, &Configuration::save}}
        }, {}, {}) {
}
```

### Where the caller bus name exists but is dropped (the crux)
```cpp
// src/ipcgull/src/server_gdbus.cpp:301-354  [VERIFIED, abridged]
static void gdbus_method_call(
        [[maybe_unused]] GDBusConnection* connection,
        [[maybe_unused]] const gchar* sender,   // <-- caller unique name, e.g. ":1.42"
        ...) {
    ...
    const auto args = std::get<variant_tuple>(v_args);
    const auto response = f_it->second(args);   // <-- sender NOT passed in
    ...
}
```

### Bus selection (keep system bus as default)
```cpp
// src/logid/logid.cpp:155-161  [VERIFIED]
#ifdef USE_USER_BUS
    auto server_bus = ipcgull::IPCGULL_USER;
#else
    auto server_bus = ipcgull::IPCGULL_SYSTEM;   // default — keep
#endif
    auto server = ipcgull::make_server(SERVICE_ROOT_NAME, server_root_node, server_bus);
```

---

## Project Constraints (from CLAUDE.md)
- **C++20** throughout; header guards `#ifndef`/`#define` (not `#pragma once`); follow `.editorconfig`.
- Naming: PascalCase types/files, camelCase methods, `_leadingUnderscore` private members, snake_case free/util fns.
- **Smart pointers** for ownership; `make()` factories for shared/post-construction objects.
- **Exception-based** error handling routed through `util/ExceptionHandler` — and note CLAUDE.md explicitly warns: rethrow with bare `throw;` not `throw error;` (exactly CONCERNS #1).
- **Logging:** `logPrintf` printf-style; CLAUDE.md explicitly warns "avoid passing untrusted/runtime strings as the format argument" (exactly CONCERNS #5).
- CMake per-subtree; config schema declared in `config/schema.h`.
- **GSD workflow enforcement:** repo edits must go through a GSD command — relevant to execution, not research.
- CI builds with `-DCMAKE_CXX_FLAGS="-Werror"` across ubuntu:latest/20.04, fedora:latest, arch — new code must be warning-clean on all four, and the new `polkit-gobject-1` dep must be installed in each CI image.

---

## State of the Art

| Old Approach | Current Approach | Impact |
|--------------|------------------|--------|
| Root-only D-Bus policy (`user="root"`) | `group="..."` policy for non-root clients + polkit for privileged writes | Standard pattern for hardware daemons (e.g. how many system services expose a group + polkit) `[ASSUMED]` |
| `assert` for runtime invariants | explicit runtime checks (asserts compile out under `NDEBUG`) | Locked decision; matches CONCERNS #8 |
| Unsandboxed root systemd service | `ProtectSystem=strict` + `NoNewPrivileges` + empty `CapabilityBoundingSet` | Modern systemd hardening baseline |

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `<policy group="logiops">` with `send_destination`/`receive_sender` `<allow>` is valid and overrides the default `<deny>` | D-Bus Policy | ACCESS-01 fails silently (access denied or unexpectedly open). VERIFY `man dbus-daemon`. |
| A2 | `polkit-gobject-1` is the correct pkg-config module and `polkit_authority_get_sync` / `polkit_authority_check_authorization_sync` / `polkit_system_bus_name_new` / `polkit_authorization_result_get_is_authorized` exist with these signatures | polkit Integration | ACCESS-02 won't compile. VERIFY against installed `polkit/polkit.h`. (dev pkg not installed on this machine.) |
| A3 | `POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION` is the flag enabling the auth prompt | polkit | No prompt; `auth_admin_keep` never challenges → all denied. VERIFY enum name. |
| A4 | `.policy` action XML schema + install dir `/usr/share/polkit-1/actions/` + `auth_admin_keep` spelling | polkit | Action not registered; check fails. VERIFY `man polkit` / polkit DTD. |
| A5 | Empty `CapabilityBoundingSet=` works because root bypasses DAC for hidraw/uinput; no `CAP_*` needed | systemd | Daemon loses device access. VERIFY on real hardware; add caps only if testing proves needed. |
| A6 | `PrivateDevices=yes` would hide hidraw/uinput (so must NOT be used); `DeviceAllow` default is safe | systemd | If wrong, either over-restrict (break) or under-restrict. VERIFY `man systemd.exec`. |
| A7 | `ProtectSystem=strict` + `ReadWritePaths=/etc/logid.cfg` lets `save()` write; depends on libconfig writing in-place vs temp+rename | systemd / Pitfall 2 | If libconfig does temp+rename in `/etc`, need `ReadWritePaths=/etc`. VERIFY libconfig `writeFile`. |
| A8 | GDBus dispatches method calls on a single main-loop thread, so a per-call "current caller" slot under `server_lock` is safe | polkit Option A | Race if multi-threaded dispatch. VERIFY GMainContext threading in ipcgull. |
| A9 | `_sync` (blocking) polkit call on the dispatch thread won't deadlock the bus for a rare save | polkit | Bus stall during auth prompt. Mitigate with async API if observed. |
| A10 | All `ExceptionHandler::Default` callers invoke it from inside a `catch` (so bare `throw;` is valid) | CONCERNS #1 | `std::terminate` if called outside an active exception. VERIFY callers in `util/task.cpp` etc. |
| A11 | CONTEXT line cites for #5 ("logid.cpp:96") are off-by-one; actual is line 97 | CONCERNS #5 | Minor — fix the right line; verified this session it is line 97. |

---

## Open Questions

1. **libconfig write strategy (in-place vs temp+rename).**
   - Known: `_config.writeFile(path)` (Configuration.cpp:61) writes `/etc/logid.cfg`.
   - Unclear: whether libconfig writes in place (then `ReadWritePaths=/etc/logid.cfg` suffices) or writes a temp file in `/etc` and renames (then needs `ReadWritePaths=/etc`).
   - Recommendation: test `save()` under the hardened unit early; widen `ReadWritePaths` to `/etc` only if the file-scoped path fails.

2. **ipcgull threading model.**
   - Known: dispatch holds `server_lock` (recursive_mutex).
   - Unclear: whether GDBus can invoke `gdbus_method_call` concurrently from multiple threads.
   - Recommendation: assume single main-loop thread (A8); if uncertain, use a thread-local for the "current caller" slot to be safe under either model.

3. **Async vs sync polkit.**
   - Known: `_sync` blocks the calling thread.
   - Unclear: acceptable for the (rare) save on the main loop?
   - Recommendation: start with `_sync`; if the bus stalls during the prompt, migrate `Save` to ipcgull's async/deferred-reply path (larger change — flag for a follow-up if needed).

4. **CI smoke feasibility.**
   - Full D-Bus+polkit+hardware smoke is hard in CI containers. Recommendation: CI covers compile (+`-Werror`, +polkit dep) and `systemd-analyze` static checks; runtime/hardware checks stay manual (`TESTED.md`).

---

## Sources

### Primary (HIGH confidence — read in-tree this session)
- `src/logid/logiops-dbus.conf.in`, `src/logid/logid.service.in`, `src/logid/Configuration.cpp`, `src/logid/DeviceManager.cpp:31-46`, `src/logid/ipc_defs.h`, `src/logid/logid.cpp:85-103,145-184`
- `src/ipcgull/src/server_gdbus.cpp:300-414`, `src/ipcgull/src/function.cpp`, `src/ipcgull/src/include/ipcgull/function.h`, `interface.h`, `server.h`, `connection.h`
- `src/logid/backend/hidpp/Device.cpp:110-135`, `backend/hidpp10/ReceiverMonitor.cpp:30-55,155-179`, `backend/hidpp/Report.cpp:118-294`, `backend/hidpp/Report.h`, `backend/hidpp/defs.h`, `backend/raw/RawDevice.cpp:205-249`, `RawDevice.h:49`
- `src/logid/util/ExceptionHandler.cpp`
- `CMakeLists.txt`, `src/logid/CMakeLists.txt`
- `.planning/{REQUIREMENTS,STATE}.md`, `.planning/phases/01-.../01-CONTEXT.md`, `.planning/research/ARCHITECTURE.md`, `.planning/codebase/{INTEGRATIONS,CONCERNS}.md`, `CLAUDE.md`, `.planning/config.json`
- Tooling probe (this session): `busctl`/`gdbus`/`dbus-send`/`systemd-analyze`/`pkexec` present; `polkit-gobject-1` pkg-config **absent**; no `logiops` group.

### Secondary / Tertiary (MEDIUM-LOW — training knowledge, web verification UNAVAILABLE)
- D-Bus `system.d` policy semantics (`group=`, `send_destination`, `receive_sender`, precedence) — `man dbus-daemon`. **Verify.**
- libpolkit-gobject-1 API + `.policy` XML + `auth_admin_keep` + action install dir — polkit docs/headers. **Verify.**
- systemd directive names/semantics (`ProtectSystem`, `CapabilityBoundingSet`, `RestrictAddressFamilies`, `PrivateDevices`, `ReadWritePaths`) — `man systemd.exec`. **Verify.**

---

## Metadata

**Confidence breakdown:**
- In-tree code facts (files/lines/signatures, the ipcgull crux): **HIGH** — every file read this session.
- D-Bus policy / polkit API / systemd directive *syntax*: **MEDIUM** — training knowledge, web verification unavailable; all in Assumptions Log.
- Capability-set minimality (empty set works): **MEDIUM-LOW** — must be confirmed on real hardware.

**Research date:** 2026-05-30
**Valid until:** in-tree facts stable until the code changes; external-syntax assumptions should be re-verified at implementation time (they are stable APIs but unverified this session).
