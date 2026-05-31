# Phase 4: Fine-Grained Gesture Control - Research

**Researched:** 2026-05-31
**Domain:** logiops daemon gesture engine (`src/logid/actions/gesture/`) + Qt 6.4.2 / QML gesture-builder GUI (`src/logiops-gui/`), wired over ipcgull D-Bus
**Confidence:** HIGH (every claim grounded in the in-tree source; web verification not used, none needed — this is a closed brownfield codebase)

## Summary

The gesture engine already exists and is richer than the phase brief assumes. A gesture is a button action: `config::GestureAction` is a member of the `Action` variant, exposes the D-Bus interface `pizza.pixl.LogiOps.Action.Gesture` with a **`SetGesture(direction, type)` method that already exists** (`GestureAction.cpp:226`), and owns a `std::map<Direction, shared_ptr<Gesture>>` of per-direction gesture objects published at child nodes `.../buttons/M/gestures/{up,down,left,right,none}`. Each gesture object publishes exactly one interface whose name encodes its mode (`pizza.pixl.LogiOps.Gesture.<Axis|OnInterval|OnRelease|None>`), and `SetGesture` already does the live rebuild: it releases the old gesture, resets the `shared_ptr`, mutates the config variant in place, and re-makes the gesture interface on the same node. **The "first daemon extension" is therefore mostly already implemented** — the real daemon work shrinks to (a) safety-hardening `SetGesture` against untrusted input, (b) closing a concrete interface-name collision and arg-name bug, and (c) fixing the granularity/overshoot defect (GEST-02).

The motivating "volume +2 / one desktop per flick" pain is a **real, locatable defect**, not just a tuning knob. `IntervalGesture::move` (`IntervalGesture.cpp:63-82`) fires its action once per `(_axis - threshold) / interval` boundary crossing; with a default `threshold` of 50 (`Configuration.h:34`) and an unset `interval` it does nothing at all, and when `interval` is small relative to one physical flick it fires many times (overshoot). `AxisGesture` carries a remainder correctly but its `axis_multiplier` defaults to 1 with no sane "one notch per flick" preset. The fix is partly a real off-by-one/threshold-accounting change in the daemon and partly establishing granularity *defaults* the GUI slider's leftmost stop maps to.

**Primary recommendation:** Treat the daemon side as a *small, surgical* extension — harden the existing `SetGesture`, fix two concrete bugs (the `OnRelease`/`OnThreshold` interface-name collision and the `IntervalGesture::SetThreshold` arg-name mislabel), and define granularity defaults — then build the GUI gesture builder as a new `GestureModel` mirroring `ButtonsModel`'s two-step async pattern plus a typed `Action.Gesture` proxy and per-mode generic `QDBusInterface` param setters. Persistence and dirty-tracking reuse the Phase 3 `ConfigState` seam unchanged (gesture config already lives in `schema.h` under `GestureAction.gestures` and already round-trips through `Configuration::save()`).

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Gesture builder lives INSIDE the existing Buttons reassign panel** as the "Gestures" action category, surfaced when a gesture-capable button is selected (`config::GestureAction` is a member of the `Action` variant). Reuses Phase 3's `ReassignPanel`/`ButtonsModel` infrastructure.
- **Modes presented in plain language**, mapped to daemon modes: "Repeat while moving" → `OnInterval`; "Do once when moved far enough" → `OnThreshold`/`OnRelease`; "Adjust proportionally" → `Axis`; "Nothing" → `NoPress`.
- **Granularity is a slider** over the mode's `interval`/`threshold` with a live readout ("~1 step per X of movement"), not a raw number field.
- **Four cardinal directions only** (up/down/left/right), exactly what the daemon exposes at `/buttons/N/gestures/{up,down,left,right}`. No daemon extension for direction set.
- **Live preview sentence** below the builder, updating as mode/granularity/action change (GEST-04).
- **Daemon extension:** add a per-direction "set gesture mode" mechanism over D-Bus (analogous to the button `SetAction` two-step), since the interface name encodes the mode. Per-mode parameter setters already exist and are reused (`Axis`→`SetThreshold`/`SetMultiplier`; `OnRelease`/`OnThreshold`→`SetThreshold`/`SetAction`; `OnInterval`→`SetThreshold`/`SetAction`/`SetInterval`). **No new param setters beyond mode-switch wiring.**
- **Investigate and fix the "volume +2" / "one desktop per gesture" bug** as a sub-task: reproduce against `AxisGesture::axis_multiplier` and `IntervalGesture` interval accounting; fix in the daemon if a defect is confirmed.
- **Apply & persistence:** same as Phase 3 — live-apply on change → "Unsaved changes" pill → polkit-gated Save writes `logid.cfg`, via the existing `ConfigState`/`markDirty` seam.
- **Visual fidelity:** push toward genuine Options+ polish (per UI-SPEC), not a bare skeleton.

### Claude's Discretion
- The exact granularity-default values that produce "one flick = one step" (researched below; needs on-hardware confirmation against the MX Master 4).
- The concrete daemon API surface for hardening/extending `SetGesture` and how readback/enumeration is exposed (recommended below).
- Whether to fix the `OnRelease`/`OnThreshold` interface-name collision now or work around it in the GUI (recommended: address it; see Pitfall 1).

