---
phase: 01-access-path-daemon-hardening
plan: 01
subsystem: testing
tags: [ctest, cmake, bash, smoke-test, dbus, polkit, systemd, hidpp, tdd, red-green]

# Dependency graph
requires: []
provides:
  - "CTest enabled in the root build (BUILD_TESTING option + enable_testing())"
  - "test/ subtree with a registered hidpp_bounds CTest unit (RED until Plan 04)"
  - "RED hidpp_bounds_test.cpp exercising 5 header-length cases of hasHidppHeader()"
  - "test/smoke/access-path.sh (ACCESS-01 runtime smoke)"
  - "test/smoke/polkit-deny.sh (ACCESS-02 runtime smoke, sha256 before/after a denied save)"
  - "test/smoke/hardening.sh (ACCESS-03 runtime smoke, systemctl show / systemd-analyze)"
  - "test/README.md documenting manual prerequisites and run commands"
affects: [01-02, 01-03, 01-04, 01-05, 01-06]

# Tech tracking
tech-stack:
  added: [CTest, bash smoke-test scripts]
  patterns:
    - "RED-first CTest unit referencing a symbol a later plan adds (observable RED->GREEN)"
    - "Graceful-skip smoke scripts (exit 0 on missing prerequisite, non-zero only on real failure)"

key-files:
  created:
    - test/CMakeLists.txt
    - test/hidpp_bounds_test.cpp
    - test/README.md
    - test/smoke/access-path.sh
    - test/smoke/polkit-deny.sh
    - test/smoke/hardening.sh
  modified:
    - CMakeLists.txt

key-decisions:
  - "Header-only test target (no link to logid): the unit needs only Report.h constants + the free hasHidppHeader() inline, avoiding libconfig/udev/evdev coupling"
  - "Dependency-free C++ test with a tiny main() returning non-zero on failure instead of pulling in gtest (the tree has no test framework)"
  - "Each of the 5 cases makes its own explicit hasHidppHeader() call (one call per case) per the acceptance criterion"
  - "Smoke scripts degrade gracefully (skip + exit 0) when prerequisites are absent; they assert-fail only on a true violation"

patterns-established:
  - "RED baseline: hidpp_bounds compiles RED until Plan 04 (ACCESS-04) adds hasHidppHeader()"
  - "ACCESS-0x smoke scripts named with their requirement in the header comment"

# This Wave 0 plan builds the VERIFICATION HARNESS for these requirements; the
# implementations land in plans 02-06. Requirements are intentionally left OPEN
# until their implementing plan completes (they are re-declared in those plans'
# frontmatter). Listing here documents the harness coverage, not completion.
requirements-harnessed: [ACCESS-01, ACCESS-02, ACCESS-03, ACCESS-04]
requirements-completed: []

# Metrics
duration: 3min
completed: 2026-05-30
---

# Phase 1 Plan 01: Wave 0 Test Harness Summary

**CTest-driven RED hidpp-bounds unit plus three graceful-skip ACCESS-01/02/03 bash smoke scripts, giving every later Phase 1 plan a concrete runnable verification target.**

## Performance

- **Duration:** ~3 min
- **Started:** 2026-05-30T17:47:05Z
- **Completed:** 2026-05-30T17:50:24Z
- **Tasks:** 3
- **Files modified:** 7 (6 created, 1 modified)

## Accomplishments
- Wired `BUILD_TESTING` + `enable_testing()` + `add_subdirectory(test)` into the root `CMakeLists.txt`, after `add_subdirectory(src/logid)` so the test can reference the daemon's include dirs.
- Created a header-only `hidpp_bounds` CTest target (`Test #1`) registered via `add_test(NAME hidpp_bounds ...)`, configuring cleanly.
- Authored the RED `hidpp_bounds_test.cpp`: a dependency-free unit exercising the 5 header-length cases (empty / 1 / 3 / 4 / 7 bytes) of `logid::backend::hidpp::hasHidppHeader()` — intentionally fails to compile until Plan 04 adds the helper.
- Authored three executable, `bash -n`-clean smoke scripts (`access-path.sh`, `polkit-deny.sh`, `hardening.sh`) covering ACCESS-01/02/03, each skipping cleanly when prerequisites are absent.
- Documented manual prerequisites (`groupadd`/`usermod`, running daemon, polkit agent) and run commands in `test/README.md`.

## Task Commits

Each task was committed atomically:

1. **Task 1: Wire enable_testing() and a test/ subtree into the build** - `0458af4` (feat)
2. **Task 2: Author the RED hidpp bounds unit** - `9d6c058` (test — RED baseline)
3. **Task 3: Author the three ACCESS-01/02/03 smoke scripts** - `f6e21ca` (test)

