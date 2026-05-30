---
phase: 01-access-path-daemon-hardening
plan: 03
subsystem: infra
tags: [systemd, sandboxing, hardening, hidraw, uinput, dbus, security]

# Dependency graph
requires:
  - phase: 01-access-path-daemon-hardening (Plan 02)
    provides: D-Bus policy relaxed to group=logiops — widened audience this plan sandboxes against
provides:
  - Hardened logid.service [Service] block (NoNewPrivileges, ProtectSystem=strict, empty CapabilityBoundingSet, RestrictAddressFamilies=AF_UNIX AF_NETLINK, ReadWritePaths=/etc/logid.cfg)
  - Verified-safe sandbox that preserves hidraw/uinput/udev (no PrivateDevices, no DeviceAllow)
affects: [01-06 polkit-gate save (ReadWritePaths pairing exercised there), 09-debian-packaging (ships this unit)]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "systemd sandboxing on a root daemon that must retain raw device access — drop everything except what hidraw/uinput/udev/D-Bus need"

key-files:
  created: []
  modified:
    - src/logid/logid.service.in

key-decisions:
  - "Keep User=root (hidraw/uinput need it); dedicated-user deferred per locked decision"
  - "Omit PrivateDevices/DeviceAllow/DevicePolicy — they would hide /dev/hidraw* and /dev/uinput"
  - "ReadWritePaths file-scoped to /etc/logid.cfg first; widen to /etc only if a real EROFS is observed under hardware test (Plan 06 Task 3)"

patterns-established:
  - "Pattern: pair ProtectSystem=strict with a minimal ReadWritePaths re-opening only the config path the daemon must persist"

requirements-completed: [ACCESS-03]

# Metrics
duration: 4min
completed: 2026-05-30
---

# Phase 1 Plan 03: systemd Hardening of logid.service Summary

**Hardened the root `logid` systemd unit with NoNewPrivileges, ProtectSystem=strict, an empty CapabilityBoundingSet, and AF_UNIX/AF_NETLINK-only sockets — while preserving hidraw/uinput access (no PrivateDevices) and keeping `Configuration::save()` writable via ReadWritePaths=/etc/logid.cfg.**

## Performance

- **Duration:** ~4 min
- **Started:** 2026-05-30T17:53:00Z
- **Completed:** 2026-05-30T17:57:19Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments
- Added the locked Phase 1 hardening set to the `[Service]` block of `logid.service.in`.
- Dropped all capabilities (empty `CapabilityBoundingSet=`), mounted the system read-only (`ProtectSystem=strict`), and restricted sockets to `AF_UNIX` (D-Bus) + `AF_NETLINK` (udev) — nothing else.
- Re-opened the single config path (`ReadWritePaths=/etc/logid.cfg`) so `save()` still works under `ProtectSystem=strict`.
- Kept `User=root` and deliberately omitted `PrivateDevices`/`DeviceAllow`/`DevicePolicy` so `/dev/hidraw*` and `/dev/uinput` remain visible.

## Directive verification (man systemd.exec)

`man systemd.exec` **was available** and was used to confirm RESEARCH assumptions A5/A6/A7 before editing:
- `CapabilityBoundingSet=` — empty value resets the bounding set; with no caps added, ALL capabilities are dropped. Confirmed.
- `ProtectSystem=strict` — mounts the whole FS read-only; `ReadWritePaths=` is the documented escape hatch for specific paths. Confirmed (man explicitly notes `ReadWritePaths=` re-grants write access under `strict`).
- `RestrictAddressFamilies=` — allow-lists socket families (`AF_UNIX`, `AF_NETLINK`, etc.). Confirmed.
- `PrivateDevices=` — sets up a private `/dev/` with only API pseudo-devices, which would hide `/dev/hidraw*` and `/dev/uinput`. Confirmed forbidden; correctly omitted.

No documentation gap remains for this plan.

## Task Commits

Each task was committed atomically:

1. **Task 1: Verify directives and harden the [Service] block** - `83fd745` (feat)

**Plan metadata:** committed separately with SUMMARY.md, STATE.md, ROADMAP.md, REQUIREMENTS.md.

## Files Created/Modified
- `src/logid/logid.service.in` - Added the Phase 1 hardening directives to the `[Service]` block (NoNewPrivileges, ProtectSystem=strict, ProtectHome, PrivateTmp, ReadWritePaths=/etc/logid.cfg, RestrictAddressFamilies=AF_UNIX AF_NETLINK, empty CapabilityBoundingSet, ProtectControlGroups/KernelTunables/KernelLogs/KernelModules, RestrictRealtime, LockPersonality, MemoryDenyWriteExecute), with inline comments explaining the User=root retention and the device-sandboxing omissions.

