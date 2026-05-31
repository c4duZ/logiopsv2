---
status: partial
phase: 03-core-config-ui-persistence
source: [03-VERIFICATION.md]
started: 2026-05-30
updated: 2026-05-30
---

## Current Test

[awaiting human testing on a paired Logitech device]

## Tests

### 1. Buttons tab — reassign, key-capture, hover sync, non-remappable
expected: Captured combo reads back; the physical button fires Ctrl+Alt+T; hover sync works both directions; non-remappable button is inert with tooltip.
result: [pending]

### 2. Buttons tab — host switch + profile switch on hardware
expected: Host hop / profile change occurs on hardware; host slots count matches the device's paired-host count (ChangeHost.GetHostCount).
result: [pending]

### 3. Pointer tab — DPI slider bounds + live pointer feel
expected: Slider bounded by reported min/max; snapped readout; live pointer-speed change.
result: [pending]

### 4. Pointer tab — DPI preset values + labels persist across daemon restart
expected: Presets + labels persist device-scoped across a daemon restart after Save (polkit approved).
result: [pending]

### 5. Scroll tab — capability gating + SmartShift/hi-res/thumbwheel behavior
expected: Capability-absent sections hidden; each control changes real scroll behavior.
result: [pending]

### 6. Persistence + polkit — async Save (no freeze), approve/deny copy, restart persistence
expected: Async Save (no freeze) through the real polkit prompt; correct approve/deny copy; persistence only after a successful Save.
result: [pending]

### 7. Restore defaults + UI-01 visual contract
expected: ClearProfile resets the device live; the interface matches the UI-SPEC visual/motion contract (sliding accent underline, tab cross-fade, motion budget).
result: [pending]

## Summary

total: 7
passed: 0
issues: 0
pending: 7
skipped: 0
blocked: 0

## Gaps
