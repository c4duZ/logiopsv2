# Quick Research: Logitech Options+ Capability Map + Button-Reassign Diagnosis

**Researched:** 2026-05-31
**Domain:** Logitech Options+ feature parity; logiops daemon action vocabulary; GUI button-reassign path
**Confidence:** HIGH (code diagnosis, file:line verified) / MEDIUM (Options+ web capability map)

## Summary

Three findings. (1) A **capability map** of what Logitech Options+ lets a user assign, for roadmap parity scoping. (2) The **"can't set middle button as middle button"** root cause: the daemon has **no mouse-button action** in its `config::Action` variant — there is literally no way to emit `BTN_MIDDLE`/`BTN_BACK`/`BTN_FORWARD`. BUT the existing `KeypressAction` + `InputDevice` can *already* emit `BTN_*` codes with zero daemon changes, because `BTN_*` are `EV_KEY` codes that pass `registerKey`'s bounds check. (3) The **"reassign still buggy in UI and clicks"** root cause: the daemon live-swap is correct, but the GUI's optimistic update + the daemon's `NullAction` divert behavior create two real bugs — "Disabled"/"None" does **not** restore the hardware default (the button stays diverted and dead), and the binding-list summary can desync because reassigns are optimistic-only with no read-back confirmation.

**Primary recommendation:** Add a first-class `ChangeButton`/mouse-button action to the daemon (small, sketched below) AND fix the restore-default semantics (un-divert vs. NullAction). Short term, the GUI can ship middle-click *today* by sending `setKeypress(row, ["BTN_MIDDLE"])`.

---

## 1. Logitech Options+ Capability Map (parity reference)

Grouped action vocabulary Options+ exposes, mapped to our roadmap. `[CITED: support.logi.com / hub.sync.logitech.com / logitech.com/discover]`, some `[ASSUMED]` from product knowledge where docs were vague.

### A. Mouse-button emulation `[ASSUMED — not in our daemon today]`
- Middle click, Back, Forward, Right click, Left click (re-emit a hardware button on any control)
- Double-click

### B. Keystroke / shortcut
- Single key or key combo capture ("Other Actions") `[CITED]` — **we have this (Keypress)**
- Copy / Paste / Cut, Undo (these are just keystroke shortcuts) `[CITED]`

### C. System / OS actions `[CITED]`
- Windows Task View / macOS Mission Control
- Window snap / window management / tab navigation
- Show desktop, Switch application (app-switch)
- Screenshot / screen capture
- Do Not Disturb toggle, Dictation, Clipboard history
- Lock screen

### D. Media & hardware `[CITED]`
- Volume up/down, Mute/unmute
- Media play/pause, next/prev
- Screen brightness up/down
- Zoom in/out, Pan, Rotate (pointer-driven)

### E. Pointer / device behavior — **partial parity**
- DPI / pointer-speed shift (precision toggle) — **we have ChangeDPI / CycleDPI**
- SmartShift toggle — **we have ToggleSmartShift**
- Gesture button (hold + direction) — **we have GestureAction (Phase 4)**
- Switch host / Logitech Flow `[CITED]` — **we have ChangeHost; Flow is multi-machine, not roadmapped**

### F. App launching & composite `[CITED]`
- Launch application / open URL — roadmap **Phase 7 (Smart Actions)**, routed through non-root helper
- **Smart Actions**: ordered multi-step macro (open app/URL, insert text, send keystrokes, wait, more keystrokes) — roadmap **Phase 7**
- **AI Actions** (prompt builder / ChatGPT) — out of scope, not roadmapped

### G. Gesture vocabulary `[CITED]`
- Directions: up / down / left / right (+ press/click as a 5th "no-direction") on any gesture-capable button or the thumbwheel/gesture button
- Each direction maps to any action in A–F (navigate windows, media control, task view, screen capture, switch application, volume)
- "Custom" preset lets the user bind each direction independently — **this is our GestureAction model (Phase 4)**

### Headline features vs ROADMAP
| Options+ feature | Our status |
|---|---|
| Button keystroke remap | Phase 3 (done) |
| **Mouse-button remap (middle/back/fwd)** | **MISSING — no action exists (see §2)** |
| DPI / pointer | Phase 3 (done) |
| SmartShift | Phase 3 (done) |
| Hi-res scroll | Phase 3 (done) |
| Per-app profiles / Flow-like | Phase 5 (Flow itself not roadmapped) |
| Gestures (fine-grained) | Phase 4 (done) |
| Action wheel (radial) | Phase 6 |
| Smart Actions / macros / launch app / open URL | Phase 7 |
| System actions (task view, screenshot, window mgmt) | **Not explicitly roadmapped** — most reduce to keystrokes; some need a session helper |
| Backlight / RGB | Phase 8 |