## Decisions Made
- **Keep `User=root`** — hidraw/uinput require it; dedicated-user is deferred per the locked phase decision.
- **Omit `PrivateDevices`/`DeviceAllow`/`DevicePolicy`** — any of them would hide the dynamic `/dev/hidraw*` nodes and `/dev/uinput`, breaking the daemon's purpose.
- **ReadWritePaths file-scoped first** — start with `/etc/logid.cfg`; only widen to `/etc` if a real hardware test (Plan 06 Task 3) shows `save()` failing `EROFS` because libconfig writes via temp+rename in `/etc`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Reworded the "do not add" comment so the task's own verification passes**
- **Found during:** Task 1
- **Issue:** The plan's `<action>` suggested an inline comment containing the literal token `PrivateDevices=yes`. That comment satisfied no functional need but tripped the plan's own `! grep -q 'PrivateDevices'` acceptance assertion (which matches the literal string anywhere in the file, including comments), making the verification command exit non-zero even though no active `PrivateDevices=` directive exists.
- **Fix:** Reworded the explanatory comment to "Intentionally NOT sandboxing /dev (no private-devices mount, no DeviceAllow=, no DevicePolicy=closed)" — preserving the safety documentation while removing the literal `PrivateDevices` token, so the `! grep` assertion passes as written.
- **Files modified:** src/logid/logid.service.in
- **Verification:** Full task assertion chain returns `ALL ASSERTIONS OK`; `grep -n '^PrivateDevices'` returns nothing (no active directive).
- **Committed in:** `83fd745` (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 verification/correctness)
**Impact on plan:** Cosmetic comment wording only; no change to any active directive or sandbox semantics. No scope creep.

## Issues Encountered
- The plan calls for `cmake -S . -B build` to confirm `configure_file` still copies the unit. A full top-level configure requires all daemon dev libraries and submodule clones, which would be a long, unrelated operation on this host. Instead, the `configure_file` substitution was validated in isolation (a minimal throwaway CMake project running only `configure_file(logid.service.in ...)`): it configured cleanly and rendered `ExecStart=/usr/bin/logid` with the full hardening block intact. Additionally, `systemd-analyze verify` on the rendered unit reported **no syntax errors on any hardening directive** (its only complaint was that `/usr/bin/logid` is not installed on this build host — expected). The directives parse and substitute correctly.

## Open Question (RESEARCH Pitfall 2 / A7) — ReadWritePaths scope

`ReadWritePaths=/etc/logid.cfg` is file-scoped. If, under the live hardened unit, `Configuration::save()` fails `EROFS` because libconfig writes via temp-file + rename inside `/etc` (the temp file would land on the read-only `/etc` mount), widen to `ReadWritePaths=/etc`, re-install, `systemctl daemon-reload` + restart, and re-test.

**Plan 06 Task 3 authorized-save outcome:** Not available at this write time — Plan 06 has not executed yet. The runtime authenticated-Save check that exercises this pairing lives there; the chosen path (file-scoped vs `/etc`) should be confirmed and, if widened, recorded when Plan 06 Task 3 runs on real hardware.

## Runtime verification (deferred to real hardware)
The grep/configure/analyze checks above are all that can run on a build host. The following are deferred per VALIDATION.md (require an installed unit + a Logitech device):
- `bash test/smoke/hardening.sh` — assert directives via `systemctl show` and print the `systemd-analyze security logid` exposure score.
- hidraw/uinput non-regression — confirm a device still enumerates and a remap still synthesizes input under the hardened unit.
- Authorized-save `EROFS` check — Plan 06 Task 3 (see Open Question above).

## Next Phase Readiness
- ACCESS-03 hardening is in place in the unit source and ready to ship; verifiable via `systemctl show` / `systemd-analyze security` once installed.
- No blockers for Plans 04/05/06. Plan 06 Task 3 must cross-check the ReadWritePaths pairing on real hardware and record the final path choice.

## Self-Check: PASSED

- FOUND: src/logid/logid.service.in
- FOUND: .planning/phases/01-access-path-daemon-hardening/01-03-SUMMARY.md
- FOUND: commit 83fd745

---
*Phase: 01-access-path-daemon-hardening*
*Completed: 2026-05-30*
