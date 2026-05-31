---
phase: 4
slug: fine-grained-gesture-control
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-31
---

# Phase 4 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest + standalone C++ test executables (mirrors `tests/phase3/`) |
| **Config file** | `tests/phase4/CMakeLists.txt` — Wave 0 installs (mirror `tests/phase3`) |
| **Quick run command** | `ctest --test-dir build -R phase4 --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | ~1 second |

---

## Sampling Rate

- **After every task commit:** Run `ctest --test-dir build -R phase4`
- **After every plan wave:** Run the full suite (`ctest --test-dir build`)
- **Before verification:** Full suite must be green
- **Max feedback latency:** ~5 seconds

---

## Per-Task Verification Map

> Filled by the planner. Each GEST requirement maps to at least one automated test with injected fakes (logic/math) plus an on-hardware human UAT row (live HID++ firing). Mirrors the Phase 3 split.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 04-00-01 | 00 | 0 | infra | — | N/A | unit | `ctest --test-dir build -R phase4` | ❌ W0 | ⬜ pending |

---

## Wave 0 Requirements

- [ ] `tests/phase4/` — test target wired into CTest (mirror `tests/phase3/CMakeLists.txt`)
- [ ] Fakes/recording seams for the gesture model + daemon mode-switch logic (mirror Phase 3's `ButtonsModel` recording subclass)
- [ ] Math fixtures for the GEST-02 interval/multiplier fix (assert "one flick = one step" accounting; carry-remainder / no multi-fire)

*Concrete files assigned by the planner.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Live gesture firing on the device (direction → action actually triggers) | GEST-01/03 | Requires a paired Logitech device + observing real HID++ gesture dispatch | Build a gesture, perform the motion on the device, confirm the action fires |
| "One flick = one step" feel (volume +1, one desktop) | GEST-02 | Requires on-hardware calibration of the granularity default against the MX Master 4 | Set the granularity slider to its left stop, flick once, confirm exactly one step |
| Live mode-switch + polkit Save persistence | GEST-01/04 | Live polkit prompt + daemon restart cycle | Switch mode, Save (approve polkit), restart daemon, confirm the gesture survives |

*Logic (model two-step, mode-string allowlist, interval/multiplier math) is unit-covered with injected fakes; only live firing/feel/polkit are on-hardware.*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
