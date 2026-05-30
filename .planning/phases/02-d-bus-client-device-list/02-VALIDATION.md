---
phase: 2
slug: d-bus-client-device-list
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-30
---

# Phase 2 — Validation Strategy

> Per-phase validation contract. The device-list model is unit-testable against a fake D-Bus proxy (no hardware); live battery/hotplug behavior needs a running daemon + device (smoke, graceful-skip — Phase 1 pattern).

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest (CMake) — Qt Test for the C++ model/proxy units; shell smoke scripts for live daemon |
| **Config file** | `test/CMakeLists.txt` (extended from Phase 1); GUI units under `src/logiops-gui/test/` or `test/` |
| **Quick run command** | `cmake --build build && ctest --test-dir build --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --output-on-failure && bash test/smoke/device-list.sh` |
| **Estimated runtime** | ~90 seconds (Qt build dominates) |

---

## Sampling Rate

- **After every task commit:** `cmake --build build` (clean, `-Werror`) + `ctest` for any landed unit
- **After every plan wave:** full suite (units + smoke where a daemon is runnable)
- **Before `/gsd-verify-work`:** units green; live smoke executed once on real hardware
- **Max feedback latency:** ~90 seconds

---

## Per-Task Verification Map

> Refined by the planner per task. Anchors derive from the success criteria + the battery addition.

| Task ID | Plan | Wave | Requirement | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------------|-----------|-------------------|-------------|--------|
| 2-xx | xx | 0 | DEV-01/03/04 | model lists/adds/removes devices from a fake proxy | unit | `ctest -R device_model` | ❌ W0 | ⬜ pending |
| 2-xx | xx | 1 | DEV-02 | BatteryStatus wrapper decodes 0x1000 report; D-Bus Battery property + change signal | unit | `ctest -R battery_feature` | ❌ W0 | ⬜ pending |
| 2-xx | xx | 1 | CONF-03 | model updates via signal (dataChanged), never resets/refetches whole list | unit | `ctest -R device_model_noflicker` | ❌ W0 | ⬜ pending |
| 2-xx | xx | 2 | DEV-01..04 | live list + battery against the running daemon | smoke | `bash test/smoke/device-list.sh` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `device_model` unit (Qt Test) — drive a fake/in-process proxy emitting DeviceAdded/Removed/StatusChanged; assert the QAbstractListModel rows/roles (DEV-01/03/04)
- [ ] `device_model_noflicker` unit — assert a battery/status tick emits per-role `dataChanged` (not `modelReset`), and no re-sort on tick (CONF-03)
- [ ] `battery_feature` unit — feed a synthetic HID++ 0x1000 BatteryStatus response, assert decoded percentage/charging + that the D-Bus Battery property + change signal fire (DEV-02)
- [ ] `test/smoke/device-list.sh` — graceful-skip smoke: enumerate the live daemon, assert ≥1 device path, read Name/Battery, observe a StatusChanged on sleep/wake

*Existing infrastructure (Phase 1 CTest harness) is extended; Qt Test is added for the GUI.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Battery % updates live as the device drains/charges | DEV-02 | Needs real hardware + time | Open the app with a real device; confirm % + charging icon update without restart |
| Hotplug (plug/unplug, sleep/wake) updates the list without restart | DEV-03 | Needs real device | Unplug/replug the receiver; sleep/wake the mouse; list reflects it live |
| Access-denied onboarding shows for a non-group user | DEV-01/access | Needs a user outside the `logiops` group | Run the GUI as a user not in `logiops`; confirm the onboarding screen (usermod guidance), not a crash |
| Daemon-down vs no-devices vs no-access are distinct screens | CONF-03 | Needs daemon stop/start | Stop `logid` → "not running"; start with no device → "no devices"; each distinct |

*Hardware/time-dependent checks cannot be fully automated in CI.*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 90s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
