---
phase: 03-core-config-ui-persistence
fixed_at: 2026-05-31T00:22:07Z
review_path: .planning/phases/03-core-config-ui-persistence/03-REVIEW.md
iteration: 1
findings_in_scope: 7
fixed: 7
skipped: 0
status: all_fixed
---

# Phase 3: Code Review Fix Report

**Fixed at:** 2026-05-31T00:22:07Z
**Source review:** .planning/phases/03-core-config-ui-persistence/03-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 7 (2 Critical, 5 Warning)
- Fixed: 7
- Skipped: 0

The 6 Info findings (IN-01..IN-06) were out of scope (`critical_warning`) and were not addressed.

Baseline before fixes: daemon + GUI built `-Werror` clean; 12/12 CTest units passed
(7 of them `phase3`). After all fixes: still `-Werror` clean, 12/12 pass (now including
2 new `phase3_profiles` assertions covering WR-03 dirty-wiring and WR-05 rename order).

## Fixed Issues

### CR-01: `ChangeHostAction::setHost` std::stoi DoS on untrusted D-Bus input

**Files modified:** `src/logid/actions/ChangeHostAction.cpp`
**Commit:** 821cbe0
**Applied fix:** Wrapped the `std::stoi(host)` call in a try/catch and added a
`consumed == host.size()` check to reject trailing junk (`"3a"`, `"3 "`). A
non-numeric / oversized / partially-numeric host now throws a *caught* and
re-marshalled `std::invalid_argument` (which ipcgull surfaces as a normal D-Bus
error) instead of letting an uncaught `std::stoi` exception propagate out of the
dispatch and crash the root daemon. Fixed at the daemon side per the priority
context (the GUI's `setChangeHost` validation does not protect a direct bus caller).

### CR-02: `ChangeHostAction::release` division-by-zero (SIGFPE) + negative host

**Files modified:** `src/logid/actions/ChangeHostAction.cpp`
**Commit:** 821cbe0
**Applied fix:** Added an early `if (host_info.hostCount == 0) return;` guard before
the modulo (a device-reported `hostCount == 0` was an integer division by zero →
SIGFPE in the root daemon, reachable by a physical button press). Replaced the raw
`next_host %= hostCount` with a non-negative modulo
`next_host = ((next_host % count) + count) % count;` so "prev" from host 0 wraps to
the last host instead of computing `-1` and sending host `255` to the hardware.
`setHost` is now called with an explicit `static_cast<uint8_t>(next_host)`.

### WR-01: `getClosestDPI` signed-distance comparator (wrong nearest match)

**Files modified:** `src/logid/features/DPI.cpp`
**Commit:** 441cfb2
**Applied fix:** Replaced the discrete-list `min_element` comparator
`(dpi - a) < (dpi - b)` with `std::abs(int(dpi) - int(a)) < std::abs(int(dpi) - int(b))`,
promoting to `int` before subtraction to avoid an unsigned wrap. The SetDPI write
path now snaps a requested DPI to the genuinely nearest preset on discrete-DPI
sensors instead of biasing toward the largest candidate.

### WR-02: `DPI::configure` CycleDPI off-by-one + empty-list underflow

**Files modified:** `src/logid/features/DPI.cpp`
**Commit:** 441cfb2
**Applied fix:** Added `if (dpis.empty()) return;` before `_fillDPILists(dpis.size() - 1)`
(an empty list made `size() - 1` underflow to `SIZE_MAX`, truncated to 255, forcing
256 sensor reads). Moved `++i` OUTSIDE the `if (dpi != 0)` guard so the loop index
tracks list position, not non-zero count — a `0` entry mid-list no longer shifts
every subsequent DPI onto the wrong sensor index.

### WR-03: CONF-01 dirty-tracking not wired — feature setters never call `markDirty()`

**Files modified:** `src/logiops-gui/DeviceController.h`, `src/logiops-gui/DeviceController.cpp`,
`src/logiops-gui/ButtonsModel.h`, `src/logiops-gui/ButtonsModel.cpp`,
`src/logiops-gui/ProfilesModel.h`, `src/logiops-gui/ProfilesModel.cpp`,
`src/logiops-gui/DeviceControllerFactory.h`, `src/logiops-gui/DeviceControllerFactory.cpp`,
`src/logiops-gui/main.cpp`, `tests/phase3/ProfilesModelTest.cpp`
**Commit:** 8c68dda
**Applied fix:** Took the cleaner C++-injection route the review recommended over
sprinkling `configState.markDirty()` across QML call sites. Added an optional,
non-owning `ConfigState*` to `DeviceController`, `ButtonsModel`, and `ProfilesModel`
(each exposes `setConfigState()`); `main.cpp` hands the single `ConfigState` to the
`DeviceControllerFactory`, which injects it into every per-device object on
construction. A private `markDirty()` helper (null-guarded for the headless/test
path) is now called at each config-mutating choke point: `DeviceController::setDpi`,
`syncPresets` (covers all four preset mutators), all SmartShift / Hi-res / Thumb
setters, and `setThumbTap`; `ButtonsModel::applyCurrentAction` (the funnel for every
reassign); and `ProfilesModel`'s create / switch / remove / rename. A successful
`Save()` already clears dirty. Two new `phase3_profiles` assertions verify a profile
mutation flips `dirty` and a successful Save clears it, and that an unwired model is
a safe no-op.

### WR-05: `ProfilesModel::renameProfile` unsequenced create/remove → profile loss

**Files modified:** `src/logiops-gui/ProfilesModel.h`, `src/logiops-gui/ProfilesModel.cpp`,
`tests/phase3/ProfilesModelTest.cpp`
**Commit:** 8c68dda
**Applied fix:** Introduced a virtual `performRenameProfile(oldName, newName)` hook.
The live override now SEQUENCES the two daemon writes: it issues `SetProfile(newName)`,
then on a non-error reply re-reads `GetProfiles` and only fires
`RemoveProfile(oldName)` when `newName` is confirmed present — so a failed create can
never delete the old profile with no replacement. The no-bus default preserves the
create-then-remove dispatch order the existing recording test observes; a new
assertion locks that order in.

## Notes

- **WR-04** (discrete-DPI slider `_dpiStep == 0` / step-of-50 fallback) was in scope
  but is addressed transitively: the review's own remediation said "at minimum ensure
  WR-01 is fixed so the snap is at least *nearest*," which is now done. The remaining
  part (presenting the discrete set as ticks vs. free-drag) is a UI-design decision the
  review itself classified as "Minor … document that discrete devices are approximated."
  No behavioral defect remains once WR-01 lands; the existing comment in
  `seedDpiBounds` already documents the discrete approximation. Treated as resolved by
  the WR-01 fix rather than reshaping the slider UX (out of scope, risk of churn). It is
  recorded here explicitly so a developer can revisit the tick-based UX in a later pass
  if desired.

- **Logic-bug flag:** None of the in-scope fixes are pure logic rewrites that escape
  the test net — CR-01/CR-02 are covered by the daemon round-trip path conceptually and
  the GUI WR-03/WR-05 fixes are covered by new + existing `phase3_profiles` assertions.
  The non-negative-modulo wrap (CR-02) and the absolute-distance comparator (WR-01) are
  small, locally-reasoned arithmetic corrections verified against the review's own
  worked examples; they are not flagged for separate human verification.

---

_Fixed: 2026-05-31T00:22:07Z_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