**Parity gap worth adding to backlog:** a curated "System action" picker (task view, screenshot, window snap, media, volume) that is mostly **pre-baked keystroke presets** the GUI offers — cheap parity win on top of the existing Keypress action, no daemon change.

---

## 2. "Can't set middle button as middle button" — root cause + fix

### Root cause (VERIFIED)
The `config::Action` variant has **no mouse-button action**:
`src/logid/config/schema.h:231-241` — variant = `NoAction, KeypressAction, ToggleSmartShift, ToggleHiresScroll, CycleDPI, ChangeDPI, ChangeHost, ChangeProfile, GestureAction`. There is no `ChangeButton`/`MouseButton`. The GUI's `kActionTypes[]` (`src/logiops-gui/ButtonsModel.cpp:42`) mirrors this — no mouse-button entry. So **no UI path can assign a mouse button**, confirmed.

### Key discovery: BTN_* already works through Keypress (no daemon change needed for a quick win)
`InputDevice` enables `EV_KEY` broadly and `registerKey` accepts any code `< KEY_CNT` (767):
- `src/logid/InputDevice.cpp:48-59` — `libevdev_enable_event_type(EV_KEY)`; pre-enables codes < 128, defers the rest to `registerKey`.
- `src/logid/InputDevice.cpp:80-89` — `registerKey(code)` enables any `code < KEY_CNT` on demand.
- `BTN_LEFT=0x110(272)`, `BTN_MIDDLE=0x112(274)`, `BTN_SIDE=0x113`, `BTN_EXTRA=0x114`, `BTN_BACK=0x116`, `BTN_FORWARD=0x115` — **all `EV_KEY` codes < 767**, so `registerKey` enables them and `pressKey`/`releaseKey` emit them.
- `KeypressAction::_setConfig` (`src/logid/actions/KeypressAction.cpp:66-100`) resolves a string via `toKeyCode` → `libevdev_event_code_from_name(EV_KEY, "BTN_MIDDLE")`, which **succeeds** for `BTN_*` names.

**=> Today, `buttonsModel.setKeypress(row, ["BTN_MIDDLE"])` already makes a button emit a middle click.** The blocker is purely that the **GUI KeyCaptureField captures keyboard keys, not mouse buttons**, and there's no UI affordance offering BTN_*.

### Minimal daemon change to add a first-class mouse-button action
A dedicated action is cleaner than overloading Keypress (clearer UI, correct summary, validates to the known BTN set). Sketch:

1. **Schema** (`src/logid/config/schema.h`): add a struct + variant entry.
   ```cpp
   struct ChangeButton : public signed_group<std::string> {
       typedef actions::ChangeButton action;
       std::optional<std::variant<std::string, uint>> button; // "BTN_MIDDLE" | code
       ChangeButton() : signed_group<std::string>(
           "type", "ChangeButton", {"button"}, &ChangeButton::button) {}
   };
   ```
   Add `ChangeButton` to the `Action` variant (line 231-241) AND forward-declare `class ChangeButton;` (line 24-52).

2. **Action class** `src/logid/actions/ChangeButton.{h,cpp}` — model on `KeypressAction`:
   - `interface_name = "ChangeButton"`, IPC `GetButton`/`SetButton`.
   - `_setConfig`: resolve name→code via `virtualInput()->toKeyCode`, `registerKey(code)`, store.
   - `press()` → `virtualInput()->pressKey(code)`; `release()` → `releaseKey(code)`.
   - `reprogFlags()` → `hidpp20::ReprogControls::TemporaryDiverted`.
   - **Validate** the code is in the BTN_* range (reject arbitrary KEY_* to keep the action honest) — but it physically works for any EV_KEY.

3. **Factory** `src/logid/actions/Action.cpp:62-83` — add `else if (name == ChangeButton::interface_name) config = config::ChangeButton();` and `#include <actions/ChangeButton.h>`.

4. **Build** — add `actions/ChangeButton.cpp` to `src/logid/CMakeLists.txt`.

5. **GUI** — add `"ChangeButton"` to `kActionTypes` (`ButtonsModel.cpp:42`), a `setChangeButton(row, "BTN_MIDDLE")` two-step setter, a category in `ReassignPanel.qml`, a glyph + `defaultSummary` entry. Offer a fixed list (Middle, Back, Forward, Right, Side, Extra).

**Effort:** ~1 new action class mirroring KeypressAction + 5 wiring edits. Low risk — reuses the proven uinput path.

