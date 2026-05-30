---
phase: 01-access-path-daemon-hardening
plan: 06
subsystem: auth
tags: [polkit, dbus, polkit-gobject-1, cmake, authorization, c++20]

# Dependency graph
requires:
  - phase: 01-05
    provides: ipcgull::current_caller() — the D-Bus caller's unique bus name during dispatch
  - phase: 01-03
    provides: hardened logid.service (ProtectSystem=strict + ReadWritePaths=/etc/logid.cfg)
  - phase: 01-02
    provides: D-Bus policy opened to group=logiops (the audience whose privileged writes must now be gated)
provides:
  - polkit-gated Configuration::save() (action pizza.pixl.logiops.save-config, auth_admin_keep)
  - logiops-policy.policy.in polkit action file installed to /usr/share/polkit-1/actions
  - CMake linkage against polkit-gobject-1
  - fail-safe-deny on null authority / null subject / null result / error / empty caller
affects: [03-core-config-persistence, 09-debian-packaging]

# Tech tracking
tech-stack:
  added: [polkit-gobject-1 (libpolkit-gobject-1, v124)]
  patterns:
    - "Daemon-side authorization: build a polkit_system_bus_name subject from the ipcgull caller and check_authorization_sync before a privileged write"
    - "Default-deny: every error/null branch returns false; the throw precedes writeFile so /etc/logid.cfg stays byte-unchanged on denial"

key-files:
  created:
    - src/logid/logiops-policy.policy.in
  modified:
    - src/logid/CMakeLists.txt
    - src/logid/Configuration.cpp
    - src/logid/Configuration.h

key-decisions:
  - "Gate exactly Configuration::save() (not every live mutation); auth_admin_keep on all three <allow_*>"
  - "Accept the blocking _sync polkit call on the GLib dispatch thread for the rare save (T-01-06-04 = accept); async/deferred-reply is a follow-up only if a real bus stall is observed"
  - "Subject built from ipcgull::current_caller() (unique bus name), not from a resolved PID (racy)"

patterns-established:
  - "polkit check helper Configuration::checkSaveAuthorized(callerBusName) — fail-safe DENY on every path"

requirements-completed: [ACCESS-02]

# Metrics
duration: 3min
completed: 2026-05-30
---

# Phase 01 Plan 06: polkit-gate Configuration::save() Summary

**Configuration::save() now queries polkit (action `pizza.pixl.logiops.save-config`, `auth_admin_keep`) for the D-Bus caller before writing `/etc/logid.cfg`, fail-safe-denying on any error/null/empty-caller path; project compiles AND links clean with `-Werror` against polkit-gobject-1 v124.**

## Performance

- **Duration:** ~3 min
- **Started:** 2026-05-30T18:14:54Z
- **Completed:** 2026-05-30T18:18:06Z
- **Tasks:** 2 automated (Tasks 1-2 committed); 1 human-verify checkpoint (Task 3) statically verified + manual steps documented below
- **Files modified:** 4 (1 created, 3 modified)

## Accomplishments

- Shipped the polkit action `pizza.pixl.logiops.save-config` (all three `<allow_*>` = `auth_admin_keep`), installed to `/usr/share/polkit-1/actions`.
- Linked `polkit-gobject-1` (v124) via `pkg_check_modules(POLKIT REQUIRED ...)`; `logid` now dynamically links `libpolkit-gobject-1.so.0` (verified via `ldd`).
- Gated `Configuration::save()`: obtains the caller via `ipcgull::current_caller()`, builds a `polkit_system_bus_name` subject, calls `polkit_authority_check_authorization_sync(...)` with `POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION`, throws before `writeFile()` on any denial.
- Full project builds AND links **warning-clean with `-Werror`** now that polkit is present.

## Task Commits

1. **Task 1: .policy action + CMake linkage** - `ce75eb3` (feat)
2. **Task 2: polkit-gate save() with fail-safe-deny** - `f51f4d7` (feat)

**Plan metadata:** (final docs commit — this SUMMARY + STATE + ROADMAP)

## Files Created/Modified

