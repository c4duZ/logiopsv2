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

## 🚦 START HERE — EXECUTION GATE (must clear before ANY 4.2 code change)

> **Status as of 2026-06-01:** Phase 4.1 is complete. Phase 4.2 has NOT started.
> The very first 4.2 task is this gate — a live hardware capture. Do it before touching code.
> **Do NOT just run `/gsd-execute-phase 4.2`** until the gate result is recorded below, because
> Plan 04.2-01 is written for the *daemon* hypothesis, which the evidence below contradicts.

### The conflict to resolve
The original debug session concluded **Bug A = a daemon defect** (a *live* `SetAction` creates a
detached, non-dispatched, non-introspectable child action). The plan-checker AND a fresh static
code analysis (2026-06-01) **contradict that premise**. The daemon already does the "right" wiring;
the defect is almost certainly in the **GUI call-ordering / node-materialization**. If you "fix" the
daemon you'll get a green CTest and a still-broken mouse.

### Static-analysis findings (2026-06-01, code read — no hardware)
Confidence: **~85% GUI defect (Hypothesis B)**, ~15% daemon (Hypothesis A).
1. `ReleaseGesture::setAction` (`src/logid/actions/gesture/ReleaseGesture.cpp:97`) ALREADY stores a
   strong `_action = Action::makeAction(_device, type, _config.action, _node)`. `ThresholdGesture`
   does the same (`ThresholdGesture.cpp:95`). `Action::makeAction` sets `ret->_self = ret`
   (`Action.cpp:106`), keeping the action alive; the node's weak entry
   (`ipcgull/node.h:109`, `_interfaces` is `map<string, weak_ptr<interface>>`) therefore resolves.
   → The daemon's live `setAction` is **NOT** obviously a detached copy. Plan 04.2-01's core premise
   is unconfirmed by the code.
2. **GUI call order** (target nodes matter):
   - `SetGesture("up","OnRelease")` → on the **button** node `.../buttons/<N>`
     (`GestureModel.cpp performSetGesture`, child path built at `GestureModel.cpp:519`).
   - `SetAction("Keypress")` → on `.../buttons/<N>/gestures/up`, iface `Gesture.OnRelease`.
   - `SetKeys([...])` → on `.../buttons/<N>/gestures/up`, iface `Action.Keypress`.
   - SetAction/SetKeys are **chained behind the in-flight SetGesture watcher**
     (`_pendingSetGesture`, `GestureModel.cpp:562-569`). This ordering relies on strict in-order
     processing; a race between SetGesture materializing the child node and SetAction/SetKeys
     arriving is the prime suspect.
3. **Real collision (independent defect, fix regardless):** `ReleaseGesture::interface_name` and
   `ThresholdGesture::interface_name` are **both** `"OnRelease"`
   (`ReleaseGesture.cpp:25`, `ThresholdGesture.cpp:25`). In `Gesture::makeGesture`
   (`Gesture.cpp:70-89`) the `ThresholdGesture` branch is therefore **never reached** — `"OnRelease"`
   always builds a `ReleaseGesture`. The GUI dodges it by never sending `"OnThreshold"`
   (`GestureModel.cpp:188-189`), but the daemon defect is real and should get a fix + regression CTest.

### ✅ THE GATE: run this discriminating capture FIRST (needs the MX Master 4 + GUI)
Goal: prove whether, on a **fresh GUI-driven sequence on a button that is NOT already a gesture in
`logid.cfg`**, the `Action.Keypress` interface is actually missing from Introspect after a
successful `SetAction("Keypress")` — and whether the GUI hits the **correct node path, in order, without error.**

1. Stop the service daemon, run it foreground with full verbosity:
   ```bash
   sudo systemctl stop logid
   sudo /usr/bin/logid -vvv -c /etc/logid.cfg   # leave running in terminal A
   ```
2. In terminal B, monitor the system bus traffic for the service:
   ```bash
   sudo busctl --system monitor pizza.pixl.LogiOps   # or: sudo dbus-monitor --system "destination='pizza.pixl.LogiOps'"
   ```
3. Open the GUI, pick a button that is **NOT** configured as a gesture in `logid.cfg`, set it to
   Gesture → set a direction's mode (OnRelease) → assign a Keypress → set keys. Watch terminals A+B.
4. Immediately after the `SetAction("Keypress")` reply is seen, introspect the SAME gesture node
   (substitute the real device id / button index / direction from the capture):
   ```bash
   sudo busctl --system introspect pizza.pixl.LogiOps \
     /pizza/pixl/LogiOps/devices/<id>/buttons/<N>/gestures/up
   ```

### Record the result here, then proceed by the matching branch:
- **Branch A — daemon detached-copy DOES reproduce** (Introspect omits `Action.Keypress` on a
  correct, in-order, error-free GUI sequence to the right node):
  → Execute **Plan 04.2-01 as written**, pinning its CTest to this fresh evidence.
- **Branch B — GUI ordering/materialization confirmed** (the GUI hits the wrong node, wrong order,
  an error reply, or the `Action.Keypress` interface IS present in Introspect so the daemon was fine):
  → **Re-scope before executing.** Shrink the daemon plan to: fix the `OnRelease`/`OnThreshold`
  interface-name collision + add a regression CTest. Move the real fix to the **GUI**
  (`ReassignPanel` / `GestureModel` — the `SetAction("Gesture")`-then-write ordering / node
  materialization / watcher chaining at `GestureModel.cpp:562-569`). Update Plans 04.2-01..04 to
  match, then execute.

**Invariant (do not violate):** do not "fix" daemon code that is already correct while the real
defect goes untouched — that yields a green CTest and a red hardware UAT.

> Full static-analysis detail and the daemon-vs-GUI evidence chain were produced in the
> 2026-06-01 autonomous session (stopped here by user decision). Re-read this gate top-to-bottom
> before running `/gsd-execute-phase 4.2` or `/gsd-plan-phase 4.2 --gaps`.
