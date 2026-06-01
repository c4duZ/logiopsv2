# Phase 4.2 Context — Gesture Live-Apply Fix & Options+ UX Alignment (INSERTED)

**Created:** 2026-05-31
**Depends on:** Phase 4.1 (vocabulary/UX spec), Phase 4 (the built gesture feature)

## Why this exists
Phase 4 was marked "complete" but on-hardware UAT (2026-05-31) showed the core feature is broken: a gesture built **in the GUI** does not fire on hardware, the GUI doesn't read back an existing gesture, and granularity has no effect. Root cause is diagnosed (not yet fixed) — the user chose "re-plan first," so the fix is folded here, combined with aligning the gesture UX to the mined Options+ model.

## Root cause on file
Full evidence: `.planning/debug/gesture-live-apply-and-save.md` (session `gesture-live-apply-and-save`).

- **Bug B (Save / polkit "authorization declined") — FIXED + committed `70e9457`.** The system D-Bus policy had an unqualified `<deny receive_sender="pizza.pixl.LogiOps"/>` that blocked the root daemon's own outbound `CheckAuthorization` call to polkitd. Narrowed to `receive_type="signal"`. Verified on hardware.
- **Bug A (gesture doesn't fire) — root-caused, NOT fixed:**
  - Test 1 (decisive): a live `SetKeys` on cfg-wired button `0xC3` (index 5) changed hardware behavior (volume went DOWN). → **daemon dispatch identity is CORRECT** for cfg-loaded gestures.
  - D-Bus capture of the GUI configuring a non-cfg button (index 8 / cid 416): the full two-step lands successfully — `SetGesture("up","OnRelease")` → `SetAction("Keypress")` → `SetKeys` → `SetThreshold` all return success.
  - **Smoking gun:** after a successful `SetAction("Keypress")` on the gesture node, an `Introspect` of the SAME path still shows ONLY `Gesture.OnRelease` — the `Action.Keypress` interface does NOT appear — yet `SetKeys` to that interface succeeds. So a **live**-created gesture child action is reachable for method calls but is **not wired into the introspectable/dispatched object** (a detached copy). This single defect explains all three symptoms:
    - gesture engages but never fires (dispatch uses the gesture's real `_action`, which the live `SetAction` didn't replace),
    - read-back shows "choose what this direction does" (GUI introspects, sees no `Action.*`),
    - granularity has no effect (`SetThreshold` likely mutates the same detached copy).
  - Also: gesture direction nodes are created lazily — only configured directions exist (`down/left/right` return "Object does not exist" until set).

## Likely fix area (daemon)
`src/logid/actions/gesture/*.cpp` (`ReleaseGesture`/`IntervalGesture`/`AxisGesture` `setAction`/`makeAction`) + `src/logid/actions/GestureAction.cpp` — ensure a live `Gesture::setAction("<type>")` constructs the action AND (a) stores it as the gesture's dispatched `_action`, and (b) registers it on the gesture's ipcgull node so it's introspectable. Cross-check against how cfg-load (`makeAction`) wires it (that path works). Also confirm `SetThreshold` mutates the live gesture object.
- Note flagged in prior pass: `ReleaseGesture` and `ThresholdGesture` both register interface name `"OnRelease"`; `makeGesture("OnRelease")` always resolves to `ReleaseGesture`. Confirm this isn't masking OnThreshold.

## Success criteria
1. GUI-built gesture on a non-cfg button fires on hardware.
2. GUI reads back an existing gesture's mode + action.
3. Granularity/threshold from the GUI changes on-hardware behavior.
4. Builder wording/flow matches mined Options+ model (4.1 `vocabulary.md`).

## Requirements
- GEST-05 (live-apply correctness) + GEST-01..04 rework for UX alignment.
