# Phase 4: Fine-Grained Gesture Control - Context

**Gathered:** 2026-05-30
**Status:** Ready for planning
**Mode:** Smart discuss (autonomous)

<domain>
## Phase Boundary

Deliver a guided GUI gesture builder so a user can configure a gesture-capable button's per-direction gestures (direction → mode → action → granularity) without editing `logid.cfg`, and fix the concrete pain that motivated this phase: gestures that overshoot ("volume steps by 2") or under-deliver ("one gesture = only one desktop switch"). The daemon's existing gesture engine (`actions/gesture/`: Axis / OnInterval / OnRelease / OnThreshold / NoPress, mapped per direction) is the foundation; the one daemon gap — switching a direction's gesture MODE live over D-Bus — is closed here ("first daemon extension"). Per-direction is the four cardinal directions only (matches the daemon).

Out of scope: diagonal/8-way gestures, the action wheel (Phase 6), per-app profile auto-switching (Phase 5).
</domain>

<decisions>
## Implementation Decisions

### Gesture Builder Model & Flow
- The gesture builder lives **inside the existing Buttons reassign panel** as the "Gestures" action category, surfaced when a gesture-capable button is selected (the daemon models a gesture as a button action — `config::GestureAction` is a member of the `Action` variant). Reuses Phase 3's `ReassignPanel`/`ButtonsModel` infrastructure.
- Modes are presented in **plain language**, mapped to daemon modes: "Repeat while moving" → `OnInterval`; "Do once when moved far enough" → `OnThreshold`/`OnRelease`; "Adjust proportionally" → `Axis`; "Nothing" → `NoPress`.
- **Granularity** is a slider over the mode's `interval`/`threshold` with a live readout ("~1 step per X of movement"), not a raw number field — makes the "one step" concept tangible without HID++ units.
- **Four cardinal directions** only (up/down/left/right), exactly what the daemon exposes at `/buttons/N/gestures/{up,down,left,right}`. No daemon extension needed for direction set.

### Plain-Language Explanation (explicit requirement)
- A **live preview sentence** below the builder, updating as mode/granularity/action change (e.g. "Moving up far enough switches one desktop; keep moving to switch more"). Directly satisfies "the UI explains when and how often the action will fire."

### Daemon Extension Scope
- Add a **per-direction "set gesture mode" mechanism over D-Bus** (analogous to the button `SetAction` two-step in `ButtonsModel`), since the interface name encodes the mode (`pizza.pixl.LogiOps.Gesture.<mode>`) and switching mode currently requires editing the cfg. This is the phase's "first daemon extension."
- Per-mode parameter setters already exist and are reused: `Gesture.Axis` → `SetThreshold`/`SetMultiplier`; `Gesture.OnRelease`/`OnThreshold` → `SetThreshold`/`SetAction`; `Gesture.OnInterval` → `SetThreshold`/`SetAction`/`SetInterval`. No new param setters needed beyond mode-switch wiring.
- **Investigate and fix the "volume +2" / "one desktop per gesture" bug** as a sub-task: reproduce against `AxisGesture::axis_multiplier` handling and `IntervalGesture` interval accounting; fix in the daemon if a defect is confirmed (not just expose the knob).

### Apply & Persistence
- Same model as Phase 3: **live-apply** to the device on change → "Unsaved changes" pill → **polkit-gated Save** writes `logid.cfg`. Consistent dirty-tracking via the existing `ConfigState`/`markDirty` seam.

### Visual Fidelity
- Per standing user feedback ("ridículo de feio, porém funcional"), the gesture builder must push toward genuine Logitech Options+ polish, not the bare functional skeleton. Detailed visual/motion contract is deferred to the UI-SPEC (next step) but is an explicit quality bar for this phase.
</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/logiops-gui/ButtonsModel.{h,cpp}` — two-step async `SetAction` pattern to mirror for live gesture mode-switch; CID-keyed rows; dirty-tracking wired.
- `src/logiops-gui/qml/config/ReassignPanel.qml` + `KeyCaptureField.qml` — the action picker the gesture category slots into; key-capture reusable for gesture actions.
- `src/logiops-gui/ConfigState.{h,cpp}` — dirty pill + async polkit Save; reuse `markDirty()` and `.Config.Save`.
- Daemon: `src/logid/actions/gesture/` — `Gesture.cpp` (base IPC `pizza.pixl.LogiOps.Gesture.<mode>`), `AxisGesture` (`SetThreshold`/`SetMultiplier`), `ReleaseGesture`/`ThresholdGesture`/`IntervalGesture` param setters; `config/schema.h` gesture structs (`threshold`, `action`, `interval`, `axis`, `axis_multiplier`).
- Daemon D-Bus already exposes `/devices/N/buttons/M/gestures/{up,down,left,right,none}` nodes per gesture-capable button.

### Established Patterns
- C++ models + typed D-Bus proxies (qdbusxml2cpp); QML renders only, no business logic in JS.
- Capability gating by D-Bus Introspect of present interfaces (`DeviceController`).
- NOTIFYable `count`/capability properties so QML reacts to async data (lesson from Phase 3 fixes).

### Integration Points
- Gesture category enters the Buttons tab's `ReassignPanel` when `GestureSupport` is true for the selected button.
- New daemon mode-switch method extends the per-gesture-node interface (or the button node), then a new typed proxy/model method in the GUI drives it.
</code_context>

<specifics>
## Specific Ideas

- Concrete target pains to validate against (project owner, MX Master 4): horizontal/vertical gesture → "switch exactly one desktop per flick" and "volume changes by exactly one tick." These are the acceptance feel-tests.
- The daemon's `Gesture.<mode>` interface name encoding is the reason a dedicated mode-switch call is needed rather than a property set.
</specifics>

<deferred>
## Deferred Ideas

- Diagonal / 8-direction gestures (would require extending the daemon's direction model).
- Animated/visual gesture demonstration (text preview sentence ships first).
- Action wheel (Phase 6); per-app profile switching (Phase 5).
</deferred>
