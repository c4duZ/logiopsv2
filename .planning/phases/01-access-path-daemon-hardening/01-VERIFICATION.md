---
phase: 01-access-path-daemon-hardening
verified: 2026-05-30T00:00:00Z
status: human_needed
score: 18/18 statically-verifiable must-haves verified
overrides_applied: 0
re_verification:
  previous_status: none
  note: "Initial verification. REVIEW (3 warnings) + REVIEW-FIX (all 3 fixed) already applied; this is goal-backward verification, not code review."
requirements_coverage:
  ACCESS-01: satisfied_static_pending_human
  ACCESS-02: satisfied_static_pending_human
  ACCESS-03: satisfied_static_pending_human
  ACCESS-04: satisfied
human_verification:
  - test: "Trigger an interactive polkit prompt on Configuration::save() from a desktop session with an auth agent, then DENY it."
    expected: "/etc/logid.cfg is byte-for-byte unchanged (sha256 identical before/after) and the daemon journal shows a WARN denial line."
    why_human: "Requires a live polkit authentication agent and an interactive prompt; cannot be exercised headlessly. The fail-safe-deny code paths are statically verified, but the authorize/deny round-trip is a runtime behavior."
  - test: "Authorize a save (approve the polkit prompt) while logid runs under the hardened systemd unit (ProtectSystem=strict + ReadWritePaths=/etc/logid.cfg)."
    expected: "The write to /etc/logid.cfg succeeds (no EROFS) and the new config is persisted."
    why_human: "ReadWritePaths re-opening the one config path under ProtectSystem=strict is a runtime systemd mount decision; needs a running, authorized session to confirm the write actually lands."
  - test: "From a non-root user who is a member of the `logiops` group, call a logid D-Bus method (e.g. read/change a device setting) on real Logitech hardware."
    expected: "The call reaches pizza.pixl.LogiOps over the system bus and the setting changes on the device; a non-group non-root user is still denied."
    why_human: "Requires the `logiops` system group, a group-member session, the running daemon on the system bus, and physical Logitech hardware. NOTE: the build does not create the `logiops` group (REVIEW IN-05) — an admin must create it for this path to work out of the box."
  - test: "Run the daemon under the full hardened unit with a Logitech device attached and observe device enumeration + input emulation."
    expected: "logid still opens /dev/hidraw* and /dev/uinput, enumerates devices, and emulated input works (hardening did not break the device path)."
    why_human: "RestrictAddressFamilies / CapabilityBoundingSet= / ProtectSystem=strict interaction with real hidraw/uinput access can only be confirmed with a device attached and the unit active."
notes_followups:
  - "IN-05: D-Bus policy grants group=logiops but no sysusers/postinst creates the group. Fails closed (deny), so not a security defect, but the non-root access path is inert until an admin creates the group. Packaging follow-up (Phase 9 / PKG-01)."
  - "IN-06: ReadWritePaths=/etc/logid.cfg may require the file to exist at unit start on older systemd. Packaging should ship an example config. Follow-up."
  - "WR-01 accepted-deferred: polkit _sync blocks the dispatch thread under server_lock during the prompt. Documented in-code; revisit if a real stall is observed."
deferred:
  - truth: "The `logiops` system group is created so the non-root access path works out of the box"
    addressed_in: "Phase 9"
    evidence: "PKG-01: 'installs on Debian/Ubuntu as a package that depends on the logid daemon and ships its D-Bus policy, polkit action, and systemd unit' — group creation belongs to packaging/postinst, not Phase 1."
---

# Phase 1: Access Path & Daemon Hardening — Verification Report

**Phase Goal:** A non-root user can safely reach the `logid` D-Bus service without widening the root attack surface, and privileged config writes are authorization-gated.
**Verified:** 2026-05-30
**Status:** human_needed
**Re-verification:** No — initial verification (REVIEW + REVIEW-FIX already applied upstream)

## Goal Achievement

