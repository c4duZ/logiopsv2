# Phase 3: Core Config UI & Persistence - Research

**Researched:** 2026-05-30
**Domain:** Qt 6.4.2 / QML desktop GUI driving the `logid` daemon's ipcgull D-Bus config surface
**Confidence:** HIGH (all claims verified against in-tree source this session)

## Summary

Phase 3 extends the Phase 2 Qt 6 + QML shell (`src/logiops-gui/`) into the full tabbed Options+-style config UI. The single most important finding is that **the daemon already exposes a writable D-Bus surface for almost every config node this phase touches** — DPI, SmartShift (active/threshold/torque), hi-res scroll (hires/invert/target), thumbwheel (divert/invert), button action-type assignment, and per-action parameter setters (keys, host, DPI change/cycle, profile) — and **every one of those setters writes back into the live `config::*` schema tree** that `Configuration::save()` re-serializes. So the "live-edit-over-D-Bus → Save()" model in CONTEXT.md is sound: round-trip fidelity holds for values/structure (comments and formatting are lost, which CONTEXT explicitly accepts). Profile create/switch/remove and **restore-to-defaults map directly to existing Device methods** (`SetProfile`/`RemoveProfile`/`ClearProfile`). [VERIFIED: src/logid/* read this session]

The critical architectural nuance the planner must internalize: **the config schema `group`/`map` structs are libconfig serialization helpers, NOT D-Bus objects.** Only `ipcgull::property<T>` members and the hand-written `IPC` interface classes inside each feature are on the bus. The GUI therefore drives config through **per-feature D-Bus interfaces** (`.DPI`, `.SmartShift`, `.HiresScroll`, `.ThumbWheel`, `.Buttons`/`.Button`, `.Action.<Type>`) at **per-feature/per-button object paths under each device node**, not by editing a config tree directly. [VERIFIED]

Two real gaps require small daemon additions: (1) **DPI cycle-list editing (DPI-02/DPI-03 labels)** has no first-class D-Bus setter for an arbitrary labeled preset list — the `.DPI` interface only sets a single per-sensor value; the cycle list lives in `config::CycleDPI` on a *button action* and there are no labels in the schema at all; and (2) **thumbwheel tap/left/right and hi-res up/down are gesture-typed**, so assigning a *simple* action to them goes through the gesture machinery (Phase 4 territory). Everything else is GUI-only client work. [VERIFIED]

**Primary recommendation:** Build Phase 3 as GUI-only client work against the existing ipcgull interfaces via `qt_add_dbus_interface`-generated typed proxies (one new XML per interface) plus a thin generic-`QDBusInterface` path for the dynamically-typed `.Action.<Type>` interface. Add exactly two small, well-scoped daemon C++ additions: a DPI-preset/label write surface (DPI-02/DPI-03) and "labels" storage in the schema. Use `ClearProfile` for restore-defaults and `Save()` for persistence — both already exist and are polkit-gated.

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Device Render & Button Mapping (UI-01, BTN-01, BTN-04)**
- Per-model vector/SVG render with clickable button hotspots; generic annotated mouse/keyboard fallback when no model artwork exists.
- Current bindings (BTN-04) shown two ways simultaneously: callout labels around the render AND a synced button→binding list view.
- Click a hotspot → opens a **non-modal** side/detail panel (render stays visible), not a blocking modal.
- Unsupported / non-remappable buttons shown but visually disabled, with an explanatory tooltip.

**Action Assignment (BTN-02, BTN-03, HOST-01)**
- Keystroke (BTN-02): live key-capture widget ("press keys now") + a manual modifier+key editor fallback.
- Built-in action picker (BTN-03): categorized list — Keystroke, DPI (change/cycle), Host switch, Profile switch, SmartShift toggle, Hi-res toggle, None/disabled.
- Rich gestures deferred to Phase 4. This phase exposes only the simple action types. "Disabled" → daemon `NullAction`.
- Host switch (HOST-01): numbered host slots (1/2/3, index-based ChangeHost) + next/prev cycle option.

**Pointer & Scroll (DPI-01..03, SCR-01..03)**
- DPI: sensitivity slider bounded by device-reported min/max (DPI-01) PLUS a separate DPI-cycle list editor with add/remove and per-preset labels (DPI-02/DPI-03).
- SmartShift (SCR-01): on/off toggle + threshold slider + torque slider; section capability-gated (only when device supports SmartShift).
- Hi-res scroll (SCR-02): toggle + invert-direction checkbox; capability-gated.
- Thumbwheel (SCR-03): capability-gated; divert/invert + left/right/tap action mapping, only on supported devices.

**Profiles, Persistence & Restore (PROF-01, CONF-01, CONF-02)**
- Apply model: every change applies live instantly (daemon is single source of truth); explicit **Save** persists to disk; UI shows an unsaved-changes indicator.
- Persistence path: GUI calls the daemon's existing polkit-gated `Save()` over D-Bus. GUI NEVER writes `/etc/logid.cfg` directly. Round-trip fidelity of `Configuration::save()` must be verified (this research) — comment/formatting loss acceptable, structure/value loss NOT.
- Profile scope: manual profiles only — create / name / switch (PROF-01). Per-app & sharing are Phase 5.
- Restore defaults (CONF-02): per-device "Restore defaults" action with a confirmation dialog; resets that device's config section.

### Claude's Discretion
- Exact QML component breakdown, tab structure, the live-edit binding mechanism (mutate over D-Bus vs. a GUI-side staging model before Save), model role names, key-capture implementation specifics, and the SVG hotspot data format — guided by the UI-SPEC and existing logiops feature/ipcgull/QML conventions.

### Deferred Ideas (OUT OF SCOPE)
- Fine-grained / repeat-control gestures → Phase 4.
- Per-application automatic profile switching + profile file sharing/export → Phase 5.
- Action wheel (radial menu) → Phase 6.
- Macros / smart multi-step actions → Phase 7.
- Keyboard backlight / RGB → Phase 8.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| BTN-01 | Click a button on a visual device rep and reassign its action | `.Buttons.Enumerate` → per-button nodes; `Button.Remappable`/`ControlID`/`TaskID`/`GestureSupport` properties drive hotspot enable + identity. Hotspot↔CID mapping is a GUI artwork concern. [VERIFIED] |
| BTN-02 | Assign a keystroke via key-capture | `Button.SetAction("Keypress")` then `Action.Keypress.SetKeys(as)` (array of evdev key-name strings). Daemon resolves names via `InputDevice::toKeyCode`. [VERIFIED] |
| BTN-03 | Assign built-in actions (DPI change/cycle, host, profile, SmartShift/hires toggle) | `Button.SetAction(type)` creates the action interface; per-type setter sets params: `ChangeDPI.SetChange`, `CycleDPI.SetDPIs`, `ChangeHost.SetHost`, `ChangeProfile.SetProfile`. ToggleSmartShift/ToggleHiresScroll/NullAction have no params. [VERIFIED] |
| BTN-04 | See at a glance what each button is bound to | Read current action interface name at the button's object path (introspect) + per-action `Get*` getters (`Keypress.GetKeys`, `ChangeHost.GetHost`, etc.). [VERIFIED] |
| DPI-01 | DPI slider bounded by device min/max | `.DPI.GetDPIs(sensor)` → `(dpis[], dpiStep, isRange)`; `.DPI.GetDPI(sensor)`; `.DPI.SetDPI(dpi, sensor)`. Bounds = min/max of `dpis` when `isRange`. [VERIFIED] |
| DPI-02 | Define a set of DPI levels to cycle through | **GAP**: cycle list only exists as a `CycleDPI` *button action* (`CycleDPI.SetDPIs`). No device-level "cycle preset list" surface. See Don't Hand-Roll + daemon addition note. [VERIFIED] |
| DPI-03 | Label DPI presets | **GAP**: schema has NO label field for DPI presets anywhere. Requires a daemon schema addition OR GUI-only label store. [VERIFIED] |
| SCR-01 | Toggle/tune SmartShift (on/off, threshold, torque) | `.SmartShift.GetConfig` → `(active, threshold, torque)`; `SetActive(b,clear)`, `SetThreshold(y,clear)`, `SetTorque(y,clear)`. `TorqueSupport` (bool prop) gates torque slider. [VERIFIED] |
| SCR-02 | Toggle hi-res + invert | `.HiresScroll.GetConfig` → `(hires, invert, target)`; `SetHires(b)`, `SetInvert(b)`, `SetTarget(b)`. [VERIFIED] |
| SCR-03 | Thumbwheel divert/invert + left/right/tap actions | `.ThumbWheel.GetConfig` → `(divert, invert)`; `SetDivert(b)`, `SetInvert(b)`. **left/right/tap are gesture-typed** (`SetLeft/SetRight/SetTap` take a *type* string and build a gesture) — simple-action mapping is partial; see gap note. [VERIFIED] |
| HOST-01 | Easy-Switch (change-host) action on a button | `ChangeHost.SetHost(s)` — accepts "1"/"2"/"3" (1-based, daemon subtracts 1) or "next"/"prev". `ChangeHost.GetHost()` reads back. Host count requires a daemon read (see capability gap). [VERIFIED] |
| PROF-01 | Create/name/switch manual profiles | `Device.GetProfiles()` → `as`; `Device.SetProfile(name)` (creates if absent + switches); `Device.RemoveProfile(name)`. `ActiveProfile`/`DefaultProfile` properties. [VERIFIED] |
| CONF-01 | Apply live + persist without editing text | Live = the per-feature setters (already push to hardware). Persist = `Config.Save()` (polkit-gated). [VERIFIED] |
| CONF-02 | Restore device to defaults | `Device.ClearProfile(name)` resets the named profile's `config::Profile` to default and reconfigures hardware. [VERIFIED] |
| UI-01 | Faithful Options+ tabbed, polished, animated UI | Qt Quick Controls 2 `TabBar`/`StackLayout` on the existing `DetailPane.qml`; tokens in `Theme.qml`; motion budget per UI-SPEC. [VERIFIED] |
</phase_requirements>

---

## D-Bus Write-Surface Audit (BLOCKING question 1)

**The mechanism:** Each high-level feature (`features/DPI`, `SmartShift`, `HiresScroll`, `RemapButton`, `ThumbWheel`) owns a nested `class IPC : public ipcgull::interface` registered via `device->ipcNode()->make_interface<IPC>(this)`. The IPC setters hold a `std::reference_wrapper` into the active profile's `config::*` struct and **write the new value into that struct AND push it to hardware**. Because `Configuration` *is* the `config::Config` tree and `save()` re-serializes `*this`, a live D-Bus edit is automatically part of what `Save()` writes. [VERIFIED: src/logid/features/*.cpp]

Config schema `group`/`signed_group`/`map` (`config/group.h`, `map.h`) are **libconfig (de)serialization helpers only** — they are not `ipcgull::object`/`interface` and are not individually on the bus. The only schema members directly on the bus are `ipcgull::property<T>` (e.g. `config::Device::default_profile`, exposed as the `.Device.DefaultProfile` property). [VERIFIED: config/schema.h, types.h]

### Per-node write surface table

| Config node (UI edits) | D-Bus interface @ path | Read | Write | Daemon addition needed? |
|---|---|---|---|---|
| DPI single value (DPI-01) | `.DPI` @ `…/devices/N` | `GetDPIs(sensor)`→`(aq,q,b)`, `GetDPI(sensor)`, `GetSensors` | `SetDPI(dpi,sensor)` | No |
| DPI cycle preset **list** (DPI-02) | `.Action.CycleDPI` @ a *button* node | `GetDPIs()`→`ai` | `SetDPIs(ai)` | **Yes** — list is per-button-action only; no device-level cycle store. See note. |
| DPI preset **labels** (DPI-03) | — | — | — | **Yes** — no label field exists in schema at all. |
| SmartShift on (SCR-01) | `.SmartShift` @ `…/devices/N` | `GetConfig`→`(yyy)` | `SetActive(b,clear)` | No |
| SmartShift threshold | `.SmartShift` | `GetConfig` | `SetThreshold(y,clear)` | No |
| SmartShift torque | `.SmartShift` (`TorqueSupport` prop gates) | `GetConfig` | `SetTorque(y,clear)` | No |
| Hi-res toggle (SCR-02) | `.HiresScroll` @ `…/devices/N` | `GetConfig`→`(bbb)` | `SetHires(b)` | No |
| Hi-res invert (SCR-02) | `.HiresScroll` | `GetConfig` | `SetInvert(b)` | No |
| Hi-res up/down (scroll gestures) | `.HiresScroll` | — | `SetUp(type)`, `SetDown(type)` (gesture) | No (but gesture = Phase 4) |
| Thumbwheel divert (SCR-03) | `.ThumbWheel` @ `…/devices/N` | `GetConfig`→`(bb)` | `SetDivert(b)` | No |
| Thumbwheel invert (SCR-03) | `.ThumbWheel` | `GetConfig` | `SetInvert(b)` | No |
| Thumbwheel left/right/tap (SCR-03) | `.ThumbWheel` | — | `SetLeft/SetRight/SetTap(type)` — **gesture-typed** | Partial — simple action via gesture; see gap |
| Button action **type** (BTN-01/03) | `.Button` @ `…/devices/N/buttons/M` | `ControlID`,`TaskID`,`Remappable`,`GestureSupport` props | `SetAction(type)` | No |
| Button → Keypress keys (BTN-02) | `.Action.Keypress` @ button node | `GetKeys`→`as` | `SetKeys(as)` | No |
| Button → ChangeDPI (BTN-03) | `.Action.ChangeDPI` @ button node | `GetConfig`→`(nq)` | `SetChange(n)`, `SetSensor(y,reset)` | No |
| Button → CycleDPI (BTN-03) | `.Action.CycleDPI` @ button node | `GetDPIs`→`ai` | `SetDPIs(ai)` | No |
| Button → ChangeHost (HOST-01) | `.Action.ChangeHost` @ button node | `GetHost`→`s` | `SetHost(s)` | No |
| Button → ChangeProfile (BTN-03) | `.Action.ChangeProfile` @ button node | `GetProfile`→`s` | `SetProfile(s)` | No |
| Button → ToggleSmartShift / ToggleHiresScroll / None | `.Button.SetAction(type)` only | — | (no params) | No |
| Manual profile create/name/switch (PROF-01) | `.Device` @ `…/devices/N` | `GetProfiles`→`as`, `ActiveProfile`, `DefaultProfile` props | `SetProfile(s)`, `RemoveProfile(s)` | No |
| Restore-to-defaults (CONF-02) | `.Device` | — | `ClearProfile(s)` | No |
| Persist to disk (CONF-01) | `.Config.Save` @ `/pizza/pixl/logiops` | — | `Save()` (polkit-gated) | No |

**Object-path geometry (CRITICAL for the GUI):**
- Device: `/pizza/pixl/logiops/devices/N` — hosts `.Device`, `.DPI`, `.SmartShift`, `.HiresScroll`, `.ThumbWheel`, `.Buttons` interfaces (all on the *same* device node). [VERIFIED: features attach via `device->ipcNode()->make_interface`]
- Buttons: `.Buttons.Enumerate()` returns object paths `…/devices/N/buttons/M` (M = enumeration index, NOT the CID). Each hosts a `.Button` interface. [VERIFIED: RemapButton.cpp — `_ipc_node = make_child("buttons")`, `Button` node = `make_child(std::to_string(index))`]
- Action: the **current** action interface (`.Action.Keypress`, `.Action.ChangeHost`, …) is added/dropped on the *button's own node* when `SetAction(type)` runs. The interface name is `pizza.pixl.LogiOps.Action.<Name>` (e.g. `…Action.Keypress`). [VERIFIED: Action.cpp ctor `SERVICE_ROOT_NAME ".Action." + name`]
- HiresScroll also makes child nodes `…/hires_scroll/up` and `…/hires_scroll/down`; ThumbWheel makes `…/thumb_wheel/left|right|tap|...` for the gesture/action interfaces. [VERIFIED]

### Daemon additions required (scope these as small ipcgull-pattern tasks)

1. **DPI cycle list + labels (DPI-02 / DPI-03)** — There is no device-level "cycle preset list with labels" in the schema. Options for the planner:
   - (a) Treat the DPI-cycle editor as editing a `CycleDPI` *button action* (the daemon's actual model) — but that conflicts with the UI-SPEC's standalone "Pointer tab cycle editor."
   - (b) Add a small schema field + `.DPI` interface methods to store a labeled preset list at device/profile scope, following the exact `IPC` pattern in `DPI.cpp` (writeback to `config::*` so `Save()` persists it). Labels have **no current schema home** → a `labels` field must be added to whatever struct holds the list (e.g. extend `config::CycleDPI` with an optional `std::list<std::string> labels`, or add a profile-level preset map).
   - **Recommendation:** confirm with the user (this is `[ASSUMED]` design territory). Minimal-surface option: store labels GUI-side keyed by device+dpi value (no daemon change), and drive the cycle list through `CycleDPI.SetDPIs`. Flag as an Assumption.

2. **Thumbwheel left/right/tap simple actions (SCR-03)** — `SetLeft/SetRight/SetTap` build *gestures* (`makeGesture`), and tap is a `BasicAction` in the schema but the IPC setter routes through gesture machinery. A clean "assign a simple action to thumbwheel tap" may need a small IPC method that sets the `tap` `BasicAction` directly (mirroring `Button.SetAction`). Verify against `ThumbWheel.cpp:setTap` during planning; if it already builds a `BasicAction` interface at `…/thumb_wheel/tap`, no change needed.

3. **Host count for HOST-01 capability/UI** — `ChangeHost.GetHost` reads the configured value, but the **number of paired hosts** comes from `hidpp20::ChangeHost::getHostInfo().hostCount`, which is **not exposed over D-Bus**. To render "1/2/3" slots accurately, add a tiny read (e.g. a `HostCount` property on a ChangeHost-capable surface), or default to 3 and tolerate. Flag as a small addition or an accepted approximation.

---

## Configuration::save() round-trip fidelity (BLOCKING question 2)

**Verdict: round-trip is SAFE for structure and values. Comments/formatting are lost (CONTEXT accepts this). No whole-device-block loss risk.** [VERIFIED: Configuration.cpp, types.h]

Mechanism, step by step:
1. On load, `Configuration::Configuration(file)` reads the libconfig file into `_config` (a `libconfig::Config`) and does `Config::operator=(get<Config>(_config.getRoot()))` — the **entire** tree is parsed into the in-memory `config::Config`. Unknown/extra settings that don't map to the schema are dropped at this point (e.g. a hypothetical future key the schema doesn't know). [VERIFIED]
2. Live D-Bus edits mutate the in-memory `config::*` structs (via the feature IPC setters, which hold references into the active profile). [VERIFIED]
3. `save()` calls `config::set(_config.getRoot(), *this)` which walks the schema and writes **every** field back into `_config`, then `_config.writeFile()`. [VERIFIED: Configuration.cpp:125-127]

Fidelity analysis:
- **No data loss of schema-known structure**: `config_io<group>::set` and `config_io<map>::set` iterate the full in-memory tree. The device map is keyed by device name; *all* devices present in memory are written. A device the daemon never instantiated still survives because it was loaded into `config.devices` at startup (the map round-trips). [VERIFIED: types.h map/group set]
- **Maps are rebuilt destructively but completely**: `config_io<map>::set` does `while (getLength()) remove(0)` then re-adds every entry — so order may change and comments inside list elements are lost, but no entries vanish. [VERIFIED: types.h:363-372]
- **Comments / blank lines / formatting: LOST.** libconfig's `writeFile` emits a canonical representation; original comments are not preserved. CONTEXT explicitly accepts this. [VERIFIED — libconfig behavior; CITED: libconfig docs, standard behavior]
- **`hiresscroll` shorthand normalization**: a config written as `hiresscroll: true` (bool) loads into the `std::variant<bool, HiresScroll>` and may be re-emitted as the bool or the group form depending on edits. Not data loss, but the on-disk shape can change. [VERIFIED: schema.h Profile::hiresscroll]
- **Legacy single-profile device blocks get upgraded**: `Device::_getConfig` converts an old `Profile`-shaped device entry into a `{ default_profile; profiles{} }` shape in memory; `save()` then writes the new shape. This is an intentional, lossless structural upgrade but the file *shape* changes on first save. [VERIFIED: Device.cpp:374-400]

**Risk to flag in PLAN:** any setting the user hand-added that the schema does NOT model is silently dropped on the first GUI `Save()` (because it was never loaded into the in-memory tree). For the keys this phase edits, the schema is complete, so this is a low but real risk for power-user configs. Recommend a one-line warning in Save copy or docs: "Saving rewrites the config file; hand-added comments and unsupported keys are not preserved." Validate empirically in a Wave-0 round-trip test (load a known-good `logid.example.cfg`, mutate one value over D-Bus, `Save()`, diff structure). [VERIFIED reasoning]

---

## Button remap representation (question 3)

Schema: `config::Button { std::optional<Action> action; }` where `Action` is a `std::variant<NoAction, KeypressAction, ToggleSmartShift, ToggleHiresScroll, CycleDPI, ChangeDPI, ChangeHost, ChangeProfile, GestureAction>`. `RemapButton` is `map<uint16_t /*cid*/, Button, "cid">`. [VERIFIED: schema.h]

**How the GUI sets a button action (two-step, by design):**
1. Call `pizza.pixl.LogiOps.Button.SetAction(type)` at the button's object path, where `type` ∈ {`"Keypress"`, `"ChangeDPI"`, `"CycleDPI"`, `"ChangeHost"`, `"ChangeProfile"`, `"ToggleSmartShift"`, `"ToggleHiresScroll"`, `"Default"` (→ NullAction/clear), `"Gestures"` (Phase 4)}. The strings are each action's `interface_name`. [VERIFIED: Action.cpp `_makeAction` switch]
2. `SetAction` **drops the old action interface and creates the new one on the same button node**, then calls `Button::configure()` which re-runs `setControlReporting` (re-divert) on the hardware — so **reassignment auto-reconfigures; the GUI does not need a separate reconfigure call.** [VERIFIED: RemapButton.cpp:287-303]
3. For parametric actions, the GUI then calls the type-specific setter on the **newly-present** interface at the same path: `Action.Keypress.SetKeys(as)`, `Action.ChangeHost.SetHost(s)`, `Action.ChangeDPI.SetChange(n)`, `Action.CycleDPI.SetDPIs(ai)`, `Action.ChangeProfile.SetProfile(s)`. [VERIFIED]

**ipcgull marshalling of the variant:** the variant itself is **never** marshalled over D-Bus. The active variant alternative is realized as a concrete `ipcgull::interface` subclass (the Action). The GUI discovers the current type by **introspecting which `…Action.<X>` interface exists at the button path** (`BTN-04` "what is it bound to"). NullAction = no action interface present / `Default`. [VERIFIED: Action extends ipcgull::interface; makeAction creates one interface per type]

**Disable ("None"):** `Button.SetAction("Default")` → `_makeAction` does `config.reset(); return nullptr` → no action interface, `config::Button.action` becomes empty. The UI row shows "Disabled". [VERIFIED: Action.cpp:78-80]

**Guardrails the daemon enforces (GUI must mirror/handle the thrown errors):**
- `SetAction` throws `std::invalid_argument("Non-remappable")` if `!(_info.flags & TemporaryDivertable)` → use the `Remappable` property to disable the hotspot up-front (BTN-01 "show disabled + tooltip"). [VERIFIED]
- `SetAction("Gestures")` throws `"No gesture support"` if `!(additionalFlags & RawXY)` → mirror via `GestureSupport` property (gestures are Phase 4 anyway). [VERIFIED]

---

## Capability discovery from the GUI (question 4)

The GUI mirrors the daemon's present-or-absent model: a feature interface **only exists** on a device node if the hardware advertised it (`_addFeature<T>` swallows `UnsupportedFeature`). So capability-gating = **does the interface/property exist at the device path?** [VERIFIED: Device.cpp `_addFeature`]

| UI capability gate | D-Bus read source | Verified |
|---|---|---|
| SmartShift section shown | `.SmartShift` interface present at device node | Interface exists ⇔ HID++ 0x2110/0x2111 supported |
| SmartShift torque slider shown | `.SmartShift.TorqueSupport` (bool property) | YES — property in IPC ctor |
| Hi-res scroll section shown | `.HiresScroll` interface present | YES |
| Thumbwheel section shown | `.ThumbWheel` interface present | YES |
| DPI tab / slider shown + bounds | `.DPI` present; `GetDPIs(sensor)`→`(dpis, step, isRange)`; bounds = min/max(dpis) | YES |
| DPI sensor count | `.DPI.GetSensors()` | YES |
| Button remappable (per hotspot) | `.Button.Remappable` (bool prop) per button | YES |
| Button gesture-capable | `.Button.GestureSupport` (bool prop) | YES (Phase 4 relevance) |
| Host count (HOST-01 slots) | **NOT exposed** — `getHostInfo().hostCount` is internal | NO — gap, see daemon additions |
| Device kind (mouse/keyboard) | `.Device.ProductID` (q) + existing `DeviceModel::kindFromPid` heuristic | YES (Phase 2) |

**Gating discovery mechanism options:** (a) `org.freedesktop.DBus.Introspectable.Introspect()` on the device path and check for the interface name (works, but parse XML), or (b) attempt a `Get*` call and treat `UnknownInterface`/`UnknownMethod` D-Bus errors as "absent." Recommendation: introspect once per device on selection, cache the present-interface set, and gate the QML tabs/sections from a C++ capability model (keeps "QML renders only"). [CITED: dbus spec — Introspectable]

---

## Qt/QML patterns (question 5)

### Existing shell structure to extend
- `DetailPane.qml` is today a read-only `Rectangle` with a hidden `Repeater` over `deviceModel` to read the selected row's roles, a header (icon/name/model), and Connection/Battery sections. Phase 3 replaces its body with a **header + `TabBar` + `StackLayout`** (Buttons/Pointer/Scroll/Profiles). [VERIFIED: DetailPane.qml]
- `DaemonConnection` (C++) owns the system-bus connection, the `.Devices` proxy, a `QMap<path, .Device proxy>`, and the `QDBusServiceWatcher` reconnect. **It is the natural home for the new per-feature proxies** and a per-device capability model. [VERIFIED: DaemonConnection.h]
- `DeviceModel` (QAbstractListModel) carries device-list roles only. Per-device *config* state (DPI value, smartshift, buttons) should NOT be crammed into list roles — introduce dedicated C++ objects (see below). [VERIFIED]
- Proxies are generated by `qt_add_dbus_interface(VAR xml basename)` from trimmed introspection XML in `dbus/` (qdbusxml2cpp at `/usr/lib/qt6/bin/qdbusxml2cpp`). Phase 3 adds one XML per new interface. [VERIFIED: CMakeLists.txt; qdbusxml2cpp present]

### Recommended C++↔QML bridge pattern for config
The Phase 2 rule is "C++ model classes bridge daemon state; QML renders only, zero business logic in JS." Apply it to config by introducing **per-feature QObject controllers** exposed as context properties or as roles/children of a `DeviceController`:

- `DeviceController` (QObject) created per selected device: wraps the device path, lazily builds the typed feature proxies, exposes a `capabilities` gadget (hasSmartShift/hasThumbwheel/hasHires/dpiMin/dpiMax/sensorCount/hostCount) as `Q_PROPERTY`, and `Q_PROPERTY` notifyable values (dpi, smartShiftOn/threshold/torque, hiresOn/invert, thumbDivert/invert) bound from the `Get*` reads. Setters are `Q_INVOKABLE` and call the async D-Bus setters; on success they update the property + emit changed.
- `ButtonsModel` (QAbstractListModel) over `.Buttons.Enumerate()`: roles = controlId, taskId, remappable, gestureSupport, currentActionType, currentActionSummary. A `Q_INVOKABLE setAction(row, type)` + parametric setters.
- An **UnsavedChanges** tracker (C++): set a dirty flag whenever any setter is invoked; clear on successful `Save()`. Drives the warning pill + Save button enable (UI-SPEC).

This keeps all D-Bus marshalling, async handling, and dirty-tracking in C++ and leaves QML declarative.

### Concrete QML/Controls 2 patterns (Qt 6.4.2)
- **Tabbed config:** `TabBar { TabButton … }` + `StackLayout { currentIndex: tabBar.currentIndex }`. Style the indicator (2px accent underline that slides) via a custom `contentItem`/`background` on `TabButton` and an animated `Rectangle` over the bar; cross-fade tab content with `Behavior on opacity`. All within the existing `Theme.*` tokens. [CITED: Qt Quick Controls 2 TabBar docs]
- **Sliders bound to live D-Bus:** `Slider { from: ctrl.dpiMin; to: ctrl.dpiMax; stepSize: ctrl.dpiStep; value: ctrl.dpi; onMoved: ctrl.setDpi(value) }`. Bind `value` to the property and call the setter on `onMoved`/`onPressedChanged` (not on every pixel) to avoid D-Bus call storms. Use `live: false` or debounce in the controller. [CITED: Qt Slider docs]
- **Key-capture widget (BTN-02):** an `Item` with `focus: true` and `Keys.onPressed`/`Keys.onReleased`; accumulate `event.key` + modifiers, map to evdev `KEY_*` names (the daemon's `SetKeys` takes evdev key-name strings like `"KEY_A"`, `"KEY_LEFTCTRL"`). `Esc` cancels. Qt key codes must be translated to evdev names — build the mapping in C++ (a `KeyNameMapper` helper), since `SetKeys` expects `InputDevice::toKeyName`-style strings (e.g. `KEY_VOLUMEUP`). Provide a manual modifier-checkbox + key-combo fallback. [VERIFIED: KeypressAction.cpp uses `InputDevice::toKeyName`/`toKeyCode`; evdev `KEY_*` strings]
- **Unsaved/Save flow:** Save button calls `Config.Save()` async; while pending show `BusyIndicator` and disable; on `QDBusError` map AccessDenied/auth-declined vs ServiceUnknown/daemon-down to the two UI-SPEC error strings; on success emit "Saved" transient + clear dirty. The polkit prompt is driven by the **daemon's** `polkit_authority_check_authorization_sync` (blocking on the daemon dispatch thread) — the GUI just sees a delayed reply or an error; keep the call async so the GUI never blocks. [VERIFIED: Configuration.cpp polkit; DaemonConnection async pattern]
- **Restore-defaults dialog:** Qt Quick Controls 2 `Dialog` (modal) with destructive-styled confirm → `Device.ClearProfile(activeProfile)`. [VERIFIED]
- **Numeric readouts:** reuse the Phase 2 `TextMetrics` fixed-width trick (font.features tnum is Qt 6.7+, target is 6.4.2). [VERIFIED: Theme.qml/DetailPane comments]

### Anti-patterns to avoid
- **Do not** put D-Bus calls or dirty-tracking logic in QML/JS (violates the Phase 2 "QML renders only" decision).
- **Do not** poll for current values — read once on selection and rely on the daemon's reconfigure + your optimistic property update; there are no per-config "changed" signals from the daemon (only `StatusChanged`/`BatteryChanged`), so the GUI is the source of optimistic UI state between Save cycles. [VERIFIED: only those two signals exist on `.Device`]
- **Do not** assume the button enumeration index equals the CID — use the `ControlID` property to map a hotspot↔button. [VERIFIED]

---

## Standard Stack

### Core (all already present; versions verified on this machine)
| Library | Version | Purpose | Why standard |
|---|---|---|---|
| Qt6 Core/DBus/Gui | 6.4.2 | C++ bridge, typed D-Bus proxies, clipboard | Phase 2 baseline; `qmake6 --version` → Qt 6.4.2 [VERIFIED] |
| Qt6 Quick/Qml/Quick Controls 2 | 6.4.2 | QML UI, TabBar/Slider/Dialog/Switch | Phase 2 baseline |
| Qt6 Svg | 6.4.2 | Render per-model device SVG + glyphs | Already linked (`Qt6::Svg`) [VERIFIED: CMakeLists] |
| qdbusxml2cpp | (Qt 6.4.2) | Generate typed proxies from introspection XML | `/usr/lib/qt6/bin/qdbusxml2cpp` present [VERIFIED] |
| ipcgull (daemon side) | vendored | Existing D-Bus interface host | In-tree; any daemon addition follows its pattern [VERIFIED] |

**Installation:** none new — Phase 2 already pulls Qt6 Core/DBus/Gui/Quick/Qml/Svg + Quick Controls 2 runtime (`qml6-module-qtquick-controls2` on Debian). Confirm `qml6-module-qtquick-controls` / `-templates` / `-layouts` are installed at runtime (operator prerequisite, mirror Phase 2 note). [VERIFIED: CMakeLists runtime note]

### Alternatives considered
| Instead of | Could use | Tradeoff |
|---|---|---|
| Typed `qt_add_dbus_interface` proxies | Generic `QDBusInterface` for everything | Generic is needed ONLY for the dynamically-typed `.Action.<Type>` interface (its presence varies); use typed proxies for stable interfaces (`.DPI`, `.SmartShift`, …) and generic for action params. |
| Per-feature QObject controllers | Stuff config into `DeviceModel` roles | Roles bloat + no per-feature signals; controllers are cleaner and match "QML renders only." |

---

## Architecture Patterns

### Recommended structure (additions under src/logiops-gui/)
```
src/logiops-gui/
├── dbus/                       # + one trimmed XML per new interface
│   ├── logid_config.xml        # .Config (Save)
│   ├── logid_dpi.xml           # .DPI
│   ├── logid_smartshift.xml    # .SmartShift
│   ├── logid_hiresscroll.xml   # .HiresScroll
│   ├── logid_thumbwheel.xml    # .ThumbWheel
│   ├── logid_buttons.xml       # .Buttons (Enumerate)
│   └── logid_button.xml        # .Button (+ generic QDBusInterface for .Action.*)
├── DeviceController.{h,cpp}    # per-device capability + config controller (QObject)
├── ButtonsModel.{h,cpp}        # QAbstractListModel over .Buttons
├── KeyNameMapper.{h,cpp}       # Qt key/modifier → evdev KEY_* strings
├── ConfigState.{h,cpp}         # dirty tracking + Save() driver
├── icons/                      # + UI-SPEC glyphs (keystroke, dpi, host, …)
└── qml/
    ├── DetailPane.qml          # → header + TabBar + StackLayout
    └── config/                 # ButtonsTab, PointerTab, ScrollTab, ProfilesTab,
                                #   ReassignPanel, KeyCaptureField, DpiCycleEditor,
                                #   RestoreDialog, SaveToolbar
```

### Device extension assessment
`logid::Device` hardcodes HID++ 2.0 (`Device.h:56` TODO). This does NOT block Phase 3 — every device the GUI configures is already a HID++ 2.0 `logid::Device` with the feature interfaces. The hardcoding only limits HID++ 1.0-only devices, which were never instantiated as configurable devices anyway. [VERIFIED]

---

## Don't Hand-Roll

| Problem | Don't build | Use instead | Why |
|---|---|---|---|
| Persisting config to disk | Writing `/etc/logid.cfg` from the GUI | `Config.Save()` D-Bus (polkit-gated) | CONTEXT-locked; root-only file; already gated. [VERIFIED] |
| Restore-to-defaults | Manually clearing each setting over D-Bus | `Device.ClearProfile(name)` | Atomic reset + hardware reconfigure in one daemon call. [VERIFIED] |
| Profile create | Add-then-populate dance | `Device.SetProfile(name)` (creates-if-absent + switches) | Single call. [VERIFIED] |
| Closest-DPI snapping / range math | GUI-side rounding to valid DPI steps | `DPI.SetDPI` (daemon snaps via `getClosestDPI`) | Daemon already snaps to `dpiStep`/range. [VERIFIED: DPI.cpp] |
| evdev keycode names | Hard-coded Qt→string map guessed | Reuse evdev `KEY_*` names; map in a tested `KeyNameMapper` | `SetKeys` expects exact `InputDevice::toKeyName` strings. [VERIFIED] |
| Capability detection | Maintaining a per-PID feature table | Introspect which feature interfaces exist | Mirrors daemon's `_addFeature` present-or-absent model exactly. [VERIFIED] |

**Key insight:** the daemon is an opinionated, hardware-aware single source of truth. The GUI should be a thin optimistic client over its existing interfaces; nearly all "hard" logic (snapping, reconfigure, persistence auth) already lives daemon-side.

---

## Common Pitfalls

### Pitfall 1: Treating the config schema as the D-Bus surface
**What goes wrong:** Assuming you can edit `config::SmartShift.threshold` directly over D-Bus because it's in `schema.h`. **Why:** `group`/`map` are libconfig helpers, not bus objects. **Avoid:** drive everything through the feature `IPC` interfaces. **Sign:** looking for a `Set` on a `group` member — it doesn't exist. [VERIFIED]

### Pitfall 2: Two-step action assignment race
**What goes wrong:** Calling `Action.Keypress.SetKeys` before `Button.SetAction("Keypress")` has created the interface → `UnknownMethod`. **Avoid:** sequence the calls (await SetAction reply, then setter); the action interface only appears after SetAction. **Sign:** intermittent UnknownMethod on the action path. [VERIFIED: SetAction creates the interface]

### Pitfall 3: Button index ≠ CID
**What goes wrong:** Mapping SVG hotspots by enumeration index. **Avoid:** read each button's `ControlID` property; map hotspots to CIDs. [VERIFIED]

### Pitfall 4: Save() blocks the daemon dispatch thread during polkit prompt
**What goes wrong:** The daemon holds `server_lock` for the full polkit prompt (documented in Configuration.cpp); concurrent device hotplug stalls until the user responds. The GUI must NOT also block. **Avoid:** call `Save()` async (QDBusPendingCallWatcher), show BusyIndicator. **Sign:** UI freeze during Save. [VERIFIED: Configuration.cpp WR-01 note]

### Pitfall 5: First Save() rewrites the file shape
**What goes wrong:** Comments and legacy single-profile device shorthand are rewritten/lost on first GUI Save. **Avoid:** accept (per CONTEXT) but warn the user; verify in a Wave-0 round-trip test that no *values* are lost. **Sign:** user's hand-tuned comments disappear. [VERIFIED]

### Pitfall 6: Qt 6.4.2 has no `colorScheme` / `font.features` tnum
**What goes wrong:** Using 6.5+/6.7+ APIs. **Avoid:** dark detection via `SystemPalette` lightness (existing Theme pattern); tabular numerals via `TextMetrics` fixed width. [VERIFIED: Theme.qml]

### Pitfall 7: qdbusxml2cpp breaks on `--` and `<` in XML comments
**What goes wrong:** Phase 2 hit this — comment chars silently produced empty proxies. **Avoid:** keep new introspection XML comments clean. [VERIFIED: STATE.md Phase 2 note]

---

## Code Examples

### Bound DPI slider → live D-Bus (QML + controller)
```qml
// Source: pattern from Qt Quick Controls 2 Slider + existing DaemonConnection async model
Slider {
    from: ctrl.dpiMin; to: ctrl.dpiMax; stepSize: ctrl.dpiStep
    value: ctrl.dpi
    onMoved: ctrl.setDpi(value)   // controller calls .DPI.SetDPI async; daemon snaps
}
Text { text: ctrl.dpi + qsTr(" DPI") }  // TextMetrics-fixed width
```

### Two-step button reassign (controller, async)
```cpp
// Source: VERIFIED against RemapButton.cpp SetAction + Action.cpp setters
void ButtonsModel::setAction(int row, const QString& type) {
    auto* btn = proxyFor(row);                // .Button @ …/buttons/M
    auto reply = btn->asyncCall("SetAction", type);
    auto* w = new QDBusPendingCallWatcher(reply, this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [=]{
        // now the .Action.<type> interface exists at the SAME path:
        QDBusInterface action(service, path, "pizza.pixl.LogiOps.Action." + type, bus);
        // e.g. action.asyncCall("SetKeys", QStringList{"KEY_LEFTCTRL","KEY_C"});
        markDirty();
    });
}
```

### Save with polkit-aware error mapping
```cpp
// Source: VERIFIED against Configuration.cpp (throws on unauthorized) + DaemonConnection
auto reply = configProxy->asyncCall("Save");
// on watcher finish: if reply.isError():
//   QDBusError::AccessDenied / message contains "Not authorized" -> auth-declined copy
//   QDBusError::ServiceUnknown / NoReply -> daemon-down copy
// else: emit saved(); clearDirty();
```

---

## State of the Art

| Old approach | Current approach | When | Impact |
|---|---|---|---|
| Hand-edit `/etc/logid.cfg` | GUI over polkit-gated `Save()` | This project | The entire reason for Phase 3 |
| Raw `QDBusInterface` everywhere | Typed proxies via `qt_add_dbus_interface` + generic only for dynamic `.Action.*` | Phase 2 established | Type safety + one escape hatch |

**Deprecated/outdated:** none specific to Phase 3. Qt 6.4.2 is the pinned target — avoid 6.5+ APIs (`colorScheme`, `font.features` tnum, `loadFromModule`). [VERIFIED]

---

## Validation Architecture

### Test Framework
| Property | Value |
|---|---|
| Framework | CTest + QTest (unit) + graceful-skip shell smoke scripts (Phase 1/2 pattern) |
| Config file | `CMakeLists.txt` `add_test`/`enable_testing` (no pytest/jest — this is C++/QML) |
| Quick run command | `ctest --test-dir build -R '<phase3-pattern>' --output-on-failure` |
| Full suite command | `ctest --test-dir build --output-on-failure` |

GUI logic is testable without a live bus by driving the controller/model classes directly (the Phase 2 `DeviceModelTest` precedent: model exercised via public slots, no bus). QML rendering and on-hardware behavior are manual-on-hardware.

### Phase Requirements → Test Map
| Req | Behavior | Test type | Automated command | File exists? |
|---|---|---|---|---|
| CONF-01 | save() round-trip preserves structure/values | integration (daemon) | round-trip test: load example.cfg → mutate via config_io → save → re-parse → assert values | ❌ Wave 0 |
| CONF-02 | ClearProfile resets a profile | unit (daemon) | exercise `Device::clearProfile` resets `config::Profile` | ❌ Wave 0 (or manual-on-hardware) |
| BTN-02 | Qt key event → evdev KEY_* string | unit (GUI) | `KeyNameMapperTest` maps known keys/modifiers | ❌ Wave 0 |
| BTN-01/03/04 | ButtonsModel roles + setAction sequencing | unit (GUI, mocked proxy) | `ButtonsModelTest` (no live bus) | ❌ Wave 0 |
| DPI-01 | controller exposes min/max/step from GetDPIs | unit (GUI, mocked) | `DeviceControllerTest` bounds | ❌ Wave 0 |
| DPI-02/03 | cycle list + labels store/round-trip | unit | depends on chosen design (daemon field vs GUI store) | ❌ Wave 0 |
| SCR-01/02/03 | capability gating reflects present interfaces | unit (GUI, mocked introspection) | `CapabilityModelTest` | ❌ Wave 0 |
| PROF-01 | GetProfiles/SetProfile/RemoveProfile wiring | unit (mocked) + manual | `ProfilesModelTest` | ❌ Wave 0 |
| HOST-01 | SetHost("1"/"next") formatting | unit | controller formats host arg correctly | ❌ Wave 0 |
| UI-01 | tabbed layout, motion, capability-gated sections | manual-on-hardware | visual checklist vs UI-SPEC | manual |
| CONF-01 (live) | edit applies to hardware instantly | manual-on-hardware | observe device behavior change | manual |
| Save polkit flow | prompt + denial + success copy | manual-on-hardware | trigger Save as group user | manual |

### Sampling Rate
- **Per task commit:** `ctest --test-dir build -R '<changed-area>' --output-on-failure`
- **Per wave merge:** full `ctest` suite green
- **Phase gate:** full suite green + on-hardware visual/behavior checklist before `/gsd-verify-work`

### Wave 0 Gaps
- [ ] `test/config_roundtrip` — CONF-01 fidelity (load example.cfg, mutate, save, re-parse, assert no value loss) — **highest priority, de-risks the persistence path**
- [ ] `test/KeyNameMapperTest` — BTN-02 Qt→evdev mapping
- [ ] `test/ButtonsModelTest` — BTN-01/03/04 with a mockable proxy seam
- [ ] `test/DeviceControllerTest` — DPI/SmartShift/Hires/Thumbwheel bounds + capability gating (mocked introspection)
- [ ] Mockable D-Bus proxy seam (interface abstraction) so GUI controllers test without a live bus (Phase 2 precedent: drive via public methods)
- [ ] Manual on-hardware checklist doc (UI-01, live-apply, Save polkit) — required because true validation needs a real Logitech device

---

## Security Domain

`security_enforcement` not explicitly false → included.

### Applicable ASVS categories
| ASVS | Applies | Standard control |
|---|---|---|
| V2 Authentication | yes | polkit authorization on `Save()` already in place (`pizza.pixl.logiops.save-config`, fail-safe DENY). GUI must not attempt to bypass. [VERIFIED] |
| V4 Access Control | yes | D-Bus policy: send/receive granted to `logiops` group; `own` root-only (Phase 1). GUI runs as normal user. [VERIFIED] |
| V5 Input Validation | yes | Key names → evdev strings; daemon validates via `toKeyCode` and logs/skips invalid. DPI snapped daemon-side. Host arg parsed with `std::stoi` (GUI should pre-validate to avoid daemon exceptions). [VERIFIED] |
| V6 Cryptography | no | none in scope |

### Known threat patterns for this stack
| Pattern | STRIDE | Mitigation |
|---|---|---|
| GUI tricked into editing root config file directly | Elevation | Never write `/etc/logid.cfg`; only `Save()` (polkit). [locked] |
| Malicious key-name / format string to daemon | Tampering | `SetKeys` takes typed `as`; daemon's logger already "%s"-hardened (CONCERNS #5 fixed Phase 1). [VERIFIED] |
| `std::stoi` on host string throws → unhandled | DoS (local) | GUI validates host is "1".."N"/"next"/"prev" before SetHost. [VERIFIED: setHost uses stoi] |
| Save during hotplug stalls daemon (server_lock held) | DoS (local, accepted-deferred) | Documented in daemon; GUI calls async, shows BusyIndicator; do not retry-storm. [VERIFIED] |

---

## Assumptions Log

| # | Claim | Section | Risk if wrong |
|---|---|---|---|
| A1 | DPI-02/DPI-03 "labeled cycle preset list at device/Pointer-tab scope" needs either a daemon schema addition (labels field) or a GUI-side label store; daemon has no label field today | D-Bus audit / Don't Hand-Roll | Wrong → plan under-scopes a daemon change; user may want labels persisted in the cfg vs GUI-local. **Confirm with user.** |
| A2 | Thumbwheel tap simple-action assignment may need a small daemon IPC method (tap currently routed via gesture machinery) | D-Bus audit gap #2 | Wrong → SCR-03 tap mapping either already works or needs a daemon task; verify `ThumbWheel.cpp:setTap` during planning |
| A3 | Host count (HOST-01 slot rendering) is not exposed over D-Bus; default to 3 or add a tiny read | Capability discovery | Wrong → UI shows wrong number of host slots; low impact, easily corrected |
| A4 | Capability gating via Introspect (or error-on-call) is acceptable; no per-config "changed" signals exist so GUI holds optimistic state between Saves | Capability / QML patterns | Wrong → if the daemon gains config-changed signals later, the optimistic model could drift; acceptable for this phase |
| A5 | Comment/formatting loss on Save is acceptable (CONTEXT says so) but unsupported hand-added keys are silently dropped — recommend a user-facing note | save() fidelity | Wrong → power users surprised by lost custom keys; mitigated by warning copy + round-trip test |

---

## Open Questions

1. **DPI cycle list + labels home (DPI-02/DPI-03).**
   - Known: daemon's only cycle list is `config::CycleDPI` on a *button action*; no labels anywhere.
   - Unclear: does the user want a standalone Pointer-tab cycle editor persisted in the cfg (needs daemon schema + IPC additions) or a GUI-local label overlay over `CycleDPI.SetDPIs`?
   - Recommendation: surface in discuss/plan; default to minimal (GUI-local labels + `CycleDPI` for the list) unless user wants cfg persistence.

2. **Thumbwheel tap/left/right simple actions vs gestures (SCR-03).**
   - Known: `SetLeft/SetRight/SetTap` build gestures; schema `tap` is a `BasicAction`.
   - Unclear: whether the existing IPC already exposes a simple-action interface at `…/thumb_wheel/tap`.
   - Recommendation: read `ThumbWheel.cpp` setTap fully in plan; add a small IPC method only if needed.

3. **Per-model SVG artwork source (BTN-01/UI-01).**
   - Known: fallback generic diagram is in scope; per-model SVG + hotspot data format is Claude's discretion.
   - Unclear: where artwork comes from and the hotspot↔CID mapping format.
   - Recommendation: define a JSON sidecar (per-PID → SVG + hotspot[{cid, x, y, w, h}]); ship generic fallback first, a couple of models as proof.

---

## Environment Availability

| Dependency | Required by | Available | Version | Fallback |
|---|---|---|---|---|
| Qt6 (Core/DBus/Gui/Quick/Qml/Svg) | entire GUI | ✓ | 6.4.2 | — |
| qdbusxml2cpp | typed proxies | ✓ | /usr/lib/qt6/bin | hand-write proxies (worse) |
| Quick Controls 2 runtime modules | TabBar/Slider/Dialog/Switch | assume ✓ (Phase 2 prereq) | 6.4.2 | install `qml6-module-qtquick-controls2` |
| busctl | dev-time introspection | ✓ | — | `gdbus introspect` |
| A real Logitech HID++ device | on-hardware validation (UI-01, live-apply, Save) | unknown (operator) | — | none — manual validation requires hardware |
| running `logid` daemon + `logiops` group membership | live D-Bus | operator-provided | — | AccessDenied/DaemonDown states already handled (Phase 2) |

**Missing with no fallback:** real-hardware on-device validation is operator-dependent (same as Phase 2, which recorded hardware validation in commits).

---

## Sources

### Primary (HIGH — read this session)
- `src/logid/config/{schema.h,types.h,group.h,map.h,config.cpp}` — config model + serialization
- `src/logid/Configuration.{h,cpp}` — save() round-trip + polkit gate
- `src/logid/Device.{h,cpp}` — `.Device` IPC: GetProfiles/SetProfile/RemoveProfile/ClearProfile + properties/signals
- `src/logid/features/{DPI,SmartShift,HiresScroll,RemapButton,ThumbWheel}.{h,cpp}` — feature IPC write surfaces
- `src/logid/actions/{Action,KeypressAction,ChangeHostAction,ChangeDPI,CycleDPI,ChangeProfile}.{h,cpp}` — action interfaces + setters
- `src/logiops-gui/{DaemonConnection.h,DeviceModel.h,CMakeLists.txt,qml/DetailPane.qml,qml/Theme.qml,dbus/*.xml}` — shell to extend
- `logid.example.cfg` — on-disk config shape
- `.planning/phases/03-core-config-ui-persistence/{03-CONTEXT.md,03-UI-SPEC.md}`, `.planning/REQUIREMENTS.md`, `.planning/STATE.md`, codebase maps

### Secondary (MEDIUM)
- Qt 6 Quick Controls 2 docs (TabBar/Slider/Dialog/Switch patterns) — CITED, standard usage
- libconfig writeFile comment/formatting behavior — CITED, standard

### Tertiary (LOW)
- None relied upon; web verification unavailable this session (`brave_search`/`exa`/`firecrawl` all false in config) — all load-bearing claims verified against in-tree source instead.

---

## Metadata

**Confidence breakdown:**
- D-Bus write surface: HIGH — every interface/method read directly from feature `.cpp` IPC ctors
- save() fidelity: HIGH — mechanism traced load→edit→save in source; round-trip test recommended to confirm empirically
- Capability discovery: HIGH for present-interface model; MEDIUM on best introspection technique (Introspect vs error-on-call)
- Qt/QML patterns: HIGH for the existing-shell facts; MEDIUM for Controls 2 recommendations (standard but not bench-verified this session)
- DPI labels / thumbwheel tap / host count: MEDIUM — real gaps flagged as Assumptions for user/plan confirmation

**Research date:** 2026-05-30
**Valid until:** ~2026-06-29 (stable; the daemon D-Bus surface is in-tree and unlikely to shift without a phase touching it)
