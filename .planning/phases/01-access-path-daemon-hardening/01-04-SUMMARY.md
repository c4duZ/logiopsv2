---
phase: 01-access-path-daemon-hardening
plan: 04
subsystem: security
tags: [hidpp, bounds-check, oob-read, exception-handling, format-string, cpp20]

# Dependency graph
requires:
  - phase: 01-access-path-daemon-hardening (Plan 01)
    provides: RED hidpp_bounds CTest unit referencing hasHidppHeader()
provides:
  - hasHidppHeader() shared bounds helper in logid::backend::hidpp
  - HID report length-checks at all three raw filter lambdas (ACCESS-04)
  - Defense-in-depth length guards in Report::isError10/isError20
  - CONCERNS #1 (sliced exception) fixed via bare throw;
  - CONCERNS #5 (format-string log) fixed via "%s" format
  - CONCERNS #8 (release-mode assert on read length) replaced with explicit runtime guard
affects: [daemon-extension, gestures, action-wheel, any phase parsing HID reports]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Shared inline free fn (hasHidppHeader) for untrusted-input length validation"
    - "Bare throw; in central ExceptionHandler to preserve dynamic exception type"
    - "Explicit runtime guards (not assert) for security-relevant checks so they survive NDEBUG"

key-files:
  created:
    - .planning/phases/01-access-path-daemon-hardening/01-04-SUMMARY.md
  modified:
    - src/logid/backend/hidpp/Report.h
    - src/logid/backend/hidpp/Device.cpp
    - src/logid/backend/hidpp10/ReceiverMonitor.cpp
    - src/logid/backend/hidpp/Report.cpp
    - src/logid/util/ExceptionHandler.cpp
    - src/logid/logid.cpp
    - src/logid/backend/raw/RawDevice.cpp

key-decisions:
  - "hasHidppHeader() lives in Report.h as a free inline (snake_case) reusing Report::HeaderLength=4"
  - "No assert for security-relevant checks anywhere (locked decision); explicit runtime guards only"
  - "isError10/20 guards are belt-and-suspenders (ctor already pads _data to 20); the primary OOB surface is the raw filter lambdas"

patterns-established:
  - "Untrusted HID input is length-checked before any field indexing"
  - "Central error handler rethrows the active exception (throw;) instead of the sliced parameter"

requirements-completed: [ACCESS-04]

# Metrics
duration: 12min
completed: 2026-05-30
---

# Phase 1 Plan 04: HID Report Length-Checks + CONCERNS #1/#5/#8 Summary