The phase decomposes into four requirement-aligned outcomes. Every artifact, key link, and CONCERNS fix that can be checked statically is present, substantive, wired, and builds/links/tests clean. The remaining gaps are not code gaps — they are runtime behaviors (interactive polkit, a real group-member session, real hardware) that cannot be exercised in this environment and are routed to human verification.

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | D-Bus policy grants group=logiops send/receive | ✓ VERIFIED | `logiops-dbus.conf.in:18-21` — `<policy group="logiops">` allows `send_destination`/`receive_sender`, no `own`. |
| 2 | Only root may `own` the name | ✓ VERIFIED | `own="pizza.pixl.LogiOps"` appears only in the `user="root"` block (line 11); default policy denies receive_sender. |
| 3 | systemd unit: NoNewPrivileges/ProtectSystem=strict/empty CapabilityBoundingSet | ✓ VERIFIED | `logid.service.in:17,18,27` — `NoNewPrivileges=yes`, `ProtectSystem=strict`, `CapabilityBoundingSet=` (empty). |
| 4 | RestrictAddressFamilies = AF_UNIX + AF_NETLINK only; no PrivateDevices | ✓ VERIFIED | `logid.service.in:25` exactly `AF_UNIX AF_NETLINK`; explicit comment (12-16) documents intentional non-sandboxing of /dev. |
| 5 | save() can still write /etc/logid.cfg under ProtectSystem=strict | ✓ VERIFIED (static) | `ReadWritePaths=/etc/logid.cfg` (line 23). Runtime write deferred to human. |
| 6 | hasHidppHeader() guards all three raw filter lambdas | ✓ VERIFIED | `Device.cpp:124`, `ReceiverMonitor.cpp:40`, `ReceiverMonitor.cpp:168` each call `hasHidppHeader` before any `report[Offset::*]`. |
| 7 | isError10/isError20 length-guard before indexing 3,4,5 | ✓ VERIFIED | `Report.cpp:270,285` — `if (_data.size() <= Offset::Parameters + 1) return false;` before reads. |
| 8 | Short/empty HID report rejected before header indexing (no OOB read) | ✓ VERIFIED | `hasHidppHeader` returns `size() >= Report::HeaderLength` (Report.h:136-138); bounds unit test exercises short/empty reports. |
| 9 | ExceptionHandler::Default uses bare `throw;` (no slicing) | ✓ VERIFIED | `ExceptionHandler.cpp:29` — `throw;` with explanatory comment. |
| 10 | No runtime-controlled string passed as logPrintf format | ✓ VERIFIED | `logid.cpp:79,97,149,167` all use `"%s"` with the runtime value as an argument. |
| 11 | Read-length validation holds in release (no assert) | ✓ VERIFIED | `RawDevice.cpp:241` — `if (len <= 0 || static_cast<size_t>(len) > sizeof(buf))` (WR-03 fix), survives NDEBUG. |
| 12 | get_phys/get_name no `len-1` underflow on len==0 | ✓ VERIFIED | `RawDevice.cpp:96-97,110-111` — `if (len <= 0) return {};` (WR-02 fix) before `len - 1`. |
| 13 | ipcgull::current_caller() declared + returns sender during dispatch | ✓ VERIFIED | `connection.h:34` decl; `server_gdbus.cpp:56,58` thread_local slot + accessor. |
| 14 | Caller slot set/cleared under server_lock via RAII (no race) | ✓ VERIFIED | `server_gdbus.cpp:320` lock held; `327` set from `sender`; `329` RAII `CallerGuard` clears on every exit. |
| 15 | save() polkit-gates before writeFile, fail-safe deny on every branch | ✓ VERIFIED | `Configuration.cpp:118-123` — `current_caller()` → `checkSaveAuthorized()`; every null/error/empty branch returns false before `writeFile` (127). |
| 16 | .policy action id matches check_authorization_sync string exactly | ✓ VERIFIED | Both `logiops-policy.policy.in:8` and `Configuration.cpp:99` = `pizza.pixl.logiops.save-config`. |
| 17 | Project builds clean and links polkit | ✓ VERIFIED | `cmake --build build` exit 0; `ldd logid` shows `libpolkit-gobject-1.so.0`; CMakeLists.txt:78,92 link POLKIT. |
| 18 | hidpp bounds unit passes GREEN (and CTest registered) | ✓ VERIFIED | `ctest -R hidpp_bounds` → 1/1 Passed; `test/CMakeLists.txt:15` `add_test`, `CMakeLists.txt:64-65` `enable_testing()`+`add_subdirectory(test)`. |

**Score:** 18/18 statically-verifiable must-haves verified. 4 runtime behaviors routed to human verification.

### Deferred Items

