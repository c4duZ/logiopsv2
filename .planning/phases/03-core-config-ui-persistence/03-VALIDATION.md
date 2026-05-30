---
phase: 3
slug: core-config-ui-persistence
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-30
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None wired into daemon build today (per codebase TESTING.md). Phase 3 introduces a lightweight CTest target for the GUI C++ bridge + a daemon round-trip test. Qt: `QtTest` for C++ model/bridge logic. |
| **Config file** | `src/logiops-gui/CMakeLists.txt` (add `enable_testing()` + test target); `CMakeLists.txt` test wiring |
| **Quick run command** | `cmake --build build --target logiops-gui && ctest --test-dir build -R phase3 --output-on-failure` |
| **Full suite command** | `cmake --build build && ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | ~30–60 seconds (compile-dominated) |

Note: This is a desktop Qt/HID++ application. A large share of acceptance is **manual-on-hardware** (real Logitech device required) — captured explicitly in the Manual-Only table rather than faked with mocks.

---

## Sampling Rate

- **After every task commit:** Build the affected target (`cmake --build build --target logiops-gui` or `logid`) — compile is the fastest correctness gate for C++/QML.
- **After every plan wave:** Run `ctest --test-dir build -R phase3 --output-on-failure`.
- **Before `/gsd-verify-work`:** Full build green + the Wave-0 `Configuration::save()` round-trip test green.
- **Max feedback latency:** ~60 seconds.

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 3-00-01 | 00 | 0 | CONF-01 | T-3-save | save() preserves all device blocks/values (no silent loss) | integration | `ctest --test-dir build -R phase3_save_roundtrip` | ❌ W0 | ⬜ pending |
| 3-00-02 | 00 | 0 | CONF-01 | T-3-save | schema-unknown keys: warn, don't crash | integration | `ctest --test-dir build -R phase3_save_unknownkey` | ❌ W0 | ⬜ pending |
| 3-01-01 | 01 | 1 | BTN-01/04 | — | button enumeration uses ControlID, not index | unit | `ctest --test-dir build -R phase3_button_model` | ❌ W0 | ⬜ pending |
| 3-02-01 | 02 | 1 | DPI-01/02/03 | — | DPI clamped to reported min/max | unit | `ctest --test-dir build -R phase3_dpi_model` | ❌ W0 | ⬜ pending |
| 3-03-01 | 03 | 1 | SCR-01/02/03 | — | capability-gating: absent feature → section hidden | unit | `ctest --test-dir build -R phase3_capability_gate` | ❌ W0 | ⬜ pending |
| 3-04-01 | 04 | 2 | PROF-01/CONF-02 | — | SetProfile/ClearProfile round-trip via daemon | integration | `ctest --test-dir build -R phase3_profiles` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky · "W0" = file created in Wave 0*

---

## Wave 0 Requirements

- [ ] `tests/phase3/save_roundtrip_test.cpp` — load → mutate config tree → `Configuration::save()` → reload, assert all device blocks + values preserved (CONF-01, BLOCKING per research finding #2)
- [ ] `tests/phase3/save_unknownkey_test.cpp` — config with a schema-unknown key survives or warns, never crashes
- [ ] `src/logiops-gui` QtTest target for C++ bridge model logic (button/DPI/capability models)
- [ ] `enable_testing()` + CTest wiring in CMake (no framework exists today)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Click button hotspot on device render → reassign → binding shown | BTN-01, BTN-04 | Requires real device + GUI render + visual confirmation | Launch GUI with a paired Logitech mouse; click a button hotspot, assign a keystroke; confirm the callout + list both show the new binding |
| Live key-capture records the pressed combo | BTN-02 | Real keyboard input capture in a running GUI | In reassign panel, choose Keystroke → "press keys now" → press Ctrl+Alt+T → confirm captured combo matches |
| Built-in actions fire on the device | BTN-03, HOST-01 | Requires hardware to observe the action effect | Assign DPI-cycle / host-switch / profile-switch / SmartShift-toggle to a button; press it on the device; observe effect |
| DPI slider bounded by device min/max, cycle + labels | DPI-01/02/03 | Real device reports min/max; visual | Drag DPI slider to extremes (clamps); add 3 cycle presets with labels; cycle via assigned button |
| SmartShift / hi-res / thumbwheel controls affect scrolling | SCR-01/02/03 | Physical scroll behavior | Toggle SmartShift + threshold; toggle hi-res + invert; map thumbwheel; verify on hardware |
| Apply-live vs Save-to-disk: change persists across daemon restart only after Save | CONF-01 | Requires polkit prompt + daemon restart | Make a change (applies live); restart daemon WITHOUT Save → reverts; redo + Save (polkit prompt) → restart → persists |
| Restore defaults resets the device | CONF-02 | Hardware + visual | Restore defaults with confirmation; verify device config section cleared and live state reset |
| Tabbed Options+-style UI is polished/animated | UI-01 | Subjective visual quality | Visual review against UI-SPEC.md (tab transitions, motion budget, token fidelity) |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify (manual-on-hardware items documented above)
- [ ] Wave 0 covers all MISSING references (save round-trip is the BLOCKING gate)
- [ ] No watch-mode flags
- [ ] Feedback latency < 60s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