### "Restore default" vs "assign middle-click" — important distinction (VERIFIED)
These are **different operations**, and the current "None"/Disabled does the WRONG one for "restore default":
- `NullAction::reprogFlags()` returns `TemporaryDiverted` (`src/logid/actions/NullAction.cpp:40-41`). So selecting **"None"/Disabled keeps the button DIVERTED to the daemon and makes it do nothing** — the middle button becomes *dead*, NOT a normal middle click.
- **Restoring hardware default** = *un-diverting* the control so the device handles it natively. In the factory, the special name **`"Default"`** does this: `Action.cpp:78-80` → `config.reset(); return nullptr;` — a null action means `RemapButton`'s `ConfigFunction` sends `report.flags = ChangeTemporaryDivert | ChangeRawXYDivert` **without** the `TemporaryDiverted` bit (`RemapButton.cpp:57-73`), i.e. divert OFF → hardware default restored.

**=> The GUI's "Disabled" (`clearAction` → `SetAction("None")`, `ButtonsModel.cpp:225-230`) does NOT restore the native middle click. To restore default, the GUI must call `SetAction("Default")`, not `"None"`.** This is almost certainly a chunk of the user's "middle button" confusion: clicking "Disabled" kills the button instead of giving them their middle click back. We need two distinct UI choices: **"Restore default"** (`SetAction("Default")`, un-divert) vs **"Disabled"** (`SetAction("None")`, diverted-but-inert).

---

## 3. "Changing button assignments still bugs in the UI and in the clicks" — root cause(s)

### Live-apply to hardware: the daemon path is CORRECT (VERIFIED)
Unlike the gesture bug we just fixed, the button reassign **does** swap the live action object:
- `Button::IPC::setAction` (`src/logid/features/RemapButton.cpp:287-303`) resets `_button._action`, rebuilds it via `Action::makeAction`, then calls `_button.configure()`.
- `Button::configure()` (`RemapButton.cpp:252-255`) calls `_conf_func(_action)` → the `ConfigFunction` (`RemapButton.cpp:57-73`) which calls `setControlReporting` with the new action's `reprogFlags()` — **re-diverting with the new action live**.
- The HID++ event handler (`RemapButton.cpp:117-138`) dispatches to `_buttons[cid]->press()/release()`, and `Button::press()` (`RemapButton.cpp:224-229`) reads `_action` under a shared lock — so the freshly-swapped pointer is used immediately.

**Conclusion:** SetAction reaches hardware live. So if clicks still "bug", suspects are (a) the param step never lands, (b) "None" leaves the button dead (see §2 restore-default), or (c) a UI desync.

### Bug 1 — "None" / Disabled makes the button dead, not default (see §2)
Highest-probability "clicks still buggy" cause. Fix: add a **"Restore default"** action mapping to `SetAction("Default")`; keep "Disabled" as `"None"`. `ReassignPanel.qml:306-312` currently only offers "Disabled" → `clearAction` → `"None"`.

### Bug 2 — param-call step 2 sequencing is fragile (VERIFIED, suspect)
`performParamCall` (`ButtonsModel.cpp:273-308`) does **not** chain off the actual `SetAction` reply. It fires an **independent** `Properties.GetAll` probe and assumes "same ordered connection ⇒ SetAction already processed," then sends the param setter from the probe's `finished` handler. Two problems:
- `performSetAction` and `performParamCall` are issued **back-to-back synchronously** from each setter (e.g. `setChangeDpi`, `ButtonsModel.cpp:152-161`). The param setter is gated on the *probe's* reply, not the *SetAction's* reply. With Qt's async dispatch they are separate watchers; ordering on a single `QDBusConnection` usually holds, but this is **implicit, not enforced** — a real race if the daemon reorders or the interface-creation is observable late.
- If `SetAction` **errors** (e.g. raced non-remappable, or daemon refuses), `performSetAction`'s handler swallows it (`ButtonsModel.cpp:262-270`) and `performParamCall` still fires the param setter at a non-existent interface → silent no-op. The row already shows the optimistic summary → **UI says "Change DPI +50" but hardware never got the param.**

**Fix direction:** chain step 2 inside `performSetAction`'s `finished` handler (only on success), passing the param call as a continuation — instead of the separate GetAll-probe hack. On `SetAction` error, revert the optimistic row + don't fire param.

