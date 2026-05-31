---
phase: 04-fine-grained-gesture-control
plan: 01
subsystem: daemon-gesture-engine
tags: [gesture, daemon, security-hardening, GEST-02, GEST-03]
requires:
  - tests/phase4 CTest target (04-00)
provides:
  - defaults::gesture_interval constant
  - IntervalGesture granularity fix (default + divide-guard + arg-name)
  - hardened GestureAction::setGesture (lazy-init gestures optional, allowlist-safe)
affects:
  - src/logid/actions/gesture/IntervalGesture.cpp
  - src/logid/actions/GestureAction.cpp
  - GUI Plan 02 (reads SetThreshold arg name + relies on crash-free SetGesture)
tech-stack:
  added: []
  patterns:
    - value_or(defaults::gesture_interval) fallback for unset OnInterval interval
    - lazy std::optional::emplace() guard before map access at D-Bus trust boundary
key-files:
  created:
    - .planning/phases/04-fine-grained-gesture-control/deferred-items.md
  modified:
    - src/logid/Configuration.h
    - src/logid/actions/gesture/IntervalGesture.cpp
    - src/logid/actions/GestureAction.cpp
    - src/logid/actions/gesture/AxisGesture.cpp
decisions:
  - defaults::gesture_interval = 120 (matches gesture_math_test's documented one-flick-one-step calibration; value is Claude's discretion per CONTEXT/A1, pending MX Master 4 UAT)
  - OnRelease/OnThreshold interface_name collision left as-is per plan (GUI Plan 02 canonicalizes mode 2 to OnRelease); verified unknown type surfaces as clean D-Bus error
metrics:
  duration: ~4m
  completed: 2026-05-31
  tasks: 2
  files: 4
---

# Phase 4 Plan 01: Daemon Gesture Fix/Harden Summary

Fixed the GEST-02/03 granularity defect in `IntervalGesture` (unset interval now falls back to a sane `defaults::gesture_interval=120` instead of never firing; divide-by-zero guarded) and hardened `GestureAction::setGesture` so a group-`logiops` D-Bus client cannot crash the root daemon via a `nullopt` gestures map or untrusted direction/type strings.

## What Was Built

**Task 1 — IntervalGesture granularity (GEST-02/03):**
- Added `defaults::gesture_interval = 120` to `Configuration.h` next to `gesture_threshold = 50`, documented as the leftmost granularity-slider stop (one flick ≈ one step, pending hardware UAT).
- `IntervalGesture::move` now reads `const int interval = _config.interval.value_or(defaults::gesture_interval)` and early-returns only when `interval <= 0`, replacing the old `if (!_config.interval.has_value()) return;` that made an unset-interval OnInterval gesture dead. The fire-on-increase `_interval_pass_count` accounting was preserved unchanged (it already fires exactly once per boundary).
- Fixed the `SetThreshold` registration arg-name from `{"interval"}` to `{"threshold"}` so introspection (read by the GUI) advertises the correct name.

**Task 2 — setGesture hardening + log arg (security):**
- `GestureAction::setGesture` now lazily `emplace()`s `_config.gestures` if it is `nullopt` before the `[dir_name]` access, closing the `std::bad_optional_access` DoS (T-04-01-01) on the `Button.SetAction("Gesture")` → `SetGesture(dir, mode)` sequence.
- Documented the untrusted-direction allowlist path (`toDirection` throws `std::invalid_argument` → clean D-Bus error, never an abort; no `std::stoi`) and the unknown-type restore path (config-driven `makeGesture` restore + `std::invalid_argument` rethrow leaves the node on a valid interface).
- Fixed `AxisGesture.cpp` `logPrintf(WARN, "Invalid axis %s.")` missing its argument → `logPrintf(WARN, "Invalid axis %s.", axis.c_str())` (CONCERNS #5 format/arg discipline).

## Verification

- `ctest --test-dir build -R phase4_gesture_math` — PASS (GREEN). One flick = one fire; M=360/I=120 = 3 fires; unset interval repeats at the default.
- `cmake --build build --target logid` — compiles -Werror clean (no warnings/errors).
- `grep` acceptance criteria for both tasks satisfied (gesture_interval present, value_or default present, SetThreshold `{"threshold"}`, lazy-init guard present, AxisGesture `%s` arg present, no real `std::stoi`/`atoi` in the gesture path — the only match was the literal text "No std::stoi" in a comment).

## Deviations from Plan

### Auto-fixed Issues

None requiring code beyond the plan.

### Plan-value adjustment (documented decision, not a deviation)

The plan's action text suggested `defaults::gesture_interval = 50`, but the plan also explicitly delegates the exact value to "Claude's discretion per CONTEXT and confirmed by hardware UAT" and the Wave-0 `gesture_math_test.cpp` documents the calibrated "one comfortable flick = one logical step" value as **120** (`kExpectedDefaultInterval = 120`). I set the daemon constant to **120** to match the test's documented contract and the one-flick-one-step intent. The test is self-contained (uses a local replica constant), so this is a forward-looking alignment with the documented calibration; the precise integer remains subject to MX Master 4 UAT (Assumption A1).

## Known Stubs

None. All changes are wired into the live daemon paths.

## Out-of-Scope (Deferred)

- `phase4_gesture_model` CTest target does not build: `tests/phase4/GestureModelTest.cpp` includes `GestureModel.h`, which does not exist yet (a Wave-0 scaffold for the GUI `GestureModel` built by a later GUI plan). Not caused by this plan's daemon-only changes. Logged in `deferred-items.md`. The in-scope `phase4_gesture_math` and `logid` targets build/pass.

## Commits

- `b6005da` fix(04-01): fix IntervalGesture granularity defect + add defaults::gesture_interval (GEST-02/03)
- `92240f4` fix(04-01): harden GestureAction::setGesture against untrusted input + fix AxisGesture log arg
- `3094782` chore(04-01): log out-of-scope phase4_gesture_model build gap to deferred-items

## Self-Check: PASSED

All 6 modified/created files present on disk; all 3 commits present in git history.
