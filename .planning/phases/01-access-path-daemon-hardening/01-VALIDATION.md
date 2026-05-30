---
phase: 1
slug: access-path-daemon-hardening
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-30
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> NOTE: the logiops daemon ships with effectively no automated test suite (see `.planning/codebase/TESTING.md`). This phase is security/infrastructure — most criteria are verified by shell smoke-tests against a running daemon plus one small C++ unit for the HID bounds helper. Wave 0 installs that minimal harness.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest (CMake) for the C++ bounds-helper unit + shell smoke-test scripts for D-Bus/polkit/systemd |
| **Config file** | `CMakeLists.txt` (enable_testing); `test/` dir (Wave 0 creates) |
| **Quick run command** | `cmake --build build && ctest --test-dir build --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --output-on-failure && bash test/smoke/access-path.sh` |
| **Estimated runtime** | ~60 seconds (compile dominates; smoke-test needs the daemon running) |

---

## Sampling Rate

- **After every task commit:** `cmake --build build` (must compile clean, `-Werror`) + `ctest` if a unit exists
- **After every plan wave:** full suite (ctest + smoke script where the daemon is runnable)
- **Before `/gsd-verify-work`:** full suite green; manual smoke-tests below executed once on real hardware/session
- **Max feedback latency:** ~60 seconds

---

## Per-Task Verification Map

> Filled/refined by the planner per task. Anchor criteria below derive from the phase success criteria.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 1-xx-xx | xx | 1 | ACCESS-01 | — | non-root logiops-group user reaches the bus | smoke | `bash test/smoke/access-path.sh` | ❌ W0 | ⬜ pending |
| 1-xx-xx | xx | 1 | ACCESS-02 | save-config | polkit deny leaves /etc/logid.cfg unchanged | smoke | `bash test/smoke/polkit-deny.sh` | ❌ W0 | ⬜ pending |
| 1-xx-xx | xx | 1 | ACCESS-03 | sandbox | hardening in effect | smoke | `systemd-analyze security logid \| tail -1` | ❌ W0 | ⬜ pending |
| 1-xx-xx | xx | 1 | ACCESS-04 | OOB-HID | short report → no OOB read | unit | `ctest --test-dir build -R hidpp_bounds` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `test/smoke/access-path.sh` — non-root (logiops group) `gdbus`/`busctl` call to `pizza.pixl.LogiOps` changes a device setting (ACCESS-01)
- [ ] `test/smoke/polkit-deny.sh` — trigger `save()`, deny polkit, assert `/etc/logid.cfg` byte-identical (sha256) before/after (ACCESS-02)
- [ ] `test/CMakeLists.txt` + `test/hidpp_bounds_test.cpp` — unit feeding short/empty reports through the new `hasHidppHeader()` bounds guard (ACCESS-04)
- [ ] `enable_testing()` wired into root `CMakeLists.txt`

*Existing infrastructure covers none of the phase requirements — Wave 0 establishes the harness.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| polkit auth prompt actually appears in a real session | ACCESS-02 | Needs an interactive polkit agent + logged-in session | Run a config save from a non-root session; confirm the admin-auth prompt appears; authenticate → write succeeds, cancel → write blocked |
| systemd sandbox doesn't break real hidraw/uinput | ACCESS-03 | Needs real Logitech hardware attached | After hardening, restart `logid`, confirm a device is still detected and a remap still synthesizes input |
| Full hardening score | ACCESS-03 | Reads live unit | `systemd-analyze security logid` — exposure score improved vs baseline |

*Hardware-dependent checks cannot be fully automated in CI without a device.*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 60s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