- `src/logid/logiops-policy.policy.in` (created) - polkit action `pizza.pixl.logiops.save-config`, `auth_admin_keep` on `allow_any`/`allow_inactive`/`allow_active`.
- `src/logid/CMakeLists.txt` (modified) - `pkg_check_modules(POLKIT REQUIRED polkit-gobject-1)`; `${POLKIT_INCLUDE_DIRS}` added to `include_directories`, `${POLKIT_LIBRARIES}` to `target_link_libraries(logid ...)`; policy install block to `/usr/share/polkit-1/actions`.
- `src/logid/Configuration.cpp` (modified) - `#include <polkit/polkit.h>` + `<ipcgull/connection.h>` + `<stdexcept>`; `checkSaveAuthorized()` helper; caller check + throw at top of `save()`.
- `src/logid/Configuration.h` (modified) - private `static bool checkSaveAuthorized(const std::string& callerBusName);`.

## polkit Symbol / Signature Verification (RESEARCH A2/A3/A4)

Verified against the **installed** headers (`$(pkg-config --variable=includedir polkit-gobject-1)/polkit-1/polkit/*.h`), polkit-gobject-1 **v124** — **no signature drift**:

| Symbol | Present |
|--------|---------|
| `polkit_authority_get_sync` | ✓ |
| `polkit_authority_check_authorization_sync` | ✓ |
| `polkit_system_bus_name_new` | ✓ |
| `polkit_authorization_result_get_is_authorized` | ✓ |
| `POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION` (enum) | ✓ |

All symbols match the RESEARCH-assumed names/signatures (A2/A3 resolved). `.policy` schema + install dir `/usr/share/polkit-1/actions` + `auth_admin_keep` spelling (A4) accepted; the XML validates via `xml.dom.minidom`.

## Static Verification of the Checkpoint (Task 3)

The interactive parts (the actual polkit prompt, the deny-leaves-config-unchanged hash check, the authorized-write-under-the-hardened-unit check) require a real logged-in session with a polkit agent + the installed/restarted daemon and were **not** run in this non-interactive executor. What IS statically confirmed:

- **Action-id string match:** `pizza.pixl.logiops.save-config` is byte-identical between `logiops-policy.policy.in` and `Configuration.cpp` (T-01-06-05 mitigated).
- **Fail-safe-deny branches present:** in `checkSaveAuthorized()` there are `return false` on empty caller, on null authority, and on null subject, plus `authorized=false` default when result is null/error — and `save()` throws on empty caller **or** unauthorized **before** `config::set(...)`/`_config.writeFile(...)`. (3× `return false` + 1× `throw` all precede `writeFile` at Configuration.cpp:117.) (T-01-06-01/02/03 mitigated.)
- **Build links polkit:** `cmake --build build` with `-DCMAKE_CXX_FLAGS="-Werror"` builds + links clean; `ldd build/logid` shows `libpolkit-gobject-1.so.0`.
- **ReadWritePaths note:** Plan 03's unit currently has `ProtectSystem=strict` + file-scoped `ReadWritePaths=/etc/logid.cfg` (confirmed in `src/logid/logid.service.in`). Whether this file-scoped path suffices vs. needing widening to `/etc` is exactly what the **manual step 4** below decides (RESEARCH A7 / Open Question 1).
- **Smoke script ready:** `test/smoke/polkit-deny.sh` exists, invokes `Save` on `pizza.pixl.LogiOps.Config` and asserts `sha256sum /etc/logid.cfg` is unchanged; it skips cleanly when the daemon isn't running.

## Task 3 — MANUAL Human-Verify Steps (run in a real desktop session)

> Deferred to a real logged-in session with an interactive polkit agent. Run these to close ACCESS-02 (a/b/c).

**Prerequisites:**
```bash
# Build + install the daemon with the new polkit gate + action file
cmake -S . -B build -DCMAKE_CXX_FLAGS="-Werror"
cmake --build build
sudo cmake --install build            # installs logid, the .policy action, dbus conf, hardened unit
sudo systemctl daemon-reload && sudo systemctl restart logid
# logiops group already exists (gid 1001) with the operator as a member; re-login if needed.
# A desktop session with an interactive polkit agent must be running (GNOME/KDE ship one).
```

