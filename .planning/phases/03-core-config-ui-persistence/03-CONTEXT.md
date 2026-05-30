# Phase 3: Core Config UI & Persistence - Context

**Gathered:** 2026-05-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Deliver the core configuration experience: a faithful Options+-style, tabbed GUI (built on the Phase 2 Qt 6 + QML shell) through which the user fully configures a device's **buttons, pointer (DPI), scroll (SmartShift / hi-res / thumbwheel), and manual profiles**, applies changes live, and persists them via the daemon's polkit-gated `Save()` — never editing `/etc/logid.cfg` by hand. Covers BTN-01..04, DPI-01..03, SCR-01..03, HOST-01, PROF-01, CONF-01, CONF-02, UI-01.

In scope: per-model/clickable device render with button reassignment, key-capture + built-in action picker, DPI slider + cycle editor, SmartShift/hi-res/thumbwheel controls, manual profile create/name/switch, live-apply + Save-to-disk persistence, per-device restore-to-defaults, and the polished tabbed Options+ interface.

Out of scope: rich/fine-grained gestures (Phase 4), per-application auto profile switching and profile file sharing (Phase 5), action wheel (Phase 6), macros/smart actions (Phase 7), keyboard backlight (Phase 8), packaging (Phase 9).
</domain>

<decisions>
## Implementation Decisions

### Device Render & Button Mapping (UI-01, BTN-01, BTN-04)
- Device visual: per-model vector/SVG render with clickable button hotspots; fall back to a generic annotated mouse/keyboard diagram when no model-specific artwork exists.
- Current bindings (BTN-04) shown two ways simultaneously: callout labels around the device render AND a synced list view of button→binding.
- Click interaction: clicking a button hotspot opens a non-modal side/detail panel (the device render stays visible) — not a blocking modal.
- Unsupported / non-remappable buttons are shown but visually disabled, with a tooltip explaining why.

### Action Assignment (BTN-02, BTN-03, HOST-01)
- Keystroke assignment (BTN-02): live key-capture widget ("press keys now") plus a manual modifier+key editor fallback.
- Built-in action picker (BTN-03): categorized list — Keystroke, DPI (change / cycle), Host switch, Profile switch, SmartShift toggle, Hi-res toggle, None/disabled.
- Rich gestures are deferred to Phase 4. This phase exposes only the simple action types listed in the success criteria. "Disabled" maps to the daemon's NullAction.
- Host switch (HOST-01): numbered host slots (1/2/3, matching the daemon's index-based ChangeHost) plus a next/prev cycle option.

### Pointer & Scroll Controls (DPI-01..03, SCR-01..03)
- DPI: a sensitivity slider bounded by the device's reported min/max (DPI-01) PLUS a separate DPI-cycle list editor with add/remove and per-preset labels (DPI-02 / DPI-03).
- SmartShift (SCR-01): on/off toggle + threshold slider + torque slider; the whole section is capability-gated (only shown when the device supports SmartShift).
- Hi-res scroll (SCR-02): toggle + invert-direction checkbox; capability-gated.
- Thumbwheel (SCR-03): capability-gated section exposing divert/invert plus left / right / tap action mapping, only on supported devices.

### Profiles, Persistence & Restore (PROF-01, CONF-01, CONF-02)
- Apply model: every change applies live instantly (daemon remains the single source of truth); an explicit **Save** action persists to disk; the UI shows an unsaved-changes indicator.
- Persistence path: the GUI calls the daemon's existing polkit-gated `Save()` method over D-Bus (libconfig serialize). The GUI NEVER writes `/etc/logid.cfg` directly. Round-trip fidelity of `Configuration::save()` (comment/formatting preservation) must be verified in research/plan — research flag from ROADMAP.
- Profile scope: manual profiles only this phase — create / name / switch (PROF-01). Per-application auto-switching and profile file sharing are Phase 5.
- Restore defaults (CONF-02): per-device "Restore defaults" action with a confirmation dialog; resets that device's configuration section.

### Claude's Discretion
- Exact QML component breakdown, tab structure, the live-edit binding mechanism (how config schema nodes are mutated over D-Bus vs. a GUI-side staging model before Save), model role names, key-capture implementation specifics, and the SVG hotspot data format are at Claude's discretion — guided by the UI-SPEC and existing logiops feature/ipcgull/QML conventions.
</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- Phase 2 GUI shell under `src/logiops-gui/`: `DaemonConnection`, `DeviceModel` (QAbstractListModel), `Main.qml` / `DetailPane.qml` / `DeviceList.qml` / `DeviceDelegate.qml` / `Theme.qml`, state screens (Empty/Loading/AccessDenied/DaemonDown), generated D-Bus proxies from `dbus/logid_device.xml` + `dbus/logid_devices.xml`. The detail pane currently shows read-only status — Phase 3 extends it into the tabbed config UI.
- Daemon persistence: `Configuration::save()` exists and is exposed over D-Bus as the `Save` method, polkit-gated on action `pizza.pixl.logiops.save-config` (delivered in Phase 1). The GUI consumes this directly.
- Daemon config schema (`src/logid/config/schema.h`, `types.h`) is the typed, ipcgull-exposed config tree — buttons, dpi, smartshift, hiresscroll, thumbwheel, profiles live here and are editable live over D-Bus.
- D-Bus identity constants in `src/logid/ipc_defs.h` (reused by the GUI proxy).

### Established Patterns
- C++20 + CMake; the GUI is a CMake target. QML renders only — no business logic in QML/JS; C++ model classes bridge daemon state (per Phase 2 decision).
- Capability gating: HID++ features are present-or-absent (UnsupportedFeature). The GUI must reflect what each device actually supports, mirroring how the daemon skips absent features.
- Follow `.planning/codebase/CONVENTIONS.md` for C++ style and `.editorconfig` for formatting.

### Integration Points
- All config reads/writes flow through the daemon over D-Bus — live property edits on the schema objects, then `Save()` to persist. No direct hidraw/file access from the GUI.
- Restore-to-defaults and profile switching must round-trip through the daemon so the live device state and the GUI stay in sync (CONF-03, already satisfied by the signal-driven model).
</code_context>

<specifics>
## Specific Ideas

- Research flag (load-bearing): verify `Configuration::save()` round-trip fidelity — that a load → mutate-over-D-Bus → save cycle preserves the config file faithfully (comments/formatting acceptable to lose, but structure/values must survive) — before relying on it as the only persistence path. If fidelity is inadequate, surface it in PLAN rather than shipping silent data loss.
- Confirm the daemon actually exposes writable D-Bus properties/methods for every config node this UI edits (buttons, DPI cycle, SmartShift threshold/torque, hi-res invert, thumbwheel, profiles). If a needed write surface is missing on the ipcgull config tree, note a small daemon addition in PLAN rather than working around it.
- "Faithful Options+-style" (UI-01) means tabbed layout, polished, animated — the UI-SPEC will define the visual contract.
</specifics>

<deferred>
## Deferred Ideas

- Fine-grained / repeat-control gestures → Phase 4.
- Per-application automatic profile switching + profile file sharing/export → Phase 5.
- Action wheel (radial menu) → Phase 6.
- Macros / smart multi-step actions → Phase 7.
- Keyboard backlight / RGB → Phase 8.
</deferred>
