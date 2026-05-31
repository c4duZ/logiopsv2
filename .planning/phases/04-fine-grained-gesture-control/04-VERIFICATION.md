---
phase: 04-fine-grained-gesture-control
verified: 2026-05-31T12:00:00Z
status: human_needed
score: 4/4 must-haves verified (code-level)
overrides_applied: 0
re_verification:
  previous_status: gaps_found
  previous_score: 3/4 (1 partial)
  gaps_closed:
    - "GEST-01 '→ action' leg: the gesture builder's keystroke capture now routes to gestureModel.setGestureKeypress(direction, names) targeting the gesture DIRECTION node, not buttonsModel.setKeypress (whole button). Closed by commit 92c7733."
  gaps_remaining: []
  regressions: []
gaps: []
deferred: []
human_verification:
  - test: "On-hardware one-flick-one-step feel (GEST-02)"
    expected: "With the MX Master 4 connected, configure an up-gesture as 'Repeat while moving' / 'Next desktop' at the granularity slider's leftmost stop; one physical flick switches EXACTLY one desktop (no overshoot to 2+); a volume gesture changes by exactly one tick"
    why_human: "Live gesture firing requires real HID++ input + uinput synthesis; the defaults::gesture_interval=120 value is an unconfirmed calibration (Assumption A1) that must be felt on hardware. Unit math (floor(M/I)) is GREEN but the unit-count-per-flick is hardware-specific."
  - test: "On-hardware multi-step repeat (GEST-03)"
    expected: "Keep moving in one continuous motion past the leftmost-stop interval; multiple desktops switch in that single motion (e.g. 3 desktops for ~3 intervals of travel)"
    why_human: "Requires continuous physical motion on real hardware to confirm the repeat-per-interval fires the expected number of times; not unit-testable without HID++/uinput."
  - test: "On-hardware gesture action fires (GEST-01 keystroke leg)"
    expected: "Bind a discrete-mode gesture direction (e.g. up = 'Do once when moved far enough') to a keystroke via the builder's 'Choose what this direction does' → key capture; flick up on hardware and confirm the bound keystroke fires (and the WHOLE BUTTON's own action did NOT change)"
    why_human: "The two-step SetAction('Keypress')→SetKeys on the .../gestures/<direction> node is unit-verified (order + target proven by the recording subclass), but actual firing of the bound keystroke on a real flick — and confirming the button binding is untouched — needs live HID++/uinput + the daemon."
  - test: "Live polkit Save persistence"
    expected: "After a gesture edit the 'Unsaved changes' pill appears; clicking Save raises the polkit prompt; after approving and restarting the daemon, reopening the app shows the gesture survived (mode + granularity restored via seedFromDaemon)"
    why_human: "Requires the live D-Bus daemon, the polkit agent, and a daemon restart cycle; cannot be exercised by the no-bus recording-subclass unit tests."
  - test: "Capability gating of the Gesture category"
    expected: "Selecting a gesture-capable button shows the 'Gesture' category; selecting a non-gesture button HIDES it (not greyed)"
    why_human: "Depends on the live daemon reporting GestureSupport per button; the gate is wired to roleData(4) but the per-button capability value comes from real hardware introspection."
  - test: "WR-01 two-step async ordering on live bus (incl. SetAction→SetKeys)"
    expected: "Issue a granularity change immediately after a mode switch; the param setter lands on the rebuilt .Gesture.<mode> child node (not lost, no UnknownMethod). For the keystroke leg, SetKeys lands on the Action.Keypress interface the SetAction just created at the gesture node — no UnknownInterface/UnknownMethod"
    why_human: "The recording-subclass test harness records call order via overrides; the live QDBusPendingCallWatcher chaining (_pendingSetGesture) and the SetKeys-fires-unconditionally-after-the-hop logic are correct by construction but are not covered by an automated live-bus test."
  - test: "WR-03 seedFromDaemon readback on a pre-configured button"
    expected: "Open the gesture builder on a button that already has gestures bound in /etc/logid.cfg; the configured dots, mode pills, and preview reflect the existing config (not blank)"
    why_human: "seedFromDaemon uses live synchronous Get* probes against the daemon; cannot be exercised without a live daemon and a pre-configured button. Note: gesture action TYPE is intentionally not seeded (no GetAction getter on the gesture interfaces)."
---

# Phase 4: Fine-Grained Gesture Control Verification Report

**Phase Goal:** The user can build gestures that fire exactly once or repeat predictably, fixing the owner's concrete pain ("volume steps by 2", "only one desktop switch").
**Verified:** 2026-05-31 (re-verification)
**Status:** human_needed
**Re-verification:** Yes — after gap closure (commit 92c7733, GEST-01 action leg)

## Re-Verification Summary

The single prior gap — GEST-01's "→ action" leg rebinding the **whole button** instead of the **gesture direction** — is now **CLOSED** at the code level.