**1. DENY leaves /etc/logid.cfg byte-unchanged (ACCESS-02b):**
```bash
bash test/smoke/polkit-deny.sh
```
- When the polkit admin-auth prompt appears, **CANCEL** it.
- Expect: the script reports the two `sha256sum` hashes **IDENTICAL** (`PASS: ... byte-identical`), and `journalctl -u logid` shows `Unauthorized save() denied.`

**2. AUTHORIZED save writes under the hardened unit (ACCESS-02a, cross-checks Plan 03 ReadWritePaths / RESEARCH A7):**
- Re-invoke `Save` and **AUTHENTICATE** at the prompt:
  ```bash
  busctl --system call pizza.pixl.LogiOps /pizza/pixl/logiops pizza.pixl.LogiOps.Config Save
  ```
- Expect: the call succeeds and `/etc/logid.cfg` mtime/hash **CHANGES**.
- **If it fails with EROFS / "read-only file system"** → libconfig is doing temp+rename inside `/etc` (RESEARCH A7 / Open Question 1). Widen Plan 03's directive from `ReadWritePaths=/etc/logid.cfg` to `ReadWritePaths=/etc` in `src/logid/logid.service.in`, re-install + `systemctl daemon-reload && systemctl restart logid`, and re-test. **Record which path sufficed** (file-scoped vs widened to `/etc`).

**3. Fail-safe with no agent (ACCESS-02c):**
- Over SSH with no polkit agent (or stop the polkit service), call `Save`:
  ```bash
  busctl --system call pizza.pixl.LogiOps /pizza/pixl/logiops pizza.pixl.LogiOps.Config Save
  ```
- Expect: a D-Bus error (`Not authorized to save configuration`), **no write**, and a WARN denial in `journalctl -u logid`.

## Decisions Made

- Followed the plan exactly: gate only `save()`, `auth_admin_keep` on all three `<allow_*>`, subject from `current_caller()`, `_sync` accepted (A9).

## Deviations from Plan

None - plan executed exactly as written. (Added `#include <stdexcept>` for `std::runtime_error`; this is a correctness include, not a behavioral deviation.)

## Issues Encountered

None. The two environmental blockers the plan flagged (polkit dev pkg, `logiops` group) were already resolved by the operator: `polkit-gobject-1` v124 present, group `logiops` (gid 1001) exists with the operator as a member.

## A9 sync-vs-async Decision

The blocking `polkit_authority_check_authorization_sync` runs on the GLib main-loop dispatch thread during the auth prompt. **Accepted** for the rare save action (T-01-06-04 = accept). Flagged for an async/deferred-reply migration **only if** a real bus stall is observed during the manual prompt verification.

## User Setup Required

- An interactive **polkit authentication agent** must run in the test session for the prompt to appear (GNOME/KDE ship one; headless/ssh has none — fail-safe-deny is the correct behavior there).
- `make install` must place `pizza.pixl.logiops.policy` under `/usr/share/polkit-1/actions/` (handled by the new CMake install block).

## Next Phase Readiness

- ACCESS-02 implementation complete; the only open item is the operator's interactive confirmation (manual steps above), incl. the file-scoped-vs-`/etc` ReadWritePaths decision feeding back to Plan 03.
- Phase 3 (Core Config UI & Persistence) can rely on a polkit-gated `Save` as its persistence path.
- Phase 9 (Debian Packaging) must ship `pizza.pixl.logiops.policy` alongside the D-Bus policy and systemd unit, and depends on the ReadWritePaths outcome from manual step 2.

## Self-Check: PASSED

- Files verified present: `logiops-policy.policy.in`, `Configuration.cpp`, `Configuration.h`, `CMakeLists.txt`, `01-06-SUMMARY.md`.
- Commits verified in git history: `ce75eb3` (Task 1), `f51f4d7` (Task 2).

---
*Phase: 01-access-path-daemon-hardening*
*Completed: 2026-05-30*