| # | Item | Addressed In | Evidence |
|---|------|-------------|----------|
| 1 | `logiops` system group creation (out-of-box non-root access) | Phase 9 | PKG-01 — packaging ships policy/action/unit; group creation is a postinst/sysusers concern. |

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/logid/logiops-dbus.conf.in` | group=logiops send/receive, root-only own | ✓ VERIFIED | Substantive, least-privilege. |
| `src/logid/logid.service.in` | hardened [Service] block | ✓ VERIFIED | All directives present + ReadWritePaths. |
| `src/logid/logiops-policy.policy.in` | save-config action, auth_admin_keep | ✓ VERIFIED | Action id matches code. |
| `src/logid/Configuration.cpp` | polkit gate via current_caller() | ✓ VERIFIED | Fail-safe deny on all paths. |
| `src/logid/backend/hidpp/Report.h` | hasHidppHeader() helper | ✓ VERIFIED | In `logid::backend::hidpp`. |
| `src/logid/util/ExceptionHandler.cpp` | bare-throw rethrow | ✓ VERIFIED | `throw;` |
| `src/ipcgull/src/include/ipcgull/connection.h` | current_caller() decl | ✓ VERIFIED | Public accessor. |
| `src/ipcgull/src/server_gdbus.cpp` | sender → caller slot | ✓ VERIFIED | RAII-guarded under lock. |
| `test/hidpp_bounds_test.cpp` | short/empty via hasHidppHeader() | ✓ VERIFIED | 92 lines, 10 references; passes. |
| `test/CMakeLists.txt` | CTest registration | ✓ VERIFIED | `add_test(NAME hidpp_bounds ...)`. |
| `test/smoke/*.sh` | self-describing smoke scripts | ✓ VERIFIED | Executable; each documents manual prerequisites. |

### Key Link Verification

| From | To | Via | Status |
|------|----|----|--------|
| Configuration.cpp | ipcgull::current_caller() | caller → polkit_system_bus_name subject | ✓ WIRED |
| Configuration.cpp | logiops-policy.policy.in | action id `pizza.pixl.logiops.save-config` | ✓ WIRED (exact match) |
| Device.cpp / ReceiverMonitor.cpp | Report.h | hasHidppHeader() guard at raw lambdas | ✓ WIRED (3/3) |
| server_gdbus.cpp | current_caller() | set from `sender` before dispatch, clear after | ✓ WIRED |
| CMakeLists.txt | test/CMakeLists.txt | enable_testing() + add_subdirectory(test) | ✓ WIRED |
| logid.service.in | /etc/logid.cfg write path | ReadWritePaths under ProtectSystem=strict | ✓ WIRED (runtime → human) |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Project builds clean (-Werror) | `cmake --build build` | exit 0, all targets built | ✓ PASS |
| hidpp bounds unit passes | `ctest --test-dir build -R hidpp_bounds` | 1/1 Passed | ✓ PASS |
| polkit linked into daemon | `ldd build/.../logid \| grep polkit` | libpolkit-gobject-1.so.0 | ✓ PASS |
| polkit prompt DENY leaves config unchanged | manual (test/smoke/polkit-deny.sh) | needs live agent | ? SKIP → human |
| non-root group user reaches bus | manual (test/smoke/access-path.sh) | needs group + hardware | ? SKIP → human |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| ACCESS-01 | 01-01, 01-02 | non-root logiops-group user reaches D-Bus | ✓ static / ⧗ human | Policy correct (truths 1-2); live bus reach needs group + hardware. |
| ACCESS-02 | 01-01, 01-05, 01-06 | save() gated behind polkit | ✓ static / ⧗ human | Gate + action id + fail-safe verified (truths 13-16); interactive prompt needs human. |
| ACCESS-03 | 01-01, 01-03 | systemd hardening | ✓ static / ⧗ human | Directives verified (truths 3-5); hidraw/uinput-still-works needs hardware. |
| ACCESS-04 | 01-01, 01-04 | HID fields length-checked before indexing | ✓ SATISFIED | Fully static-verifiable; helper + guards + passing unit test (truths 6-12). |

All four declared requirement IDs are accounted for; none orphaned. REQUIREMENTS.md maps exactly ACCESS-01..04 to Phase 1, matching the union of plan `requirements` fields.

### Anti-Patterns Found

None blocking. No stubs, placeholders, or TODO-in-critical-path detected in the modified files. The fail-safe-deny `return false` branches in Configuration.cpp are intentional security behavior, not stubs. The `RestrictAddressFamilies`/`/dev` non-sandboxing decisions are documented in-code as deliberate.

### Human Verification Required

See `human_verification` frontmatter. Four items, all runtime/hardware-bound:
1. Polkit DENY leaves /etc/logid.cfg byte-unchanged (+ WARN journal line).
2. Authorized save writes /etc/logid.cfg under the hardened unit (ReadWritePaths decision).
3. A non-root `logiops`-group user reaches the bus and changes a setting on real hardware.
4. Hardening (RestrictAddressFamilies / empty caps / ProtectSystem=strict) does not break hidraw/uinput with a device attached.

### Gaps Summary

No code gaps. Every artifact, key link, and CONCERNS fix specified by the six plans exists, is substantive, is correctly wired, and the project builds clean, links polkit, and passes the hidpp bounds unit test. ACCESS-04 is fully verified. ACCESS-01/02/03 are statically complete; their final confirmation is the runtime/hardware behavior captured in the human-verification items. One out-of-box-usability item (the `logiops` group is not created by the build, REVIEW IN-05) is correctly deferred to packaging (Phase 9 / PKG-01) and fails closed in the meantime, so it does not weaken the security posture.

---

_Verified: 2026-05-30_
_Verifier: Claude (gsd-verifier)_
