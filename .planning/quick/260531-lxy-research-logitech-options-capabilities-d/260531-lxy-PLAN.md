---
quick_id: 260531-lxy
description: Fix button reassign (restore-default, mouse-button actions, read-back, two-step chaining) per Options+ research
mode: quick
date: 2026-05-31
---

# Quick Task 260531-lxy

## Objective

Act on the Options+ research diagnosis. Four GUI-only fixes (the daemon already supports everything needed — BTN_* are EV_KEY codes the InputDevice already enables, and SetAction("Default") un-diverts a button):
1. **Restore default** — "Disabled"/None keeps the button diverted+dead; add a real "Restore default" that calls SetAction("Default") so the button regains its native hardware function (fixes "can't set middle button back to middle button").
2. **Mouse-button actions** — expose Middle/Back/Forward click as assignable actions via the existing setKeypress(["BTN_MIDDLE"/"BTN_BACK"/"BTN_FORWARD"]) path.
3. **Read-back after reassign** — the binding list is optimistic-only (shows the new binding even if hardware kept the old); after a reassign, read back the actual present .Action.<X> and update the row from reality.
4. **Two-step chaining** — performParamCall must chain off the SetAction reply (QDBusPendingCallWatcher), not a separate GetAll-probe that assumes ordering and silently no-ops on SetAction error.

## Tasks

### Task 1 — Restore-default + mouse-button actions (ButtonsModel + ReassignPanel)

<files>
- src/logiops-gui/ButtonsModel.h / .cpp (add a restoreDefault(row) Q_INVOKABLE calling SetAction("Default"); add a setMouseButton(row, btnName) convenience or reuse setKeypress with BTN_* )
- src/logiops-gui/qml/config/ReassignPanel.qml (a "Restore default" row distinct from "Disabled"; a "Mouse button" category with Middle/Back/Forward)
- src/logid/actions/Action.cpp (confirm "Default" un-diverts via config.reset() — read_first only)
</files>

<read_first>
- src/logid/actions/Action.cpp — SetAction("Default") path -> config.reset() -> button un-diverted (native function restored). NullAction (None) stays diverted+dead.
- src/logiops-gui/ButtonsModel.cpp — setAction/setKeypress two-step; clearAction currently sends "None".
- src/logid/InputDevice.cpp — BTN_LEFT/MIDDLE/RIGHT/BACK/FORWARD/SIDE/EXTRA already registered (EV_KEY); KeypressAction toKeyCode resolves "BTN_MIDDLE" etc.
</read_first>

<action>
- Add `Q_INVOKABLE void restoreDefault(int row)` to ButtonsModel: performs the daemon SetAction("Default") (un-divert), marks dirty, and updates the row's currentActionType to a "Default"/"Native" summary. This is DISTINCT from clearAction (None=Disabled).
- Expose Middle/Back/Forward click as assignable actions. Simplest: a "Mouse button" CategoryRow in ReassignPanel that offers Middle Click [BTN_MIDDLE], Back [BTN_BACK], Forward [BTN_FORWARD] (and optionally Left/Right), each calling buttonsModel.setKeypress(row, ["BTN_MIDDLE"]) etc. (the daemon already emits these via the keypress action). Use a human summary ("Middle click").
- In ReassignPanel, add a "Restore default" row (calls restoreDefault) ABOVE/near "Disabled", with copy clarifying: Restore default = the button's normal function; Disabled = button does nothing.
</action>

<verify>
- `grep -n "restoreDefault\|Default\|BTN_MIDDLE\|Mouse button\|Restore default" src/logiops-gui/ButtonsModel.cpp src/logiops-gui/qml/config/ReassignPanel.qml` shows the new action + UI.
- clean-qmlcache rebuild + offscreen smoke clean; ctest 14/14.
</verify>

<done>User can restore a button (e.g. middle) to its native function, and can assign Middle/Back/Forward click; both apply on the device.</done>

### Task 2 — Read-back + two-step chaining (ButtonsModel correctness)

<files>
- src/logiops-gui/ButtonsModel.cpp / .h (performSetAction/performParamCall live path; applyCurrentAction)
</files>

<read_first>
- src/logiops-gui/ButtonsModel.cpp — performSetAction (live SetAction), performParamCall (the GetAll-probe that assumes ordering), applyCurrentAction (optimistic-only row update). Mirror DeviceController's QDBusPendingCallWatcher chaining for the proper sequence.
</read_first>

<action>
- Make performParamCall chain off the SetAction reply: keep the QDBusPendingCallWatcher on SetAction and only issue the per-type param setter (SetKeys/SetChange/etc.) in its finished handler, on success. On SetAction error, do NOT fire the param call (and surface a rejection), instead of probing a possibly-absent interface.
- After a reassign completes (SetAction [+ param] replies landed), READ BACK the button's actual current action (re-probe which .Action.<X> interface is present at the button node, as enumerate does) and update the row from that — so the binding list reflects what the daemon actually applied, not an optimistic guess. Keep an immediate optimistic update for snappiness but reconcile with the read-back when it returns.
</action>

<verify>
- `grep -n "QDBusPendingCallWatcher\|readBack\|reconcile" src/logiops-gui/ButtonsModel.cpp` shows the chaining + read-back.
- `ctest --test-dir build` 14/14 (the phase3 button-model test still passes — keep the recording-subclass test seam intact; if the two-step ordering assertions change, update the test to match the corrected chaining, keeping it GREEN).
- clean-qmlcache rebuild + offscreen smoke clean.
</verify>

<done>Reassigning a button applies on hardware and the binding list shows the real, daemon-confirmed binding (no optimistic lie); the two-step never fires a param call at a nonexistent interface.</done>

## must_haves
- truth: A button can be restored to its native hardware function from the GUI.
- truth: Middle/Back/Forward click are assignable and work on the device.
- truth: After a reassign, the binding list reflects the daemon's actual applied action.
- artifact: src/logiops-gui/ButtonsModel.{h,cpp} (restoreDefault, chaining, read-back)
- artifact: src/logiops-gui/qml/config/ReassignPanel.qml (Restore default + Mouse button)
- key_link: ReassignPanel Restore default -> ButtonsModel.restoreDefault -> SetAction("Default")