| Prior gap | Resolution | Evidence |
| --------- | ---------- | -------- |
| Keystroke capture flowed to `buttonsModel.setKeypress(row,...)` (rebinds whole button); `setGestureAction`/per-direction path unreachable from the UI | New `GestureModel::setGestureKeypress(direction, evdevNames)` two-step targeting the gesture node; `ReassignPanel` routes the shared capture to it when `activeGestureDirection` is set, and STILL uses `buttonsModel.setKeypress` for normal button reassignment | `GestureModel.cpp:314-347`, `GestureModel.h:111-112`; `ReassignPanel.qml:69,124,131-147,283-289`; new tests pass |

Score moves from **3/4 (1 partial)** → **4/4 code-level**. Status is **human_needed** (not `passed`) because the on-hardware firing/feel, live polkit Save persistence, live async ordering, and capability gating still require the MX Master 4 + live daemon — these were always the phase's manual UAT set and are unchanged by the fix.

## Goal Achievement

### Observable Truths (ROADMAP Success Criteria)

| # | Truth | Status | Evidence |
| - | ----- | ------ | -------- |
| 1 | User can build a gesture by picking direction → mode → **action** through a guided UI (GEST-01) | ✓ VERIFIED (code) | Direction cross + mode pills + granularity + preview all bound to `GestureModel`, capability-gated on `roleData(4)`. **Action leg now genuinely wired:** `GestureBuilder.onChooseActionRequested(direction)` → `ReassignPanel.activeGestureDirection = direction` → shared `KeyCaptureField.onKeysCaptured` → `gestureModel.setGestureKeypress(direction, names)` (`ReassignPanel.qml:283-289, 131-147`). `setGestureKeypress` (`GestureModel.cpp:314-347`) dispatches `SetAction("Keypress")` then `SetKeys` on the `.../gestures/<direction>` node (`performParamCall` `:472-545` routes SetKeys to `Action.Keypress` at the gesture node, chained behind the in-flight SetGesture). The whole-button `buttonsModel.setKeypress` path is preserved for normal reassignment (else-branch, `:144-146`; direct Keystroke `CategoryRow` clears `activeGestureDirection`, `:124`). Live firing → human. |
| 2 | User can set granularity so one gesture = exactly one discrete step (GEST-02) | ✓ VERIFIED (code) | `defaults::gesture_interval = 120` (`Configuration.h`); `IntervalGesture::move` `value_or(...)` + `interval <= 0` divide-guard; `phase4_gesture_math` GREEN (120/120→1, 360/120→3, 239/120→1 no overshoot). Live "feel" → human. |
| 3 | User can make a gesture repeat per interval (GEST-03) | ✓ VERIFIED (code) | Same `IntervalGesture` fix: unset interval no longer dead-fires; `phase4_gesture_math` asserts 360/120 → exactly 3 fires. Live multi-desktop → human. |
| 4 | The gesture UI explains in plain language when/how often the action fires (GEST-04) | ✓ VERIFIED | NOTIFYable `previewSentence` composed in C++ (`GestureModel.cpp:388-408`); bound read-only in `GestureBuilder.qml:294`. `phase4_gesture_model::test_preview_sentence` asserts "Moving up does nothing." with NOTIFY. |

**Score:** 4/4 truths verified at code level (GEST-01 action leg now closed). On-hardware behaviors routed to human verification.

### Required Artifacts

| Artifact | Expected | Status | Details |
| -------- | -------- | ------ | ------- |
| `src/logiops-gui/GestureModel.{h,cpp}` | `setGestureKeypress` two-step targeting gesture node | ✓ VERIFIED | `.h:111-112` declares `Q_INVOKABLE bool setGestureKeypress(direction, evdevNames)`; `.cpp:314-347` validates then dispatches `SetAction("Keypress")`→`SetKeys`; `performParamCall:483-485` routes SetKeys to `Action.Keypress` at `.../gestures/<direction>`. |
| `src/logiops-gui/qml/config/ReassignPanel.qml` | route capture to `setGestureKeypress` in gesture mode; keep `setKeypress` for button | ✓ VERIFIED | `activeGestureDirection` property (`:69`); `onKeysCaptured` branches on it (`:138-146`); `onChooseActionRequested` sets it (`:283-289`); direct Keystroke clears it (`:124`). No regression of button reassignment. |
| `tests/phase4/GestureModelTest.cpp` | `test_gesture_keypress_two_step_order` + guards | ✓ VERIFIED | `:141-170` asserts SetAction("Keypress") THEN SetKeys, both `type="OnRelease"` (gesture mode, proves gesture not button); `:174-196` guards (empty keys / bad dir / no-action mode → 0 dispatch). All PASS. |
| `src/logid/Configuration.h` | `gesture_interval` default | ✓ VERIFIED (regression) | Unchanged; daemon math still GREEN. |
| `src/logid/actions/gesture/IntervalGesture.cpp` | interval default + divide-guard | ✓ VERIFIED (regression) | Unchanged; `phase4_gesture_math` GREEN. |
| `tests/phase4/gesture_math_test.cpp` | floor(M/I) accounting | ✓ VERIFIED (regression) | Unchanged; GREEN. |