### Deferred Ideas (OUT OF SCOPE)
- Diagonal / 8-direction gestures (would require extending the daemon's direction model).
- Animated/visual gesture demonstration (text preview sentence ships first).
- Action wheel (Phase 6); per-app profile switching (Phase 5).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| GEST-01 | Build a gesture by picking direction → mode → action through a guided UI | Daemon already exposes `Action.Gesture` + per-direction `gestures/{dir}` nodes with `SetGesture(dir,type)` (mode) and per-mode `Set*` param setters. GUI: new `GestureModel` mirroring `ButtonsModel`'s two-step pattern + a typed `Action.Gesture` proxy; new "Gesture" `CategoryRow` in `ReassignPanel.qml` gated on `GestureSupportRole` (already a model role). |
| GEST-02 | Control gesture granularity so one gesture = one discrete step — fix "volume +2"/"only 1 desktop" by exposing interval/threshold/multiplier with sane defaults | Confirmed real defect (see Pitfall 2 / Common Pitfalls). Fix in `IntervalGesture::move` accounting + define `defaults` for interval/threshold/multiplier that map to the slider's leftmost stop. Param setters `SetInterval`/`SetThreshold`/`SetMultiplier` already exist. |
| GEST-03 | Make a gesture repeat per interval (switch multiple desktops in one motion) | `IntervalGesture` (`OnInterval` mode) already implements repeat-per-interval; the GUI's "Repeat while moving" pill + granularity slider drives its `interval`. Fix accounting so each interval = exactly one action fire. |
| GEST-04 | The gesture UI explains in plain language when/how often an action fires | Pure GUI: a NOTIFYable preview-sentence string composed in C++ from {direction, mode, action, granularity}, exposed on `GestureModel` and bound by the QML preview card (UI-SPEC §5). No daemon work. |
</phase_requirements>

## Standard Stack

This phase adds **no new third-party libraries**. It extends the existing daemon and GUI stacks.

### Core (already in tree — extend, do not add)
| Component | Version / Location | Purpose | Why Standard |
|-----------|--------------------|---------|--------------|
| Daemon gesture engine | `src/logid/actions/gesture/` | The 5 gesture modes (`Axis`, `OnInterval`, `OnRelease`(threshold), `OnRelease`(release), `None`) + `GestureAction` owner | Already implements per-direction gesture firing; `SetGesture` already does live mode rebuild |
| ipcgull (vendored) | `src/ipcgull/` (in-tree, NOT a submodule per MEMORY) | D-Bus object/interface publishing; `make_interface` / `make_child` | The mechanism `SetGesture` uses to re-register a gesture node's interface live |
| libconfig++ | distro `config++` | `Configuration::save()` serialization of the gesture config tree | Gesture config already serializes (the `gestures` map is in `schema.h`) |
| Qt 6.4.2 + QML / Qt Quick Controls 2 | distro `Qt6` | GUI gesture builder | Phase 2/3 stack; pinned to 6.4.2 features only (no `colorScheme`, no 6.7+ `font.features` tnum, no 6.5+ `loadFromModule`) |
| qdbusxml2cpp via `qt_add_dbus_interface` | Qt6::DBus | Typed proxy generation from trimmed introspection XML | The established proxy pattern; 8 proxies already generated in `src/logiops-gui/CMakeLists.txt` |
| Qt6::Test (QtTest) | Qt6 | GUI-side unit tests with injected fakes / recording subclasses | Phase 3 `ButtonsModelTest`/`DeviceControllerTest` precedent |

### Supporting (reuse verbatim)
| Component | Location | When to Use |
|-----------|----------|-------------|
| `ButtonsModel` two-step pattern | `src/logiops-gui/ButtonsModel.cpp` | The exact template for `GestureModel`: `performSetAction`/`performParamCall` virtuals, `QDBusPendingCallWatcher` sequencing, `seedButtons()`-style test seam, NOTIFYable `count`/props |
| `ConfigState` | `src/logiops-gui/ConfigState.{h,cpp}` | `markDirty()` on every gesture edit; async polkit-gated `.Config.Save` (no new save path — UI-SPEC §"Live-apply, dirty, Save") |
| `DeviceController` Introspect gating | `src/logiops-gui/DeviceController.{h,cpp}` | Capability discovery; gesture support is per-button (`GestureSupportRole` already on `ButtonsModel`), not per-device — gate the new category on the row's `gestureSupport` |
| `KeyCaptureField.qml` + categorized action chooser | `src/logiops-gui/qml/config/` | The gesture's Action sub-section reuses the *exact same* action picker (UI-SPEC §3) |
| `ReassignPanel.qml` `CategoryRow` + `expanded` state | `src/logiops-gui/qml/config/ReassignPanel.qml` | The gesture builder is a new `CategoryRow` with inline expansion, mirroring ChangeDPI/ChangeHost/ChangeProfile rows |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| New `GestureModel` C++ class | Driving everything from `ButtonsModel` | `ButtonsModel` is row=button; gesture builder is "the 4 directions of ONE selected button" — a distinct, button-scoped model is cleaner and keeps `ButtonsModel` focused. Recommend a dedicated `GestureModel` constructed for the selected button's path. |
| Typed `Action.Gesture` proxy via qdbusxml2cpp | Generic `QDBusInterface` for everything | The button-level `SetGesture` and the per-mode param setters use *dynamic* interface names (`.Gesture.<mode>`); follow the Phase 3 precedent — generate a typed proxy ONLY for the stable `Action.Gesture` (with `SetGesture`), drive the per-mode `.Gesture.<mode>` setters via generic `QDBusInterface` (exactly как `ButtonsModel::performParamCall` does for `.Action.<type>`). |

**Installation:** none (no new packages). Build wiring only — see "Qt/QML Build Wiring" pitfall.

**Version verification:** No registry packages are added. Qt is pinned at **6.4.2** (confirmed across Phase 3 SUMMARYs: `03-03-SUMMARY.md:134`). Honor the 6.4.2 ceiling constraints already learned in Phase 2/3 (no `colorScheme`, no `font.features` tnum → use `TextMetrics` for the tabular granularity readout, no `loadFromModule`).

## Architecture Patterns

### Recommended structure (additions only)
```
src/logid/actions/gesture/        # daemon: bug-fix + harden SetGesture (existing files)
  Gesture.cpp                      #   makeGesture(type,...) validates type set (harden)
  IntervalGesture.cpp              #   FIX interval accounting (GEST-02/03) + arg-name bug
  ThresholdGesture.cpp/.h          #   FIX interface_name collision with ReleaseGesture
src/logid/actions/GestureAction.cpp#   harden setGesture() against untrusted direction/type
src/logiops-gui/
  GestureModel.{h,cpp}             # NEW: button-scoped, 4-direction model; two-step mode-switch
  dbus/logid_action_gesture.xml    # NEW: typed proxy for Action.Gesture (SetGesture)
  qml/config/
    GestureBuilder.qml             # NEW: the builder (direction cross, mode pills, slider, preview)
  icons/gesture.svg, icons/arrow.svg  # NEW glyphs (UI-SPEC §Iconography)
tests/phase4/
  CMakeLists.txt                   # NEW: mirror tests/phase3 (CTest `phase4` prefix)
  GestureModelTest.cpp             # NEW: recording-subclass, no-bus, two-step + preview logic
  gesture_math_test.cpp            # NEW: daemon-side interval/axis accounting (GEST-02) RED→GREEN
```

### Pattern 1: Two-step live mode-switch (mirror `ButtonsModel::setAction`)
**What:** Switching a direction's MODE is a button-node call (`Action.Gesture.SetGesture(dir, mode)`) followed, after its reply lands, by the per-mode param setters on the now-rebuilt `.../gestures/{dir}` node.
**When to use:** Every mode change in the builder.
**Why it must be two-step:** `SetGesture` drops the old `.Gesture.<oldmode>` interface and creates `.Gesture.<newmode>` (`GestureAction.cpp:245-255`). The new mode's param setters (`SetInterval`, `SetMultiplier`, etc.) don't exist until `SetGesture` has run — identical to the `.Action.<type>` ordering hazard `ButtonsModel` already solves.
**Example (the daemon side that already exists):**
```cpp
// Source: src/logid/actions/GestureAction.cpp:226 (EXISTING — not new)
void GestureAction::setGesture(const std::string& direction, const std::string& type) {
    std::unique_lock lock(_config_mutex);
    Direction d = toDirection(direction);                 // throws std::invalid_argument on bad dir
    auto it = _gestures.find(d);
    if (it != _gestures.end()) { /* release the live gesture if pressed */ }
    auto dir_name = fromDirection(d);
    auto& gesture = _config.gestures.value()[dir_name];   // mutate config variant in place
    _gestures[d].reset();
    try {
        _gestures[d] = Gesture::makeGesture(_device, type, gesture,
                                            _node->make_child(dir_name));   // re-register interface
    } catch (InvalidGesture& e) { /* fall back to current config + rethrow invalid_argument */ }
}
```
The GUI mirror is structurally `ButtonsModel::performSetAction` + `performParamCall`, retargeted at the `Action.Gesture` interface and the `.Gesture.<mode>` child node.

### Pattern 2: NOTIFYable preview sentence composed in C++ (GEST-04)
**What:** The model exposes a single `Q_PROPERTY(QString previewSentence ... NOTIFY previewChanged)` recomposed whenever {active direction, mode, action, granularity} changes. QML binds it read-only (UI-SPEC §"Rendering model": zero business logic in QML/JS).
**When to use:** GEST-04. Templates are locked in UI-SPEC §Copywriting.

### Pattern 3: Capability gating by present-interface (reuse)
**What:** The gesture category row is shown only when the selected button reports `gestureSupport == true`. This role is **already populated** by `ButtonsModel` from the `.Button` property `GestureSupport` (`ButtonsModel.cpp:341`). No new introspection needed for the gate.

### Anti-Patterns to Avoid
- **Inventing a new "SetMode" daemon method.** `SetGesture(direction, type)` already IS the mode-switch. Adding a parallel method duplicates the rebuild logic and risks divergence. Harden the existing one instead.
- **Business logic in QML/JS.** Locked project rule (UI-SPEC). The preview sentence, the granularity↔human-readout mapping, and the mode→plain-language mapping all live in C++/`GestureModel`.
- **A second Save path.** Reuse the inherited `ConfigState` Unsaved pill + polkit Save verbatim (UI-SPEC §Live-apply).
- **Trusting the direction/type string at the bus boundary.** The daemon runs as root parsing this; validate against the known set (see Security Domain).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Live mode rebuild + interface re-registration | A new daemon mode-switch path | Existing `GestureAction::setGesture` (`Action.Gesture.SetGesture`) | Already releases old gesture, mutates config, re-makes the ipcgull interface on the same node |
| Two-step async D-Bus ordering | A new sequencing helper | Copy `ButtonsModel::performSetAction`/`performParamCall` (`QDBusPendingCallWatcher` chain) | Phase 3 already solved the "param interface doesn't exist until step 1 lands" hazard |
| Dirty-tracking + polkit save | A gesture-specific save | `ConfigState::markDirty()` + `.Config.Save` | Gesture config is already in the same `schema.h` tree and already round-trips |
| Config persistence of gestures | New serialization | `Configuration::save()` (unchanged) | `GestureAction.gestures` map already serializes (proven by `logid.example.cfg` gesture blocks) |
| Host/string validation pattern | Ad-hoc checks | The `ButtonsModel::setChangeHost` validate-before-dispatch idiom | Same untrusted-input class as the gesture direction/type strings |
| Tabular numeric readout (no reflow) | `font.features` tnum (Qt 6.7+) | `TextMetrics` width pin (Phase 2 lesson) | Target is Qt 6.4.2 |

**Key insight:** This phase is ~70% "wire the GUI to an engine that already works" and ~30% "fix two real daemon bugs + harden one method." The daemon-extension framing is correct but the surface is small — most of the gesture machinery (per-direction nodes, mode encoding, live rebuild, param setters, serialization) already ships.

## Runtime State Inventory

> Rename/refactor-style audit. Phase 4 is feature work, not a rename, but it mutates live daemon objects and persisted config, so the live-vs-persisted distinction matters.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | Gesture config persisted in `/etc/logid.cfg` under each button's `action.gestures` map (`schema.h:220` `GestureAction.gestures`). Already serialized by `Configuration::save()`. | None new — reuse the Phase 3 Save path. A gesture edit dirties `ConfigState`; Save writes the whole tree. |
| Live service config | The daemon holds live `shared_ptr<Gesture>` objects per direction in `GestureAction::_gestures`, published as ipcgull nodes `.../gestures/{up,down,left,right,none}`. `SetGesture` rebuilds these live; param setters mutate the live `config::*Gesture&` reference. | Both code edit (harden/fix) AND a behavioral change to live objects. The GUI's live-apply path drives these; persistence is a separate Save. |
| OS-registered state | None — gestures synthesize input via the existing `InputDevice`/uinput virtual device (`AxisGesture` registers an axis; no new OS registration). | None — verified: gesture firing reuses `_device->virtualInput()` already set up. |
| Secrets/env vars | None. | None — verified: no secrets touch gestures. |
| Build artifacts | qdbusxml2cpp generates `logid_action_gesture_proxy.{h,cpp}` into `CMAKE_CURRENT_BINARY_DIR`; the QML module's `.rcc`/qmlcache is regenerated on QML-file-list change. | Add the new XML to `qt_add_dbus_interface` and the new QML files to `qt_add_qml_module QML_FILES`; clean rebuild if qmlcache goes inconsistent (Phase 3 lesson). |

**The canonical question — after a gesture edit, what state is where?** Live: the rebuilt `Gesture` objects + mutated `config::*Gesture` in daemon memory (applied immediately). Persisted: nothing until `.Config.Save` writes `/etc/logid.cfg`. This is exactly the Phase 3 live-apply/dirty/Save model — no new persistence surface.

## Common Pitfalls

### Pitfall 1 (CRITICAL — daemon defect): `OnRelease` interface-name collision
**What goes wrong:** `ReleaseGesture::interface_name == "OnRelease"` (`ReleaseGesture.cpp:24`) AND `ThresholdGesture::interface_name == "OnRelease"` (`ThresholdGesture.cpp:25`). Two distinct gesture classes publish the **same** D-Bus interface name `pizza.pixl.LogiOps.Gesture.OnRelease`. But in the config/`makeGesture` `type`-string path they are keyed by the config `signed_group` *mode* names `"OnRelease"` (ReleaseGesture) vs `"OnThreshold"` (ThresholdGesture, `schema.h:195`) — yet `Gesture::makeGesture(type,...)` (`Gesture.cpp:74-86`) only branches on `interface_name`, so `type == "OnThreshold"` matches **nothing** and throws `InvalidGesture`. The UI-SPEC's mode 2 ("Do once when moved far enough" → `OnThreshold`/`OnRelease`) therefore cannot be selected by the obvious string.
**Why it happens:** `ThresholdGesture` was added later (note the different copyright header: "PixlOne, michtere") and reused `OnRelease` as its `interface_name` while declaring `OnThreshold` as its config mode.
**How to avoid / fix:** Decide a single canonical mode 2 for this phase. Recommended: **use `OnRelease`/`ReleaseGesture` as the "Do once when moved far enough" mode** (it is the one `makeGesture(type)` actually constructs and what `logid.example.cfg` uses). Either (a) give `ThresholdGesture` its own `interface_name = "OnThreshold"` and add it to the `makeGesture(type)` switch, or (b) explicitly scope mode 2 to `OnRelease` and document `OnThreshold` as out of this phase's GUI surface. The planner MUST pick one; do not let the GUI send `"OnThreshold"` and silently get an `InvalidGesture`.
**Warning signs:** Selecting mode 2 in the builder throws / leaves the node on its previous mode.

### Pitfall 2 (CRITICAL — the GEST-02 motivating bug): interval/threshold overshoot & under-trigger
**What goes wrong:** Two concrete defects in `IntervalGesture::move` (`IntervalGesture.cpp:63-82`):
1. `if (!_config.interval.has_value()) return;` — if `interval` is unset the gesture **never fires** ("one desktop only" can manifest as the release-path firing once while the interval-repeat does nothing). There is **no default interval** (unlike `threshold`, which defaults to 50). GEST-03's "switch multiple desktops" depends on a sane default interval.
2. The fire count is `new_interval_count = (_axis - threshold) / interval` and fires when it increases. With a default `threshold` of 50 and a small `interval`, one physical flick of, say, 120 units past threshold yields `120/interval` fires — e.g. `interval=20` → 6 desktop switches per flick (overshoot). The "volume +2" symptom is the same class: each report batch can cross multiple interval boundaries, firing the action more than once per perceived flick.
3. **Arg-name bug:** `IntervalGesture`'s `SetThreshold` registers its argument as `{"interval"}` (`IntervalGesture.cpp:34`) — a copy-paste mislabel; the introspection will advertise the wrong arg name for `SetThreshold`. Cosmetic but should be fixed (`{"threshold"}`) since the GUI/introspection reads it.
**Why it happens:** No granularity default tuned to "one physical flick = one logical step," and integer division accumulates multiple crossings per report.
**How to avoid / fix (GEST-02):**
- Add a `defaults::gesture_interval` (alongside `gesture_threshold = 50` in `Configuration.h:34`) so `OnInterval` repeats sensibly when `interval` is unset; the GUI slider's leftmost ("one step per flick") stop maps to an interval ≈ one flick's worth of motion.
- The accounting in `move` already advances `_interval_pass_count` and fires only on *increase*, which is correct for "one fire per boundary." The overshoot is a **value** problem (interval too small), not (mostly) a logic problem — so the primary fix is the default + the slider mapping. **Confirm on-hardware** (MX Master 4) what unit-count one comfortable flick produces, and set the leftmost-stop default from that. `AxisGesture` (mode "Adjust proportionally") already carries `_axis_remainder` correctly (`AxisGesture.cpp:99-104`) and is not the bug — its only gap is `axis_multiplier` default = 1 with no "one notch" preset.
**Warning signs:** Volume jumps by 2+, desktop switches by 2+, or a "repeat" gesture that does nothing.

### Pitfall 3: `gestures` map may be absent / `None` direction special-casing
**What goes wrong:** `GestureAction::setGesture` does `_config.gestures.value()[dir_name]` — if `_config.gestures` is `std::nullopt` (a `GestureAction` with no gestures yet) this throws `std::bad_optional_access`. Also the `None` direction is special-cased to force `threshold = 0` (`GestureAction.cpp:257`).
**How to avoid:** When the GUI first turns a button into a gesture (via `Button.SetAction("Gesture")`), ensure the daemon initializes `gestures` to an empty map before `SetGesture` is called, or harden `setGesture` to lazily create the map. The planner should verify the construct-then-`SetGesture` sequence does not hit `bad_optional_access`. (The two-step is: `Button.SetAction("Gesture")` → then `Action.Gesture.SetGesture(dir, mode)`.)

### Pitfall 4: Qt 6.4.2 ceiling (inherited Phase 2/3 lessons)
**What goes wrong:** Using 6.5+/6.7+ APIs breaks the pinned build.
**How to avoid:** No `loadFromModule` (6.5+), no `font.features` tnum (6.7+) → use `TextMetrics` for the granularity readout's stable width; no `SystemPalette.colorScheme` → resolve dark via `SystemPalette.window.hslLightness < 0.5`. All inherited and already encoded in `Theme.qml`.

### Pitfall 5: qmlcache inconsistency on QML-file-list change
**What goes wrong:** Adding QML files to `qt_add_qml_module` without a clean `.rcc`/qmlcache rebuild leaves the qmlcache inconsistent (Phase 3 lesson, STATE.md). New comments/`--` or `<` characters in introspection XML silently break qdbusxml2cpp into empty proxies (Phase 2 lesson).
**How to avoid:** After editing `QML_FILES`/`GUI_DBUS_SRCS`, do a clean build; keep the new `logid_action_gesture.xml` free of `--` and bare `<` in comments.

## Code Examples

### Daemon: the existing mode-switch (what the GUI two-step targets)
```cpp
// Source: src/logid/actions/GestureAction.cpp (EXISTING)
// Interface: pizza.pixl.LogiOps.Action.Gesture  (GestureAction.cpp:78-86)
//   method SetGesture(string direction, string type)
// Per-direction child node interfaces: pizza.pixl.LogiOps.Gesture.<mode>
//   Axis     : GetConfig, SetThreshold, SetMultiplier, SetAxis   (AxisGesture.cpp:32-37)
//   OnInterval: GetConfig, SetInterval, SetThreshold, SetAction   (IntervalGesture.cpp:31-36)
//   OnRelease : GetThreshold, SetThreshold, SetAction             (ReleaseGesture.cpp:29-33)
//   None      : (no setters)                                      (NullGesture.cpp)
```

### GUI: the two-step mirror (adapt from ButtonsModel)
```cpp
// Source pattern: src/logiops-gui/ButtonsModel.cpp:250-306 (ADAPT, do not invent)
// Step 1: Action.Gesture.SetGesture(direction, mode)   -> typed Action.Gesture proxy
// Step 2 (after reply): pizza.pixl.LogiOps.Gesture.<mode>.Set*  -> generic QDBusInterface
//   at the .../gestures/{direction} child node, sequenced via QDBusPendingCallWatcher.
```

### GUI build wiring (add to src/logiops-gui/CMakeLists.txt)
```cmake
# Source pattern: src/logiops-gui/CMakeLists.txt:34-48
qt_add_dbus_interface(GUI_DBUS_SRCS dbus/logid_action_gesture.xml logid_action_gesture_proxy)
# add GestureModel.cpp to target_sources(logiops-gui-lib ...)
# add qml/config/GestureBuilder.qml to qt_add_qml_module(... QML_FILES ...)
# add icons/gesture.svg, icons/arrow.svg to RESOURCES
```

## State of the Art

| Old Approach | Current Approach | Where | Impact |
|--------------|------------------|-------|--------|
| Hand-edit `logid.cfg` `gestures:( ... )` blocks | Guided GUI builder over live D-Bus + Save | This phase | The whole point of GEST-01..04 |
| Assume a new daemon "set mode" method is needed | `Action.Gesture.SetGesture` already exists | `GestureAction.cpp:226` | Daemon work shrinks to hardening + bug-fix |

**Deprecated/outdated in the brief vs reality:**
- The brief's "the gesture node currently advertises ONE interface = its current mode, so a live set-mode must be ADDED" is accurate about the encoding but the *mechanism* (`SetGesture`) is already present at the parent `Action.Gesture` node. The extension is hardening, not greenfield.
- `OnFewPixels` (`FewPixelsGesture`, `schema.h:175`) exists as an `IntervalGesture` subclass with mode name `OnFewPixels` but has no distinct `interface_name` — out of scope for the 4 plain-language modes; do not surface it.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The "one flick = one step" granularity default value (interval/threshold/multiplier) must be derived from on-hardware MX Master 4 measurement; no exact unit-count is in the code | Pitfall 2 / GEST-02 | If the chosen default is wrong, the motivating bug persists — must be confirmed by human UAT on hardware |
| A2 | Recommending `OnRelease` (not `OnThreshold`) as the canonical "Do once" mode for the GUI, because `makeGesture(type)` only constructs `OnRelease` | Pitfall 1 | If the planner wants true `OnThreshold` semantics (fires mid-move at threshold vs on release), `ThresholdGesture` needs its own `interface_name` added to the `makeGesture` switch — a small daemon change |
| A3 | `GestureSupport` per-button property reliably indicates a gesture-capable control on the MX Master 4 | Capability gating | If false-negative, the gesture category hides on a capable button — verify on-hardware |
| A4 | qdbusxml2cpp on this machine generates `Action.Gesture` proxy cleanly from a trimmed XML (as it did for 8 Phase 3 interfaces) | Build wiring | Low — proven pattern; just avoid `--`/`<` in XML comments |

## Open Questions

1. **`OnThreshold` vs `OnRelease` semantics for mode 2.**
   - What we know: both publish `Gesture.OnRelease`; `ThresholdGesture` fires mid-move when threshold met, `ReleaseGesture` fires on button release if threshold met.
   - What's unclear: which "Do once when moved far enough" feel the owner wants (fire as soon as you've moved far enough, vs fire when you let go).
   - Recommendation: default to `OnRelease` (matches `makeGesture(type)` and the example cfg); if the owner wants fire-mid-move, add a distinct `OnThreshold` interface_name as a small daemon task. Flag for discuss/UAT.

