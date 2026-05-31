---
status: partial
phase: 04-fine-grained-gesture-control
source: [04-VERIFICATION.md]
started: 2026-05-31T00:00:00Z
updated: 2026-05-31T00:00:00Z
---

## Current Test

[awaiting human testing on the MX Master 4 + live daemon/polkit]

## Tests

### 1. On-hardware one-flick-one-step feel (GEST-02)
expected: With the MX Master 4 connected, configure an up-gesture as "Repeat while moving" / "Next desktop" at the granularity slider's leftmost stop; one physical flick switches EXACTLY one desktop (no overshoot to 2+); a volume gesture changes by exactly one tick.
result: [pending]

### 2. On-hardware multi-step repeat (GEST-03)
expected: Keep moving in one continuous motion past the leftmost-stop interval; multiple desktops switch in that single motion (e.g. 3 desktops for ~3 intervals of travel).
result: [pending]

### 3. On-hardware gesture action fires (GEST-01 keystroke leg)
expected: Bind a discrete-mode gesture direction (e.g. up = "Do once when moved far enough") to a keystroke via the builder's "Choose what this direction does" → key capture; flick up on hardware and confirm the bound keystroke fires AND the WHOLE BUTTON's own action did NOT change.
result: [pending]

### 4. Live polkit Save persistence
expected: After a gesture edit the "Unsaved changes" pill appears; clicking Save raises the polkit prompt; after approving and restarting the daemon, reopening the app shows the gesture survived (mode + granularity restored via seedFromDaemon).
result: [pending]

### 5. Capability gating of the Gesture category
expected: Selecting a gesture-capable button shows the "Gesture" category; selecting a non-gesture button HIDES it (not greyed).
result: [pending]

### 6. WR-01 two-step async ordering on live bus (incl. SetAction→SetKeys)
expected: Issue a granularity change immediately after a mode switch; the param setter lands on the rebuilt .Gesture.<mode> child node (not lost, no UnknownMethod). For the keystroke leg, SetKeys lands on the Action.Keypress interface the SetAction just created at the gesture node — no UnknownInterface/UnknownMethod.
result: [pending]

### 7. WR-03 seedFromDaemon readback on a pre-configured button
expected: Open the gesture builder on a button that already has gestures bound in /etc/logid.cfg; the configured dots, mode pills, and preview reflect the existing config (not blank).
result: [pending]

## Summary

total: 7
passed: 0
issues: 0
pending: 7
skipped: 0
blocked: 0

## Gaps
