---
phase: 01-access-path-daemon-hardening
fixed_at: 2026-05-30T00:00:00Z
review_path: .planning/phases/01-access-path-daemon-hardening/01-REVIEW.md
iteration: 1
findings_in_scope: 3
fixed: 3
skipped: 0
status: all_fixed
---

# Phase 1: Code Review Fix Report

**Fixed at:** 2026-05-30
**Source review:** .planning/phases/01-access-path-daemon-hardening/01-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 3 (WR-01, WR-02, WR-03 — Info findings IN-01..IN-06 out of scope under `critical_warning`)
- Fixed: 3
- Skipped: 0
- Build: clean with `cmake --build build` (project uses `-Werror`)

## Fixed Issues

### WR-02: `get_phys` / `get_name` underflow `len - 1` when ioctl returns 0

**Files modified:** `src/logid/backend/raw/RawDevice.cpp`
**Commit:** b5c2693
**Applied fix:** Added the suggested `if (len <= 0) return {};` guard to both `get_phys` (after the `HIDIOCGRAWPHYS` ioctl) and `get_name` (after the `HIDIOCGRAWNAME` ioctl), before the `std::string{buf, static_cast<size_t>(len) - 1}` construction. This is the highest-value fix: it closes a real out-of-bounds read (crash / info leak) reachable when a kernel/driver returns an empty string (`len == 0`) for virtual or malformed hidraw nodes — exactly the untrusted-hardware-enumeration surface this phase targets. Verified: re-read confirms guards present and surrounding code intact; full build links clean.

### WR-03: `_readReports` length guard has an unreachable lower bound and a wrong upper-bound type comparison

**Files modified:** `src/logid/backend/raw/RawDevice.cpp`
**Commit:** b6166a2
**Applied fix:** Replaced the dead guard `if (len < 0 || len > max_data_length)` with `if (len <= 0 || static_cast<size_t>(len) > sizeof(buf))` plus an explanatory comment. The new bound is tied to the actual destination buffer (`sizeof(buf)`) rather than the `int` count constant that already bounds the `read()`, and `<= 0` now skips zero-length reads that would otherwise build an empty vector indexed past downstream. Verified: re-read confirms the new condition and comment; full build links clean. Note: this is a robustness/defensive-bound change (not a behavioral logic change for normal hardware), so no separate human-logic verification flag is needed — the guard rejects only genuinely malformed reads.

### WR-01: Polkit blocking `_sync` call can stall the GLib dispatch thread (server_lock held across prompt)

**Files modified:** `src/logid/Configuration.cpp`
**Commit:** ead5057
**Applied fix:** Documentation-only. Per the phase guidance, WR-01 is an accepted design tradeoff (RESEARCH A9): the `_sync` polkit call runs on the dispatch thread under ipcgull's `server_lock` (recursive_mutex), which is therefore held for the full duration of the interactive prompt and can stall concurrent `emit_signal` / `add_interface` / `drop_interface` (e.g. a device hotplug). The review's suggested mitigations (drop `server_lock` before the call, or move to async `polkit_authority_check_authorization` with a deferred D-Bus reply) both require restructuring the synchronous same-thread dispatch/locking model that `save()`'s `current_caller()` depends on — explicitly out of scope and flagged as risky. No clean, low-risk code mitigation exists without that restructuring, so the residual concern was documented as accepted-deferred by expanding the existing in-code NOTE. No behavioral or locking change; full build links clean.

## Skipped Issues

None.

---

_Fixed: 2026-05-30_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