2. **Exact granularity default (the GEST-02 acceptance feel-test).**
   - What we know: `gesture_threshold = 50`; no `gesture_interval` default; `axis_multiplier` defaults to 1.
   - What's unclear: how many HID++ units one comfortable MX Master 4 flick produces.
   - Recommendation: instrument on-hardware, set the slider's leftmost stop from the measured "one flick" value, and persist that as the new `defaults::gesture_interval`. This is a human-in-the-loop calibration (see Validation Architecture).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Qt6 (Core/DBus/Gui/Quick/Qml/Svg/Test) | GUI + tests | ✓ (assumed — Phase 2/3 built here) | 6.4.2 | — |
| qdbusxml2cpp | Typed `Action.Gesture` proxy | ✓ (8 proxies built in Phase 3) | bundled w/ Qt6::DBus | — |
| libconfig++ (`config++`) | gesture config Save round-trip | ✓ | 1.5 (per `03-00-SUMMARY`) | — |
| ipcgull (in-tree) | gesture node interface re-registration | ✓ (vendored, in-tree) | — | — |
| A real gesture-capable Logitech device | Live gesture firing UAT (GEST-02 feel-test) | ✓ (owner's MX Master 4) | — | Logic is unit-testable with fakes; only live firing needs hardware |

**Missing dependencies with no fallback:** none.
**Missing dependencies with fallback:** Live gesture *firing* cannot be unit-tested (needs HID++ + uinput on real hardware) — falls back to **human UAT**, exactly as Phase 3 deferred live reassign/key-capture to the phase gate.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | QtTest (GUI-side) + a daemon-side plain C++ CTest unit (mirroring `tests/phase3` round-trip units) |
| Config file | New `tests/phase4/CMakeLists.txt`, added via `add_subdirectory(tests/phase4)` in root `CMakeLists.txt` (mirror line ~80 where `tests/phase3` is added) |
| Quick run command | `ctest --test-dir build -R phase4 --output-on-failure` |
| Full suite command | `ctest --test-dir build --output-on-failure` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| GEST-01 | Mode-switch is a two-step SetGesture→param sequence in correct order; direction/mode strings validated; category gated on `gestureSupport` | unit (recording subclass, no bus) | `ctest -R phase4_gesture_model` | ❌ Wave 0 (`GestureModelTest.cpp`) |
| GEST-02 | Interval/axis accounting fires exactly once per logical step at the default granularity; remainder carried; no overshoot | unit (daemon math, RED→GREEN) | `ctest -R phase4_gesture_math` | ❌ Wave 0 (`gesture_math_test.cpp`) |
| GEST-02 | Live "one flick = one volume tick / one desktop" on real hardware | manual UAT | (human, MX Master 4) | n/a — human |
| GEST-03 | `OnInterval` repeats N times across a continuous motion = N interval boundaries | unit (daemon math) | `ctest -R phase4_gesture_math` | ❌ Wave 0 |
| GEST-04 | Preview sentence recomposes correctly for each {direction,mode,action,granularity} per UI-SPEC templates; NOTIFY fires | unit (GestureModel preview logic) | `ctest -R phase4_gesture_model` | ❌ Wave 0 |
| GEST-01/02 | Gesture config round-trips through `Configuration::save()` (no loss) | unit (extend phase3 round-trip or add phase4) | `ctest -R phase4_save_roundtrip` | ❌ Wave 0 (or extend `phase3_save_roundtrip`) |

### Sampling Rate
- **Per task commit:** `ctest --test-dir build -R phase4 --output-on-failure`
- **Per wave merge:** full suite (`ctest --test-dir build`)
- **Phase gate:** full suite green + **human UAT on the MX Master 4** for live gesture firing (GEST-02 feel-test, GEST-03 multi-desktop). Mirror Phase 3's split: logic unit-tested with fakes; live HID++ firing → human UAT persisted as a verification item.

### Wave 0 Gaps
- [ ] `tests/phase4/CMakeLists.txt` — register `phase4` CTest prefix; gate GUI tests on `TARGET logiops-gui-lib AND Qt6 Test found` (copy `tests/phase3/CMakeLists.txt` structure).
- [ ] `tests/phase4/GestureModelTest.cpp` — recording subclass of `GestureModel` capturing two-step order + args (no bus); preview-sentence assertions (GEST-01, GEST-04).
- [ ] `tests/phase4/gesture_math_test.cpp` — daemon-side unit linking `IntervalGesture.cpp`/`AxisGesture.cpp` (+ stubs) asserting fire-count-per-motion and remainder carry (GEST-02/03). This is the RED test that the bug-fix turns GREEN.
- [ ] (optional) extend `phase3_save_roundtrip` or add `phase4_save_roundtrip` covering a `gestures` map (GEST-01 persistence). The existing round-trip harness compiles `config/config.cpp` directly — reuse it.
- [ ] Root `CMakeLists.txt`: `add_subdirectory(tests/phase4)` next to the existing `tests/phase3`.

## Security Domain

> `security_enforcement` not set to false in config → included. The daemon runs as **root** and `SetGesture(direction, type)` plus the per-mode setters take **untrusted strings/ints over D-Bus** (the bus is now group-`logiops`-accessible after Phase 1, ACCESS-01). This is the threat surface CLAUDE.md and CONCERNS.md flag.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no (handled at D-Bus policy + polkit for Save, Phase 1) | inherited |
| V4 Access Control | yes (Save is polkit-gated; live-apply is group-gated) | inherited `ConfigState`/`.Config.Save` polkit path — no change |
| V5 Input Validation | **yes** | Validate `direction` ∈ {up,down,left,right,none} and `type`/`mode` ∈ {Axis,OnInterval,OnRelease,None} **before** any `std::stoi`/map lookup; clamp interval/threshold/multiplier to sane bounds |
| V6 Cryptography | no | n/a |

### Known Threat Patterns for {root daemon, untrusted D-Bus input}
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Bad `direction` string → `std::invalid_argument` from `toDirection` (uncaught → crash) | DoS | `setGesture` calls `toDirection` which throws `std::invalid_argument` (`GestureAction.cpp:44`); ensure ipcgull marshals this to a D-Bus error, not a daemon abort. Validate in GUI too (the `ButtonsModel::setChangeHost` validate-before-dispatch idiom). |
| Unknown `type` string → `InvalidGesture` (handled) | DoS | `makeGesture(type)` throws `InvalidGesture`; `setGesture` catches it, restores the prior gesture, and rethrows `std::invalid_argument` (`GestureAction.cpp:250-254`). Verify the rethrow surfaces as a D-Bus error cleanly. Add GUI-side allowlist so the daemon is never sent garbage. |
| `_config.gestures == nullopt` → `bad_optional_access` | DoS | Harden `setGesture` (Pitfall 3): lazily init the map, or guarantee `SetAction("Gesture")` initializes it before `SetGesture`. |
| Unbounded/negative interval/threshold/multiplier | DoS / weird input synthesis | Clamp `SetInterval`/`SetThreshold`/`SetMultiplier` to documented bounds; reject ≤0 (note: `setThreshold(0)` is the "reset" sentinel — keep that semantics but clamp negatives). |
| No untrusted string passed as `logPrintf` format | Info/Tampering | CLAUDE.md rule: gesture code already uses `logPrintf(WARN, "%s ...", x.c_str())` — keep untrusted strings as `%s` args, never as the format (CONCERNS #5). Note: `AxisGesture.cpp:49` `logPrintf(WARN, "Invalid axis %s.")` is missing its arg — harmless but fix while nearby. |

**No `std::stoi` on untrusted input in the gesture path** (unlike `ChangeHostAction`); the integers arrive typed over D-Bus. The string risk is `direction`/`type` → covered by allowlist + the existing throw/catch. Mirror Phase 1's CR-01/CR-02 hardening discipline.

## Sources

### Primary (HIGH confidence — in-tree source, read this session)
- `src/logid/actions/gesture/{Gesture,AxisGesture,IntervalGesture,ReleaseGesture,ThresholdGesture,NullGesture}.{h,cpp}` — gesture modes, interface names, param setters, the `OnRelease` collision, the interval accounting
- `src/logid/actions/GestureAction.{h,cpp}` — `SetGesture` live mode-switch (already exists), direction handling, `None` special-case
- `src/logid/config/schema.h` — gesture config structs + `GestureAction.gestures` map + mode names
- `src/logid/Configuration.h` — `defaults::gesture_threshold = 50` (no interval default)
- `src/logiops-gui/ButtonsModel.{h,cpp}` — the two-step async pattern to mirror; `GestureSupportRole` already present
- `src/logiops-gui/DeviceController.{h,cpp}` — Introspect capability gating, `_buttonsProxy`
- `src/logiops-gui/CMakeLists.txt`, `src/logiops-gui/dbus/logid_button.xml` — qdbusxml2cpp wiring, QML module
- `tests/phase3/CMakeLists.txt`, `tests/phase3/ButtonsModelTest.cpp` — the recording-subclass / no-bus test pattern + CTest registration
- `logid.example.cfg` — real gesture config blocks (mode "OnRelease")
- `.planning/phases/03-*/03-*-SUMMARY.md`, `STATE.md` — Qt 6.4.2 pin, qmlcache/`.rcc` lesson, two-step lesson

### Secondary / Tertiary
- None — no web sources used; this is a self-contained brownfield codebase and all findings are first-hand.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new deps; all components read in-tree.
- Architecture / mode-switch mechanism: HIGH — `SetGesture` read directly; it already does the live rebuild.
- GEST-02 bug diagnosis: HIGH on the *location/mechanism* (interval accounting + missing default + `OnRelease` collision + arg-name bug all confirmed in source); MEDIUM on the *exact default value* (needs on-hardware calibration — A1).
- Pitfalls: HIGH — collisions/bugs read directly from source.
- Security: HIGH — threat paths traced through the actual throw/catch in `GestureAction`/`Gesture`.

**Research date:** 2026-05-31
**Valid until:** stable (closed codebase) — re-verify only if the daemon's `actions/gesture/` or `GestureAction.cpp` change before planning.

## RESEARCH COMPLETE