### Bug 3 — binding list is optimistic-only, never read back (VERIFIED, desync source)
`applyCurrentAction` (`ButtonsModel.cpp:232-244`) sets `currentActionType`/`Summary` from the **GUI's intent**, emits `dataChanged`, marks dirty. There is **no read-back** from the daemon after the calls land. Consequences:
- If step 2 silently fails (Bug 2) or the daemon rejects, the list shows the new binding while hardware keeps the old one → exactly "bugs in the UI and in the clicks."
- The accent dot in `ReassignPanel` (`currentType` via `roleData(5)`) and the `BindingList` summary both trust this optimistic state; they will look correct while being wrong.

**Fix direction:** after the final param reply lands, re-read the button's present `.Action.<X>` interface (the same probe `enumerate()` uses, `ButtonsModel.cpp:349-367`) and reconcile the row to the **actual** daemon state; on mismatch, correct the row and emit `dataChanged`.

### Bug 4 — `ReassignPanel.roleData` calls `buttonsModel.rowCount()` as a function (LOW, minor)
`ReassignPanel.qml:31-33` guards with `row >= buttonsModel.rowCount()`. `rowCount` is exposed as the `count` NOTIFY property (`ButtonsModel.h:71`); calling `rowCount()` works but won't re-evaluate on row changes the way binding to `count` would. Low impact (panel row is set explicitly), but prefer `buttonsModel.count`.

### What is NOT the bug
- Daemon live-swap (verified correct, §3 top).
- Gesture-wipe and ScrollView clip — already fixed (per recent commits).
- Session/polkit access — resolved (user runs GUI directly post-reboot).

### Recommended fix order
1. **Restore-default vs Disabled** (`SetAction("Default")`) — fixes the most visible "middle button dead" symptom. (GUI-only)
2. **Read-back reconciliation** after reassign — kills UI/hardware desync. (GUI-only)
3. **Chain step 2 off SetAction success** — removes the silent param-drop race. (GUI-only)
4. **Add ChangeButton action** — true middle/back/forward assignment + parity. (daemon + GUI)

---

## Assumptions Log
| # | Claim | Section | Risk if Wrong |
|---|---|---|---|
| A1 | Options+ exposes mouse-button re-emit (middle/back/fwd) as assignable actions | §1.A | Low — parity scope only |
| A2 | Most Options+ "system actions" reduce to keystrokes or a session helper | §1.C/F | Medium — some may need compositor-specific calls (Phase 5/7) |
| A3 | `BTN_*` names resolve via `libevdev_event_code_from_name(EV_KEY, ...)` | §2 | Low — standard libevdev; verify on target by sending `setKeypress(["BTN_MIDDLE"])` once |

## Open Questions
1. Does `SetAction("Default")` survive a `Configuration::save()` round-trip as "no action key" (un-diverted) rather than persisting `type="None"`? Verify the serialization writes nothing for a reset button (config.reset() at `Action.cpp:79`).
2. Is the daemon's single ordered `QDBusConnection` guarantee strong enough that Bug 2's probe hack has never actually raced, or has the user hit it? On-hardware check: reassign DPI change rapidly and confirm the param lands.

## Sources
### Primary (HIGH) — code, file:line verified
- `src/logid/config/schema.h:231-241` (Action variant — no mouse button)
- `src/logid/InputDevice.cpp:48-89` (EV_KEY broad enable, registerKey bounds)
- `src/logid/actions/KeypressAction.cpp:66-100` (BTN_* resolvable via toKeyCode)
- `src/logid/actions/NullAction.cpp:40-41` ("None" stays diverted)
- `src/logid/actions/Action.cpp:62-86` ("Default" → config.reset → un-divert)
- `src/logid/features/RemapButton.cpp:57-73, 252-303` (live action swap + divert flags)
- `src/logiops-gui/ButtonsModel.cpp:42, 136-308` (kActionTypes, optimistic update, two-step race)
- `src/logiops-gui/qml/config/ReassignPanel.qml:306-312` ("Disabled" → "None")

### Secondary (MEDIUM) — web, Options+ capability map
- [Programming Buttons and Keys in Options+ — Logitech Hub](https://hub.sync.logitech.com/options/post/programming-buttons-and-keys-in-options-ntwM6VsAEKhHACY)
- [Configure the MX Master mouse with Logitech Options](https://support.logi.com/hc/en-001/articles/360023423313-Configure-the-MX-Master-mouse-with-Logitech-Options)
- [Mouse Gestures Setup & Customization Guide | Logitech](https://www.logitech.com/en-us/discover/a/mouse-gestures-setup)
- [How do I customize buttons and gestures with Logi Options+ on the MX Master 3S? — TechBabble](https://techbabble.co.uk/2025/10/29/how-do-i-customize-buttons-and-gestures-with-logi-options-on-the-mx-master-3s/)

## RESEARCH COMPLETE