_Note: Task 2 is a single RED commit (no GREEN/refactor here — the GREEN transition is Plan 04's deliverable)._

## Files Created/Modified
- `CMakeLists.txt` - Added `BUILD_TESTING` option, `enable_testing()`, `add_subdirectory(test)` after the src adds.
- `test/CMakeLists.txt` - Registers the header-only `hidpp_bounds_test` executable (CXX 20) and the `hidpp_bounds` CTest.
- `test/hidpp_bounds_test.cpp` - RED unit: 5 header-length cases against `hasHidppHeader()`; non-zero exit on any failure.
- `test/README.md` - Harness docs: smoke-script index, manual prerequisites, quick-run commands.
- `test/smoke/access-path.sh` - ACCESS-01: non-root logiops-group D-Bus introspection, asserts no `AccessDenied`.
- `test/smoke/polkit-deny.sh` - ACCESS-02: sha256 of `/etc/logid.cfg` before/after a denied `Save` + journal audit; never writes the config.
- `test/smoke/hardening.sh` - ACCESS-03: `systemctl show` / `systemd-analyze` assertions on the four hardening directives.

## Decisions Made
- **Header-only test target** (no link to the `logid` target): the unit only needs `Report.h`'s pure constants and the free `hasHidppHeader()` inline, keeping the test decoupled from libconfig/udev/evdev.
- **No gtest** — a tiny `main()` returning non-zero on failure is sufficient for CTest and matches a tree with no existing test framework.
- **One `hasHidppHeader()` call per case** — restructured from a single shared call site so the acceptance criterion (>= 5 calls, one per case) is satisfied and each case is self-evident.
- **Graceful-skip smoke scripts** — skip (exit 0) on a missing prerequisite, fail (non-zero) only on a true assertion violation (mitigates threats T-01-01 tampering and T-01-02 CI-hang).

## Deviations from Plan

None - plan executed exactly as written. (The minor restructure of the test to make one `hasHidppHeader()` call per case was to satisfy an explicit acceptance criterion, not a deviation from the plan's intent.)

## Issues Encountered
None blocking. Two observed-but-expected states worth recording:
- **`hidpp_bounds` is RED by design.** Building the target fails with `'hasHidppHeader' is not a member of 'logid::backend::hidpp'` — exactly the intended RED baseline. It turns GREEN once **Plan 04 (ACCESS-04)** adds the inline to `Report.h`. Per `<sequential_execution>` instructions, the helper was NOT implemented here.
- **`hardening.sh` reports FAIL (exit 1) on this machine** because the installed `logid` unit is not yet hardened (`NoNewPrivileges=no`, `ProtectSystem=no`, full `CapabilityBoundingSet`, exposure 9.6 UNSAFE). This is the correct pre-hardening reading; the script turns green once **Plan 03 (ACCESS-03)** lands the systemd directives. `polkit-deny.sh` passed its config-unchanged assertion (machine has a `logid` unit + `/etc/logid.cfg`); `access-path.sh` skipped cleanly (current user not in the `logiops` group).

## Known Stubs

None. The harness is the deliverable; the RED unit and the pre-hardening `hardening.sh` failure are intended baselines, not stubs — both are documented above with the plan that turns them green (04 and 03 respectively).

## User Setup Required
None for this plan. Running the smoke scripts later requires the manual prerequisites documented in `test/README.md` (create the `logiops` group, add the user and re-login, a running `logid`, an interactive polkit agent).

## Next Phase Readiness
- Every Wave 1/2 plan in Phase 1 now has a concrete verification target instead of "MISSING":
  - 01-02 (ACCESS-01) → `test/smoke/access-path.sh`
  - 01-03 (ACCESS-03) → `test/smoke/hardening.sh`
  - 01-04 (ACCESS-04) → `ctest -R hidpp_bounds` (will flip RED→GREEN when the helper lands)
  - 01-06 (ACCESS-02) → `test/smoke/polkit-deny.sh`
- No blockers introduced. The RED `hidpp_bounds` target intentionally fails to build; if a future CI gate compiles all targets, it must tolerate this until Plan 04, or build `hidpp_bounds_test` only from Plan 04 onward.

---
*Phase: 01-access-path-daemon-hardening*
*Completed: 2026-05-30*

## Self-Check: PASSED

- Files: all 6 harness files + SUMMARY.md present on disk.
- Commits: 0458af4 (Task 1), 9d6c058 (Task 2), f6e21ca (Task 3) all in git history.

## Requirements Status Note

This Wave 0 plan builds the **verification harness** for ACCESS-01/02/03/04 — it
does NOT implement them. `requirements mark-complete` initially checked all four
off; that was reverted in `.planning/REQUIREMENTS.md` (back to `[ ]` / `Pending`)
because the implementations belong to plans 01-02 (ACCESS-01), 01-03 (ACCESS-03),
01-04 (ACCESS-04), and 01-05/01-06 (ACCESS-02). Each of those plans re-declares
the requirement in its own frontmatter and will mark it complete when it lands.