**Closed the untrusted-HID out-of-bounds-read surface with a shared `hasHidppHeader()` guard at all three raw filter lambdas (turning Plan 01's RED `hidpp_bounds` unit GREEN), and landed the three same-theme security bug fixes: sliced exception, format-string log, and a release-stripped read-length assert.**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-05-30T18:00Z
- **Completed:** 2026-05-30
- **Tasks:** 3 completed
- **Files modified:** 7

## Accomplishments
- Added `logid::backend::hidpp::hasHidppHeader()` (inline, `size() >= Report::HeaderLength`) and guarded the `Device.cpp` `_raw_handler`, `ReceiverMonitor.cpp` connect, and `ReceiverMonitor.cpp` `waitForDevice` filter lambdas before any `report[...]` index — short/empty reports are now dropped, not indexed.
- Plan 01's RED `hidpp_bounds` CTest unit now compiles and **passes GREEN** (5/5 cases).
- Added defense-in-depth `_data.size()` guards to `Report::isError10`/`isError20`.
- Fixed CONCERNS #1, #5, #8 (see Deviations / details below).
- Project builds warning-clean under `-Wall -Wextra -Werror` (CI parity verified in a throwaway `build-werror` tree).

## Task Commits

Each task was committed atomically:

1. **Task 1: hasHidppHeader() helper + guard three raw filter lambdas** - `592c52a` (feat)
2. **Task 2: defense-in-depth guard in Report::isError10/isError20** - `b448816` (feat)
3. **Task 3: CONCERNS #1 / #5 / #8 fixes** - `fa6d386` (fix)

_Note: This plan's Task 1 was tdd-tagged but the RED unit was already authored by Plan 01; this plan supplied the GREEN implementation (the helper), so a single feat commit covers it._

## Files Created/Modified
- `src/logid/backend/hidpp/Report.h` - Added inline `hasHidppHeader(const std::vector<uint8_t>&)`.
- `src/logid/backend/hidpp/Device.cpp` - `hasHidppHeader` guard at top of `_raw_handler` filter lambda.
- `src/logid/backend/hidpp10/ReceiverMonitor.cpp` - guard at top of the connect filter and the `waitForDevice` filter lambdas (2 sites).
- `src/logid/backend/hidpp/Report.cpp` - `_data.size() <= Offset::Parameters + 1` early-return in `isError10`/`isError20`.
- `src/logid/util/ExceptionHandler.cpp` - `throw error;` → bare `throw;`; per-catch logs use `e.what()`; parameter marked `[[maybe_unused]]`.
- `src/logid/logid.cpp` - `logPrintf(WARN, e.what())` → `logPrintf(WARN, "%s", e.what())`.
- `src/logid/backend/raw/RawDevice.cpp` - replaced `assert(len <= max_data_length)` with an explicit `if (len < 0 || len > max_data_length) { logPrintf(...); continue; }` guard that survives NDEBUG.

## Verification

- `ctest --test-dir build -R hidpp_bounds --output-on-failure` → **100% tests passed, 0 failed (1/1)**. RED→GREEN confirmed.
- `cmake --build build` → clean (`-Wall -Wextra`).
- `cmake -S . -B build-werror -DCMAKE_CXX_FLAGS="-Werror -Wall -Wextra" && cmake --build build-werror` → clean (CI parity); temp tree removed.
- grep acceptance: `inline bool hasHidppHeader` present; `hasHidppHeader` in Device.cpp x1, ReceiverMonitor.cpp x2; `throw;` present and `throw error;` absent; `logPrintf(WARN, "%s", e.what())` present; `Ignoring HID read of unexpected length` present; `assert(len <= max_data_length)` absent.

## Deviations from Plan

None — plan executed exactly as written. All three tasks landed as specified with no auto-fixes required (build was warning-clean on first compile of each task).

## Audit Findings (required by plan output)

### ExceptionHandler caller audit (RESEARCH A10)
The sole caller of `ExceptionHandler::Default` is `src/logid/util/task.cpp:83`, invoked from inside an active `catch(std::exception& e)` block (task.cpp:80-84). An exception is therefore guaranteed in flight when `Default` runs, so the bare `throw;` is safe and will not call `std::terminate`. **No caller invokes `Default` outside an active exception** — the bare-throw fix is applied unconditionally and correctly. Mitigates T-01-04-05.

### logPrintf format-string sweep (CONCERNS #5)
`grep -rn 'logPrintf([A-Z_]*, [a-z_]' src/` returned exactly one offender: `src/logid/logid.cpp:97` (now fixed). No other call sites pass a runtime expression as the format argument; all other `logPrintf` calls use string-literal formats. `Report::setParams`'s internal `assert` and the Report ctor asserts were left intact (internal invariants, not attacker-controlled), per plan.

## Threat Model Coverage

| Threat ID | Disposition | Status |
|-----------|-------------|--------|
| T-01-04-01 (OOB read on short HID report) | mitigate | DONE — hasHidppHeader() at 3 lambdas; hidpp_bounds GREEN |
| T-01-04-02 (format-string injection) | mitigate | DONE — `"%s"` + tree sweep (1 fixed, 0 remaining) |
| T-01-04-03 (NDEBUG-elided length check) | mitigate | DONE — explicit runtime guard + continue |
| T-01-04-04 (sliced exception masks type) | mitigate | DONE — bare `throw;` preserves dynamic type |
| T-01-04-05 (bare throw outside catch → terminate) | mitigate | DONE — caller audit confirms sole caller is inside a catch |

No new security-relevant surface introduced beyond the threat model.

## Self-Check: PASSED
