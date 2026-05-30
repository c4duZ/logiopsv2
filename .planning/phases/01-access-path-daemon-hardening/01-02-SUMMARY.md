---
phase: 01-access-path-daemon-hardening
plan: 02
subsystem: infra
tags: [dbus, system-bus, policy, access-control, ipcgull, logiops-group]

# Dependency graph
requires:
  - phase: 01-access-path-daemon-hardening (Plan 01)
    provides: ACCESS-01/02/03 graceful-skip smoke harness (test/smoke/access-path.sh) that validates this policy at runtime
provides:
  - System-bus policy granting send_destination/receive_sender on pizza.pixl.LogiOps to group="logiops"
  - Name ownership (own=) kept root-only — group members can call/receive but cannot claim the name
  - The single D-Bus access seam every later GUI phase (2+) depends on
affects: [02-device-list, 03-config-client, GUI-dbus-client, 09-debian-packaging]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "D-Bus least-privilege relaxation: widen audience via <policy group=...> without widening name ownership (own= stays root-only)"
    - "Explicit allow-after-default-deny ordering so the group allow overrides the default <deny receive_sender>"

key-files:
  created: []
  modified:
    - src/logid/logiops-dbus.conf.in

key-decisions:
  - "Group policy grants only send_destination/receive_sender; own= deliberately omitted so only root claims pizza.pixl.LogiOps (locked decision, mitigates T-01-02-01 elevation)"
  - "The logiops group is created by packaging (Phase 9); smoke testing requires the tester to run groupadd logiops && usermod -aG logiops <user> and re-log-in"

patterns-established:
  - "Pattern: relax D-Bus audience by group, never by ownership — own= remains root-only"

requirements-completed: [ACCESS-01]

# Metrics
duration: 1min
completed: 2026-05-30
---

# Phase 1 Plan 02: D-Bus Group Policy Relaxation Summary

**System-bus policy now grants send/receive on `pizza.pixl.LogiOps` to the `logiops` group while keeping name ownership root-only — opening the bus seam every later GUI phase relies on.**

## Performance

- **Duration:** ~1 min
- **Started:** 2026-05-30T17:53:42Z
- **Completed:** 2026-05-30T17:54:22Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments
- Added a `<policy group="logiops">` block to `logiops-dbus.conf.in` granting `send_destination` and `receive_sender` for `pizza.pixl.LogiOps`.
- Kept `own="pizza.pixl.LogiOps"` exclusively under `user="root"` — group members cannot claim the bus name (mitigates elevation threat T-01-02-01).
- Preserved the default `<deny receive_sender>` so non-group, non-root users remain denied (mitigates info-disclosure threat T-01-02-03).
- Confirmed `cmake -S . -B build` still configures and `configure_file` copies the policy verbatim (with the group block) to `build/pizza.pixl.LogiOps.conf`, destined for `/usr/share/dbus-1/system.d/`.

## Task Commits

Each task was committed atomically:

1. **Task 1: Verify D-Bus group-policy syntax and add the group allow block** - `faaaaf5` (feat)

**Plan metadata:** (final docs commit below)

## Files Created/Modified
- `src/logid/logiops-dbus.conf.in` - Added `<policy group="logiops">` allow block (send_destination + receive_sender); `own=` left under `user="root"` only.

## Decisions Made
- None beyond the plan's locked decisions. Group gets call/receive only; `own=` stays root-only per locked decision.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## Verification Notes

- **`man dbus-daemon` was AVAILABLE** in the execution environment (RESEARCH Assumption A1 confirmed, no gap). Its "Configuration File" section confirms `group="group name or gid"` is a valid `<policy>` selector and that `send_destination`/`receive_sender` take a bus name. The assumed syntax is therefore verified, not just assumed.
- Automated acceptance verification all passed:
  - `grep -q 'group="logiops"'` → matches
  - group block grants both `send_destination` and `receive_sender`
  - group block contains no `own=` (`own=` appears only at line 11 under `user="root"`)
  - `user="root"` block still contains `allow own="pizza.pixl.LogiOps"`
  - file is well-formed XML (`xml.dom.minidom.parse` exits 0)
  - `cmake -S . -B build` configures cleanly (RC=0); configured `build/pizza.pixl.LogiOps.conf` contains the group block.

## User Setup Required

None at this phase. Runtime smoke-testing of ACCESS-01 (per VALIDATION.md) requires the tester to:
1. `groupadd logiops && usermod -aG logiops <user>` and re-log-in (the group is otherwise created by Phase 9 packaging).
2. `cmake --build build && make install`, then `systemctl reload dbus`.
3. As the re-logged-in `logiops` user, run `test/smoke/access-path.sh` → bus call should return success instead of `AccessDenied`.

## Next Phase Readiness
- The D-Bus access seam for non-root group members is in place; later GUI-client phases (2+) can now reach `pizza.pixl.LogiOps` as a normal user once the `logiops` group exists.
- Privileged writes (e.g. `Save`) remain intentionally ungated here — they are polkit-gated in Plan 06 (ACCESS-02, threat T-01-02-02 transferred).

## Self-Check: PASSED

- FOUND: `.planning/phases/01-access-path-daemon-hardening/01-02-SUMMARY.md`
- FOUND: `group="logiops"` in `src/logid/logiops-dbus.conf.in`
- FOUND: commit `faaaaf5`

---
*Phase: 01-access-path-daemon-hardening*
*Completed: 2026-05-30*