### Key Link Verification

| From | To | Via | Status | Details |
| ---- | -- | --- | ------ | ------- |
| `GestureBuilder.qml` | `ReassignPanel.qml` | `chooseActionRequested(direction)` → `activeGestureDirection` | ✓ WIRED | `:283-289` scopes the shared capture to the direction. |
| `ReassignPanel.qml` (capture) | `GestureModel.cpp` | `setGestureKeypress(direction, names)` when in gesture-action mode | ✓ WIRED | `:138-143`; the previously-broken sub-link is now closed. |
| `ReassignPanel.qml` (capture) | `ButtonsModel` | `setKeypress(row, names)` for normal button reassignment | ✓ WIRED (non-regressed) | `:144-146` else-branch preserved. |
| `setGestureKeypress` | `.../gestures/<direction>` node | `SetAction("Keypress")` → `SetKeys` on `Action.Keypress` | ✓ WIRED | `GestureModel.cpp:332-337` + `performParamCall:483-485,509-545`; targets gesture node, sequenced. |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| -------- | ------- | ------ | ------ |
| Full build -Werror clean | `cmake --build build` | exit 0, no warnings/errors; `CMAKE_CXX_FLAGS=-Werror` in cache | ✓ PASS |
| Full test suite | `ctest --test-dir build` | 14/14 passed (0.39s) | ✓ PASS |
| Phase-4 model unit (incl. new test) | `ctest -R phase4_gesture_model -V` | 7 passed, 0 failed, 0 skipped — incl. `test_gesture_keypress_two_step_order` + `test_gesture_keypress_guards` | ✓ PASS |
| GEST-02/03 accounting | `phase4_gesture_math` | floor(M/I): 120/120→1, 360/120→3, 239/120→1 | ✓ PASS |
| Build cache hygiene | `grep USE_USER_BUS build/CMakeCache.txt` | `OFF` (no prod contamination per MEMORY) | ✓ PASS |
| Live gesture firing | n/a | requires HID++/uinput | ? SKIP → human |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
| ----------- | ----------- | ----------- | ------ | -------- |
| GEST-01 | 00,02,03 | Build gesture via direction → mode → action guided UI | ✓ SATISFIED (code) | Action leg now wired to the gesture direction via `setGestureKeypress`; two-step order + gesture-node targeting unit-proven. Keystroke is the action offered today; `setGestureAction(direction,type)` exists in the model for the broader set. Live firing → human UAT. |
| GEST-02 | 00,01,02,03 | Granularity so one gesture = one step; sane defaults | ✓ SATISFIED (code) | Daemon `value_or` default + divide-guard + math test GREEN. Live feel → human UAT. |
| GEST-03 | 00,01,02,03 | Gesture repeats per interval | ✓ SATISFIED (code) | IntervalGesture default + 3-fire test. Live multi-desktop → human UAT. |
| GEST-04 | 00,02,03 | UI explains in plain language when/how often | ✓ SATISFIED | NOTIFYable C++ previewSentence + templates; model test GREEN. |

No orphaned requirements: all four GEST IDs declared in plan frontmatter, all map to Phase 4 only.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| ---- | ---- | ------- | -------- | ------ |
| (none) | — | The prior `ReassignPanel.qml` "rebinds whole button" warning is RESOLVED — the capture now branches to `setGestureKeypress` for the gesture direction | ℹ️ Info | Gap closed; no remaining stub or broken data path in the phase-4 surface. |

### Human Verification Required

Seven items need the MX Master 4 and/or a live daemon+polkit (see frontmatter for full steps): (1) one-flick-one-step feel GEST-02, (2) multi-step repeat GEST-03, (3) on-hardware gesture keystroke firing GEST-01 (confirm the keystroke fires on a flick AND the button binding is untouched), (4) polkit Save persistence, (5) per-button capability gating, (6) WR-01 live async ordering incl. SetAction→SetKeys, (7) WR-03 seedFromDaemon readback. These are the phase's standing manual UAT set (logic unit-tested with fakes; live HID++/uinput/polkit → human), consistent with VALIDATION.md.

### Gaps Summary

No remaining gaps. The one prior gap (GEST-01 action leg) is closed: the guided builder's action step now binds a keystroke to the chosen **gesture direction** (`SetAction("Keypress")` → `SetKeys` on `.../gestures/<direction>`, sequenced and proven by the recording-subclass test) rather than rebinding the whole button, while the normal button-reassignment keystroke path is preserved. Build is -Werror clean, 14/14 ctest, phase-4 model 7/7 (0 skipped). The remaining work is on-hardware validation of actual firing/feel and live-bus/polkit behavior, which is correctly out of the automated harness's reach and routed to human verification — hence status `human_needed`, not `passed`.

---

_Verified: 2026-05-31 (re-verification after commit 92c7733)_
_Verifier: Claude (gsd-verifier)_
